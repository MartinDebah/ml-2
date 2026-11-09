#include "PluginEditor.h"

using namespace juce;

static String u (const char* utf8) { return String::fromUTF8 (utf8); }

static const char* fxGlyph[] = { "П", "Р", "М", "С", "Х", "Г" };
static const char* fxName[]  = { "ПОВТОР", "РЕВЕРС", "ПЕРЕМІШАТИ", "СТОП СТРІЧКИ", "ХРУСТ", "ГЕЙТ" };

struct PadDef { int fx, size; const char* glyph; const char* label; };
static const PadDef padDefs[] = {
    { GlitchProcessor::Rep, 1, "П", "Повтор 1/32" },
    { GlitchProcessor::Rep, 3, "П", "Повтор 1/128" },
    { GlitchProcessor::Rev, 1, "Р", "Реверс" },
    { GlitchProcessor::Shf, 1, "М", "Перемішати" },
    { GlitchProcessor::Stp, 1, "С", "Стоп стрічки" },
    { GlitchProcessor::Cru, 1, "Х", "Хруст" },
};

//==============================================================================
GlitchLookAndFeel::GlitchLookAndFeel()
{
    setColour (Slider::textBoxTextColourId, Pal::muted);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Label::textColourId, Pal::ink);
}

void GlitchLookAndFeel::drawPad (Graphics& g, Rectangle<float> r, Colour base, bool pressed)
{
    const float edge = pressed ? 1.0f : 3.0f;
    auto body = r.withTrimmedBottom (3.0f).translated (0.0f, pressed ? 2.0f : 0.0f);
    if (! pressed)
    {
        Path sh; sh.addRoundedRectangle (body.translated (0, 2), 7.0f);
        DropShadow (Colours::black.withAlpha (0.4f), 7, { 0, 3 }).drawForPath (g, sh);
    }
    g.setColour (Pal::padEdge);
    g.fillRoundedRectangle (body.translated (0.0f, edge), 7.0f);
    g.setGradientFill (ColourGradient (base.brighter (0.18f), body.getX(), body.getY(), base.darker (0.25f), body.getX(), body.getBottom(), false));
    g.fillRoundedRectangle (body, 7.0f);
    g.setColour (Pal::padEdge);
    g.drawRoundedRectangle (body.reduced (0.5f), 7.0f, 1.0f);
    if (pressed)
    {
        g.setColour (Colours::black.withAlpha (0.25f));
        g.drawRoundedRectangle (body.reduced (1.5f), 6.0f, 2.0f);
    }
    else
    {
        g.setColour (Colours::white.withAlpha (0.18f));
        g.drawHorizontalLine ((int) body.getY() + 1, body.getX() + 6.0f, body.getRight() - 6.0f);
    }
}

void GlitchLookAndFeel::drawLed (Graphics& g, Rectangle<float> r, bool on, Colour onColour)
{
    if (on)
    {
        g.setColour (onColour.withAlpha (0.35f));
        g.fillEllipse (r.expanded (3.0f));
        g.setColour (onColour);
    }
    else
        g.setColour (Colour (0xff3a1212));
    g.fillEllipse (r);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawEllipse (r, 0.8f);
}

void GlitchLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                          float a0, float a1, Slider& s)
{
    auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = a0 + pos * (a1 - a0);

    // шкала: для перемикачів — по кроках, для ручок — 0..10
    const auto range = s.getRange();
    const int ticks = s.getInterval() >= 1.0 && range.getLength() <= 8.0 ? (int) range.getLength() : 10;
    g.setColour (Pal::muted);
    for (int i = 0; i <= ticks; ++i)
    {
        const float a = a0 + (a1 - a0) * (float) i / (float) ticks;
        const float r0 = (ticks == 10 && i % 5 != 0) ? r * 0.88f : r * 0.83f;
        g.drawLine ({ c.getPointOnCircumference (r0, a), c.getPointOnCircumference (r * 0.97f, a) }, 1.2f);
    }

    const PathStrokeType stroke (3.5f, PathStrokeType::curved, PathStrokeType::rounded);
    const float ar = r * 0.72f;
    Path track;
    track.addCentredArc (c.x, c.y, ar, ar, 0.0f, a0, a1, true);
    g.setColour (Pal::line);
    g.strokePath (track, stroke);
    if (pos > 0.002f)
    {
        Path val;
        val.addCentredArc (c.x, c.y, ar, ar, 0.0f, a0, ang, true);
        g.setColour (Pal::accent);
        g.strokePath (val, stroke);
    }

    const float capR = r * 0.52f;
    Path capP; capP.addEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2);
    DropShadow (Colours::black.withAlpha (0.55f), 6, { 1, 3 }).drawForPath (g, capP);
    g.setColour (Pal::cap);
    g.fillPath (capP);
    ColourGradient hl (Colours::white.withAlpha (0.32f), c.x - capR * 0.3f, c.y - capR * 0.45f,
                       Colours::black.withAlpha (0.25f), c.x + capR, c.y + capR, true);
    hl.addColour (0.55, Colours::white.withAlpha (0.04f));
    g.setGradientFill (hl);
    g.fillPath (capP);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawEllipse (c.x - capR * 0.75f, c.y - capR * 0.75f, capR * 1.5f, capR * 1.5f, 1.0f);

    g.setColour (Pal::ptr);
    g.drawLine ({ c.getPointOnCircumference (capR * 0.3f, ang), c.getPointOnCircumference (capR * 0.85f, ang) }, 2.5f);
}

void GlitchLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool hover, bool down)
{
    const bool on = b.getToggleState();
    auto r = b.getLocalBounds().toFloat();
    drawPad (g, r, hover ? Pal::pad.brighter (0.08f) : Pal::pad, on || down);
    auto body = r.withTrimmedBottom (3.0f).translated (0.0f, (on || down) ? 2.0f : 0.0f);
    drawLed (g, body.removeFromLeft (body.getHeight()).withSizeKeepingCentre (9.0f, 9.0f), on, Pal::sig);
    g.setColour (Pal::padInk);
    g.setFont (FontOptions (12.0f, Font::bold));
    g.drawText (b.getButtonText(), body, Justification::centredLeft);
}

//==============================================================================
void GlitchPad::paintButton (Graphics& g, bool hover, bool down)
{
    auto r = getLocalBounds().toFloat();
    const bool lit = down || flash > 0.0f;
    const Colour base = lit ? Pal::accent : (hover ? Pal::pad.brighter (0.08f) : Pal::pad);
    GlitchLookAndFeel::drawPad (g, r, base, lit);
    auto body = r.withTrimmedBottom (3.0f).translated (0.0f, lit ? 2.0f : 0.0f).reduced (12.0f, 8.0f);
    g.setColour (lit ? Pal::onAcc : Pal::hot);
    g.setFont (FontOptions (30.0f, Font::bold));
    g.drawText (glyph, body.removeFromTop (34.0f), Justification::topLeft);
    g.setColour (lit ? Pal::onAcc : Pal::padInk);
    g.setFont (FontOptions (12.0f, Font::bold));
    g.drawText (getButtonText(), body, Justification::bottomLeft);
}

//==============================================================================
GlitchEditor::GlitchEditor (GlitchProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    const char* ids[]   = { "chance", "len", "size", "ramp", "pitch", "crush", "vol" };
    const char* names[] = { "ШАНС", "ДОВЖИНА", "ДРОБЛЕННЯ", "ПРИСКОРЕННЯ", "ВИСОТА", "ХРУСТ", "ГУЧНІСТЬ" };
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setSliderStyle (Slider::RotaryVerticalDrag);
        k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 90, 16);
        k.slider.setMouseDragSensitivity (220);
        k.label.setText (u (names[i]), dontSendNotification);
        k.label.setJustificationType (Justification::centred);
        k.label.setFont (FontOptions (11.0f, Font::bold));
        addAndMakeVisible (k.slider);
        addAndMakeVisible (k.label);
        k.attach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], k.slider);
    }

    lockBtn.setButtonText (u ("ЗАМОК"));
    addAndMakeVisible (lockBtn);
    lockA = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "lock", lockBtn);

    for (int i = 0; i < GlitchProcessor::NumFx; ++i)
    {
        auto& b = fxBtns[(size_t) i];
        b.setButtonText (u (fxGlyph[i]) + "  " + u (fxName[i]));
        addAndMakeVisible (b);
        fxA[(size_t) i] = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, GlitchProcessor::fxIds[i], b);
    }

    for (auto& d : padDefs)
    {
        auto pad = std::make_unique<GlitchPad> (u (d.glyph), u (d.label));
        auto* raw = pad.get();
        const int fx = d.fx, size = d.size;
        pad->setTriggeredOnMouseDown (true);
        pad->onClick = [this, raw, fx, size] { proc.triggerPad (fx, size); raw->flash = 1.0f; raw->repaint(); };
        addAndMakeVisible (*pad);
        pads.push_back (std::move (pad));
    }

    makeWearTexture();
    setSize (1060, 660);
    startTimerHz (30);
}

GlitchEditor::~GlitchEditor()
{
    setLookAndFeel (nullptr);
}

void GlitchEditor::makeWearTexture()
{
    const int W = 640, H = 420;
    wear = Image (Image::ARGB, W, H, true);
    Random rng (2002);
    {
        Image::BitmapData bd (wear, Image::BitmapData::writeOnly);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const float v = rng.nextFloat();
                bd.setPixelColour (x, y, (v > 0.5f ? Colours::white : Colours::black).withAlpha ((uint8) (std::abs (v - 0.5f) * 26.0f)));
            }
    }
    Graphics g (wear);
    for (int i = 0; i < 26; ++i)
    {
        const float x = rng.nextFloat() * W, y = rng.nextFloat() * H, r = 15.0f + rng.nextFloat() * 70.0f;
        g.setGradientFill (ColourGradient (Colour (60, 50, 30).withAlpha (0.04f + rng.nextFloat() * 0.06f), x, y,
                                           Colour (60, 50, 30).withAlpha (0.0f), x + r, y, true));
        g.fillEllipse (x - r, y - r, r * 2, r * 2);
    }
    auto scratch = [&] (bool light)
    {
        float x = rng.nextFloat() * W, y = rng.nextFloat() * H, a = rng.nextFloat() * MathConstants<float>::twoPi;
        const float len = 8.0f + std::pow (rng.nextFloat(), 2.0f) * 110.0f;
        Path p; p.startNewSubPath (x, y);
        for (int k = 0; k < (int) (len / 6.0f) + 1; ++k)
        {
            a += (rng.nextFloat() - 0.5f) * 0.25f;
            x += std::cos (a) * 6.0f; y += std::sin (a) * 6.0f;
            p.lineTo (x, y);
        }
        g.setColour (light ? Colours::white.withAlpha (0.12f + rng.nextFloat() * 0.25f)
                           : Colours::black.withAlpha (0.08f + rng.nextFloat() * 0.14f));
        g.strokePath (p, PathStrokeType (0.4f + rng.nextFloat() * 0.9f, PathStrokeType::curved, PathStrokeType::rounded));
    };
    for (int i = 0; i < 150; ++i) scratch (true);
    for (int i = 0; i < 70; ++i)  scratch (false);
    for (int i = 0; i < 40; ++i)
    {
        g.setColour (Colours::white.withAlpha (0.15f + rng.nextFloat() * 0.25f));
        g.fillEllipse (rng.nextFloat() * W, rng.nextFloat() * H, 1.2f + rng.nextFloat() * 4.0f, 0.8f + rng.nextFloat() * 2.8f);
    }
}

//==============================================================================
void GlitchEditor::timerCallback()
{
    for (auto& p : pads)
        if (p->flash > 0.0f)
        {
            p->flash = jmax (0.0f, p->flash - 0.2f);
            p->repaint();
        }
    repaint (slicerArea);
}

void GlitchEditor::resized()
{
    auto r = getLocalBounds().reduced (30, 24);
    titleArea = r.removeFromTop (60);
    r.removeFromTop (10);
    slicerArea = r.removeFromTop (180);
    r.removeFromTop (12);
    padLabelArea = r.removeFromTop (18);
    r.removeFromTop (4);

    auto padRow = r.removeFromTop (84);
    const int pw = (padRow.getWidth() - 10 * 5) / 6;
    for (auto& p : pads)
    {
        p->setBounds (padRow.removeFromLeft (pw));
        padRow.removeFromLeft (10);
    }
    r.removeFromTop (18);

    const int gap = 16;
    const int units[3] = { 5, 3, 2 };
    const int unitW = (r.getWidth() - gap * 2) / 10;
    for (int s = 0; s < 3; ++s)
    {
        sections[(size_t) s] = r.removeFromLeft (unitW * units[s]);
        if (s < 2) r.removeFromLeft (gap);
    }

    // автогліч: перемикачі ліворуч, ручки праворуч
    auto a = sections[0].withTrimmedTop (30);
    auto toggles = a.removeFromLeft (300);
    std::vector<ToggleButton*> list { &lockBtn };
    for (auto& b : fxBtns) list.push_back (&b);
    for (size_t i = 0; i < list.size(); ++i)
    {
        const int col = (int) (i % 2), row = (int) (i / 2);
        list[i]->setBounds (toggles.getX() + col * 150, toggles.getY() + row * 36, 142, 32);
    }

    auto place = [] (Rectangle<int> area, Knob* first, int count)
    {
        const int w = area.getWidth() / count;
        for (int i = 0; i < count; ++i)
        {
            auto cell = area.removeFromLeft (w);
            first[i].label.setBounds (cell.removeFromTop (16));
            first[i].slider.setBounds (cell.removeFromTop (104).withSizeKeepingCentre (jmin (cell.getWidth(), 90), 104));
        }
    };
    place (a.withTrimmedLeft (10), &knobs[0], 2);
    place (sections[1].withTrimmedTop (30), &knobs[2], 3);
    place (sections[2].withTrimmedTop (30), &knobs[5], 2);
}

//==============================================================================
void GlitchEditor::paint (Graphics& g)
{
    g.fillAll (Pal::page);

    auto face = getLocalBounds().toFloat().reduced (8.0f);
    Path faceP; faceP.addRoundedRectangle (face, 12.0f);
    DropShadow (Colours::black.withAlpha (0.5f), 12, { 0, 3 }).drawForPath (g, faceP);
    g.setGradientFill (ColourGradient (Pal::panel.brighter (0.05f), 0, face.getY(), Pal::panel.darker (0.05f), 0, face.getBottom(), false));
    g.fillPath (faceP);
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (faceP);
        g.setTiledImageFill (wear, 0, 0, 1.0f);
        g.fillRect (face);
        for (int i = 0; i < 12; ++i)
        {
            g.setColour (Colour (50, 40, 20).withAlpha (0.035f));
            g.drawRoundedRectangle (face.reduced ((float) i * 1.6f), 12.0f, 1.6f);
        }
    }
    g.setColour (Pal::line);
    g.drawRoundedRectangle (face, 12.0f, 1.0f);

    for (auto pt : { face.getTopLeft() + Point<float> (12, 12), face.getTopRight() + Point<float> (-12, 12),
                     face.getBottomLeft() + Point<float> (12, -12), face.getBottomRight() + Point<float> (-12, -12) })
    {
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.8f), pt.x - 2, pt.y - 2, Pal::line.darker (0.4f), pt.x + 4, pt.y + 4, true));
        g.fillEllipse (pt.x - 4.5f, pt.y - 4.5f, 9.0f, 9.0f);
        g.setColour (Colours::black.withAlpha (0.45f));
        g.drawLine (pt.x - 3.0f, pt.y - 1.0f, pt.x + 3.0f, pt.y + 1.0f, 1.0f);
    }

    // назва
    g.setColour (Pal::ink);
    g.setFont (FontOptions (40.0f, Font::bold));
    const String title = u ("МАРТИН ЛОХ");
    g.drawText (title, titleArea.withHeight (44), Justification::centredLeft);
    const int tw = (int) GlyphArrangement::getStringWidth (g.getCurrentFont(), title) + 14;
    auto badge = Rectangle<float> ((float) titleArea.getX() + (float) tw, (float) titleArea.getY() + 12.0f, 40.0f, 20.0f);
    g.setColour (Pal::accent);
    g.fillRoundedRectangle (badge, 3.0f);
    g.setColour (Pal::onAcc);
    g.setFont (FontOptions (12.0f, Font::bold));
    g.drawText ("ML-2", badge, Justification::centred);
    g.setColour (Pal::muted);
    g.setFont (FontOptions (11.0f));
    g.drawText (u ("ГЛІТЧ-МАШИНА"), titleArea.withTrimmedTop (44), Justification::centredLeft);

    g.setColour (Pal::muted);
    g.setFont (FontOptions (11.0f, Font::bold));
    g.drawText (u ("РУЧНІ ЗРИВИ  ·  СПРАЦЮЮТЬ НА НАСТУПНІЙ ШІСТНАДЦЯТІЙ"), padLabelArea, Justification::centredLeft);

    const char* heads[] = { "АВТОГЛІТЧ", "ФОРМА ЗРИВУ", "ВИХІД" };
    for (size_t i = 0; i < sections.size(); ++i)
    {
        auto s = sections[i];
        g.setColour (Pal::accent);
        g.fillRect (s.getX(), s.getY(), s.getWidth(), 2);
        g.setColour (Pal::ink);
        g.setFont (FontOptions (14.0f, Font::bold));
        g.drawText (u (heads[i]), s.withHeight (24).withTrimmedTop (6), Justification::centredLeft);
        g.setColour (Pal::muted);
        g.setFont (FontOptions (11.0f));
        g.drawText ("0" + String ((int) i + 1), s.withHeight (24).withTrimmedTop (6), Justification::centredRight);
    }

    drawSlicer (g, slicerArea.toFloat());
}

void GlitchEditor::drawSlicer (Graphics& g, Rectangle<float> area)
{
    g.setColour (Pal::well);
    g.fillRoundedRectangle (area, 8.0f);
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.5f), 0, area.getY(), Colours::transparentBlack, 0, area.getY() + 10.0f, false));
    g.fillRoundedRectangle (area.withHeight (10.0f), 8.0f);
    g.setColour (Pal::line.darker (0.3f));
    g.drawRoundedRectangle (area, 8.0f, 1.0f);

    const int N = GlitchProcessor::kSteps, P = GlitchProcessor::kPeaksPerStep;
    const float pad = 14.0f, top = area.getY() + 26.0f, waveH = area.getHeight() - 26.0f - 50.0f;
    const float mid = top + waveH * 0.5f, x0 = area.getX() + pad, cw = (area.getWidth() - pad * 2) / (float) N;
    const double pos = proc.uiPos.load();
    const int ph = jlimit (0, N - 1, (int) pos);

    // зриви
    for (int i = 0; i < N; ++i)
    {
        const int h = proc.uiHist[(size_t) i].load();
        if (h == 0) continue;
        const float x = x0 + (float) i * cw;
        g.setColour (Pal::accent.withAlpha (0.35f));
        g.fillRect (x, top, cw, waveH);
        if (h >= 16)
        {
            g.setColour (Pal::hot);
            g.setFont (FontOptions (15.0f, Font::bold));
            g.drawText (u (fxGlyph[jlimit (0, 5, (h & 15) - 1)]), Rectangle<float> (x + 2.0f, top + 2.0f, cw, 16.0f), Justification::topLeft);
        }
    }

    // хвиля виходу
    const float bw = cw / (float) P;
    for (int i = 0; i < N; ++i)
    {
        const bool hot = proc.uiHist[(size_t) i].load() != 0;
        g.setColour (hot ? Pal::hot : (i == ph ? Pal::winInk : Pal::wave));
        for (int k = 0; k < P; ++k)
        {
            const float v = jmin (1.0f, proc.uiPeaks[(size_t) (i * P + k)].load());
            const float hh = jmax (1.0f, v * waveH * 0.48f);
            g.fillRect (x0 + (float) i * cw + (float) k * bw, mid - hh, jmax (1.0f, bw - 0.3f), hh * 2.0f);
        }
    }

    // сітка
    for (int i = 0; i <= N; ++i)
    {
        g.setColour (Pal::winLine.withAlpha (i % 4 == 0 ? 1.0f : 0.5f));
        g.drawVerticalLine ((int) (x0 + (float) i * cw), top, top + waveH);
    }

    // лампи кроків
    const float ly = area.getBottom() - 38.0f;
    for (int i = 0; i < N; ++i)
    {
        const bool on = proc.uiHist[(size_t) i].load() != 0;
        auto cell = Rectangle<float> (x0 + (float) i * cw + 1.0f, ly, cw - 2.0f, 10.0f);
        g.setColour (i == ph ? Pal::winInk : (on ? Pal::accent : Pal::pad));
        g.fillRect (cell);
    }

    // курсор
    const float px = x0 + (float) (pos / N) * (area.getWidth() - pad * 2);
    g.setColour (Pal::winInk);
    g.fillRect (px - 1.0f, top - 4.0f, 2.0f, waveH + 8.0f);

    // підписи
    g.setFont (FontOptions (10.0f, Font::bold));
    g.setColour (Pal::winDim);
    g.drawText (u ("ВХІД ТРЕКУ  ·  2 ТАКТИ"), Rectangle<float> (x0, area.getY() + 6.0f, 300.0f, 14.0f), Justification::centredLeft);
    String right = String (proc.uiBpm.load(), 1) + " BPM";
    if (proc.apvts.getRawParameterValue ("lock")->load() > 0.5f) right << u ("  ·  ЗАМОК");
    g.drawText (right, Rectangle<float> (area.getRight() - pad - 300.0f, area.getY() + 6.0f, 300.0f, 14.0f), Justification::centredRight);
    for (int b = 0; b < 2; ++b)
        g.drawText (String (b + 1), Rectangle<float> (x0 + (float) (b * 16) * cw + 2.0f, area.getBottom() - 22.0f, 20.0f, 12.0f), Justification::centredLeft);
    g.drawText (u ("П повтор · Р реверс · М перемішати · С стоп · Х хруст · Г гейт"),
                Rectangle<float> (area.getRight() - pad - 520.0f, area.getBottom() - 22.0f, 520.0f, 12.0f), Justification::centredRight);
}
