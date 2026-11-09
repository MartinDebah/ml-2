#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <limits>
#include <vector>

//==============================================================================
// ML-2: глітч-машина. Записує вхід у буфер історії й на межах шістнадцятих
// (за тактами DAW) зриває звук: повтор, реверс, перемішування, стоп стрічки, хруст, гейт.
class GlitchProcessor : public juce::AudioProcessor
{
public:
    enum Fx { Rep = 0, Rev, Shf, Stp, Cru, Gat, NumFx };
    static constexpr int kSteps = 32;           // два такти шістнадцятими
    static constexpr int kPeaksPerStep = 16;

    GlitchProcessor();
    ~GlitchProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return juce::String::fromUTF8 ("Мартин лох ML-2"); }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override    { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Ручний зрив із пада: спрацює на наступній шістнадцятій
    void triggerPad (int fx, int sizeIndex) { forcedSize.store (sizeIndex); forcedFx.store (fx); }

    // Для інтерфейсу
    std::array<std::atomic<int>, kSteps> uiHist;                        // 0 — нема, 1..6 — ефект, +16 — перший крок
    std::array<std::atomic<float>, kSteps * kPeaksPerStep> uiPeaks;
    std::atomic<double> uiPos { 0.0 };                                  // позиція в шістнадцятих, 0..32
    std::atomic<float> uiBpm { 120.0f };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static const juce::StringArray lenNames, sizeNames, fxIds;
    static constexpr std::array<int, 5> lenSteps { 1, 2, 4, 8, 16 };
    static constexpr std::array<int, 4> sizeDiv  { 1, 2, 4, 8 };

    juce::AudioProcessorValueTreeState apvts;

private:
    struct Event
    {
        int fx = -1;
        juce::int64 start = 0;      // абсолютний номер семпла початку
        int length = 0, elapsed = 0;
        int sizeIdx = 1, dir = 1;
        // повтор
        double repPos = 0.0, curD = 0.0, chunk = 0.0;
        int repIdx = 0, repTotal = 1;
        // стоп стрічки
        double readPos = 0.0;
        // перемішування
        std::array<int, 16> shufBack {};
    };

    struct Decision { int fx = -1, len = 1, sizeIdx = 1, dir = 1; std::array<int, 16> shufBack {}; };

    Decision makeDecision (int fx, int sizeIdx);
    void startEvent (const Decision&, int stepIdx32);
    float readHist (int ch, double absPos) const;
    float processEvent (int ch, float in);

    std::atomic<float>* pChance = nullptr;
    std::atomic<float>* pLen    = nullptr;
    std::atomic<float>* pSize   = nullptr;
    std::atomic<float>* pRamp   = nullptr;
    std::atomic<float>* pPitch  = nullptr;
    std::atomic<float>* pCrush  = nullptr;
    std::atomic<float>* pVol    = nullptr;
    std::atomic<float>* pLock   = nullptr;
    std::array<std::atomic<float>*, NumFx> pFx {};

    std::atomic<int> forcedFx { -1 }, forcedSize { 1 };

    double sr = 44100.0, stepSamples = 5000.0, freePpq = 0.0;
    std::vector<float> hist[2];
    int histLen = 1;
    juce::int64 absPos = 0, lastStep = std::numeric_limits<juce::int64>::min();

    Event ev;
    std::array<Decision, kSteps> cache {};
    std::array<bool, kSteps> cacheValid {};

    float holdVal[2] { 0, 0 };
    int holdCount = 0;
    float gateGain = 1.0f;
    float peakAcc = 0.0f;
    int lastPeakSlot = -1;

    juce::Random rng;
    juce::LinearSmoothedValue<float> volS;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchProcessor)
};
