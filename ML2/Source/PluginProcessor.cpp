#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::String u (const char* utf8) { return juce::String::fromUTF8 (utf8); }

const juce::StringArray GlitchProcessor::lenNames  { "1/16", "1/8", "1/4", "1/2", juce::String::fromUTF8 ("1 такт") };
const juce::StringArray GlitchProcessor::sizeNames { "1/16", "1/32", "1/64", "1/128" };
const juce::StringArray GlitchProcessor::fxIds     { "fx_rep", "fx_rev", "fx_shf", "fx_stp", "fx_cru", "fx_gat" };

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout GlitchProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto pctStr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });
    const NormalisableRange<float> pct (0.0f, 100.0f);

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "chance", 1 }, u ("Шанс"), pct, 30.0f, pctStr));
    layout.add (std::make_unique<AudioParameterChoice>(ParameterID { "len", 1 }, u ("Довжина"), lenNames, 2));
    layout.add (std::make_unique<AudioParameterChoice>(ParameterID { "size", 1 }, u ("Дроблення"), sizeNames, 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "ramp", 1 }, u ("Прискорення"), pct, 35.0f, pctStr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "pitch", 1 }, u ("Висота"), NormalisableRange<float> (0.0f, 24.0f, 1.0f), 7.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + u (" пт"); })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "crush", 1 }, u ("Хруст"), pct, 60.0f, pctStr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vol", 1 }, u ("Гучність"), pct, 80.0f, pctStr));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "lock", 1 }, u ("Замок"), false));

    const char* fxNames[] = { "Повтор", "Реверс", "Перемішати", "Стоп стрічки", "Хруст (ефект)", "Гейт" };
    for (int i = 0; i < NumFx; ++i)
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { fxIds[i], 1 }, u (fxNames[i]), i != Gat));

    return layout;
}

GlitchProcessor::GlitchProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pChance = apvts.getRawParameterValue ("chance");
    pLen    = apvts.getRawParameterValue ("len");
    pSize   = apvts.getRawParameterValue ("size");
    pRamp   = apvts.getRawParameterValue ("ramp");
    pPitch  = apvts.getRawParameterValue ("pitch");
    pCrush  = apvts.getRawParameterValue ("crush");
    pVol    = apvts.getRawParameterValue ("vol");
    pLock   = apvts.getRawParameterValue ("lock");
    for (int i = 0; i < NumFx; ++i)
        pFx[(size_t) i] = apvts.getRawParameterValue (fxIds[i]);

    for (auto& h : uiHist)  h.store (0);
    for (auto& p : uiPeaks) p.store (0.0f);
}

bool GlitchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const auto ok  = [] (const juce::AudioChannelSet& s)
    { return s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo(); };
    return ok (in) && ok (out) && in.size() <= out.size();
}

void GlitchProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    histLen = (int) (sr * 8.0);
    for (auto& h : hist) h.assign ((size_t) histLen, 0.0f);
    absPos = 0;
    lastStep = std::numeric_limits<juce::int64>::min();
    ev = {};
    cacheValid.fill (false);
    volS.reset (sr, 0.03);
    gateGain = 1.0f;
}

//==============================================================================
float GlitchProcessor::readHist (int ch, double pos) const
{
    const double fl = std::floor (pos);
    const float t = (float) (pos - fl);
    auto idx = [this] (juce::int64 i) { return (size_t) (((i % histLen) + histLen) % histLen); };
    const auto i0 = (juce::int64) fl;
    const float a = hist[ch][idx (i0)], b = hist[ch][idx (i0 + 1)];
    return a + (b - a) * t;
}

GlitchProcessor::Decision GlitchProcessor::makeDecision (int fx, int sizeIdx)
{
    Decision d;
    d.fx = fx;
    const int maxL = lenSteps[(size_t) juce::jlimit (0, 4, (int) pLen->load())];
    int opts[5], n = 0;
    for (int l : lenSteps) if (l <= maxL) opts[n++] = l;
    d.len = opts[rng.nextInt (n)];
    d.sizeIdx = sizeIdx;
    d.dir = rng.nextBool() ? 1 : -1;
    for (auto& k : d.shufBack) k = 1 + rng.nextInt (16);
    return d;
}

void GlitchProcessor::startEvent (const Decision& d, int idx32)
{
    const int len = juce::jmin (d.len, kSteps - idx32);
    ev = {};
    ev.fx = d.fx;
    ev.start = absPos;
    ev.length = juce::jmax (1, (int) (len * stepSamples));
    ev.sizeIdx = d.sizeIdx;
    ev.dir = d.dir;
    ev.shufBack = d.shufBack;
    ev.chunk = stepSamples / sizeDiv[(size_t) juce::jlimit (0, 3, d.sizeIdx)];
    ev.curD = ev.chunk;
    ev.repTotal = juce::jmax (1, juce::roundToInt (ev.length / ev.chunk));
    ev.readPos = (double) absPos;
    holdCount = 0;

    for (int k = 0; k < len; ++k)
        uiHist[(size_t) ((idx32 + k) % kSteps)].store (d.fx + 1 + (k == 0 ? 16 : 0));
}

//==============================================================================
float GlitchProcessor::processEvent (int ch, float in)
{
    const int e = ev.elapsed;
    float out = in;

    switch (ev.fx)
    {
        case Rep:
        {
            const double pitch = pPitch->load();
            const double rate = std::pow (2.0, ev.dir * pitch * ((double) ev.repIdx / ev.repTotal) / 12.0);
            const double off = ev.repIdx == 0 ? ev.repPos : std::fmod (ev.repPos * rate, ev.chunk);
            out = readHist (ch, (double) ev.start + off) * (1.0f - juce::jmin (0.5f, ev.repIdx * 0.015f));
            break;
        }
        case Rev: out = readHist (ch, (double) (ev.start - 1 - e)); break;
        case Shf:
        {
            const int j = (int) (e / stepSamples);
            out = readHist (ch, (double) absPos - ev.shufBack[(size_t) (j % 16)] * stepSamples);
            break;
        }
        case Stp: out = readHist (ch, ev.readPos); break;
        case Cru:
        {
            const float c = pCrush->load() * 0.01f;
            if (holdCount == 0) holdVal[ch] = in;
            const float lev = std::pow (2.0f, 8.0f - c * 6.2f);
            out = std::round (std::tanh (holdVal[ch] * (1.0f + c * 2.0f)) * lev) / lev * 0.9f;
            break;
        }
        case Gat: out = in * gateGain; break;
        default: break;
    }

    // м'які краї зриву, 3 мс
    const float f = (float) (0.003 * sr);
    const float g = juce::jlimit (0.0f, 1.0f, juce::jmin ((float) e / f, (float) (ev.length - e) / f));
    return in * (1.0f - g) + out * g;
}

void GlitchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n      = buffer.getNumSamples();
    if (n == 0 || numIn == 0) return;

    // Темп і позиція з DAW; якщо DAW стоїть — власний годинник
    double bpm = 120.0, ppq0 = freePpq;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) if (*b > 1.0) bpm = *b;
            playing = pos->getIsPlaying();
            if (playing) if (auto p = pos->getPpqPosition()) ppq0 = *p;
        }
    uiBpm.store ((float) bpm);
    stepSamples = sr * 60.0 / bpm / 4.0;
    const double ppqPerSample = bpm / 60.0 / sr;

    volS.setTargetValue (std::pow (pVol->load() / 80.0f, 1.5f));
    const float ramp  = pRamp->load() * 0.01f;
    const int   holdN = 1 + (int) (pCrush->load() * 0.15f);
    const bool  lock  = pLock->load() > 0.5f;

    float* L = buffer.getWritePointer (0);
    float* R = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : L;

    for (int i = 0; i < n; ++i)
    {
        const double ppq = ppq0 + i * ppqPerSample;
        const juce::int64 step = (juce::int64) std::floor (ppq * 4.0);
        const int idx32 = (int) (((step % kSteps) + kSteps) % kSteps);

        // --- межа шістнадцятої
        if (step != lastStep)
        {
            lastStep = step;
            if (ev.fx < 0)
            {
                Decision d;
                const int forced = forcedFx.exchange (-1);
                if (forced >= 0)
                {
                    d = makeDecision (forced, forcedSize.load());
                    cache[(size_t) idx32] = d; cacheValid[(size_t) idx32] = true;
                }
                else if (lock && cacheValid[(size_t) idx32])
                    d = cache[(size_t) idx32];
                else
                {
                    int on[NumFx], cnt = 0;
                    for (int k = 0; k < NumFx; ++k) if (pFx[(size_t) k]->load() > 0.5f) on[cnt++] = k;
                    if (cnt > 0 && rng.nextFloat() < pChance->load() * 0.01f * 0.4f)
                        d = makeDecision (on[rng.nextInt (cnt)], juce::jlimit (0, 3, (int) pSize->load()));
                    cache[(size_t) idx32] = d; cacheValid[(size_t) idx32] = true;
                }
                if (d.fx >= 0) startEvent (d, idx32);
                else uiHist[(size_t) idx32].store (0);
            }
        }

        // --- запис історії
        const float xl = L[i], xr = inR[i];
        const size_t w = (size_t) (absPos % histLen);
        hist[0][w] = xl;
        hist[1][w] = xr;

        float yl = xl, yr = xr;
        if (ev.fx >= 0)
        {
            if (ev.fx == Gat)
            {
                const float on = std::fmod ((double) ev.elapsed, ev.chunk) < ev.chunk * 0.5 ? 1.0f : 0.0f;
                gateGain += (on - gateGain) * 0.08f;
            }
            yl = processEvent (0, xl);
            yr = processEvent (1, xr);

            // крок події
            if (ev.fx == Rep)
            {
                ev.repPos += 1.0;
                if (ev.repPos >= ev.curD)
                {
                    ev.repPos = 0.0;
                    ++ev.repIdx;
                    ev.curD = juce::jmax (0.006 * sr, ev.curD * (1.0 - ramp * 0.16));
                }
            }
            else if (ev.fx == Stp)
                ev.readPos += std::exp (std::log (0.05) * (double) ev.elapsed / ev.length);
            else if (ev.fx == Cru && ++holdCount >= holdN)
                holdCount = 0;

            if (++ev.elapsed >= ev.length) { ev.fx = -1; gateGain = 1.0f; }
        }

        const float v = volS.getNextValue();
        yl *= v; yr *= v;
        if (R != nullptr) { L[i] = yl; R[i] = yr; }
        else              { L[i] = 0.5f * (yl + yr); }

        // --- пік для хвилі на дисплеї
        peakAcc = juce::jmax (peakAcc, std::abs (yl), std::abs (yr));
        const double sub = ppq * 4.0 - std::floor (ppq * 4.0);
        const int slot = idx32 * kPeaksPerStep + juce::jlimit (0, kPeaksPerStep - 1, (int) (sub * kPeaksPerStep));
        if (slot != lastPeakSlot)
        {
            if (lastPeakSlot >= 0) uiPeaks[(size_t) lastPeakSlot].store (peakAcc);
            peakAcc = 0.0f;
            lastPeakSlot = slot;
        }

        ++absPos;
    }

    for (int ch = 2; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    const double ppqEnd = ppq0 + n * ppqPerSample;
    freePpq = playing ? ppqEnd : freePpq + n * ppqPerSample;
    const double pos16 = std::fmod (ppqEnd * 4.0, (double) kSteps);
    uiPos.store (pos16 < 0 ? pos16 + kSteps : pos16);
}

//==============================================================================
void GlitchProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void GlitchProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* GlitchProcessor::createEditor() { return new GlitchEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GlitchProcessor(); }
