#include "BandPanel.h"

#include "QuailTheme.h"

namespace quail::ui
{
using animalaudio::dsp::shapeHasGain;
using animalaudio::dsp::shapeHasQ;
using animalaudio::dsp::shapeHasSlope;

namespace
{
void drawPowerGlyph(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    const auto c = area.getCentre();
    const float r = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f;
    juce::Path ring;
    ring.addCentredArc(c.x, c.y, r * 0.8f, r * 0.8f, 0.0f, 0.65f, juce::MathConstants<float>::twoPi - 0.65f, true);
    g.setColour(colour);
    g.strokePath(ring, juce::PathStrokeType(r * 0.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawLine(c.x, c.y - r, c.x, c.y - r * 0.15f, r * 0.2f);
}

void drawArrowGlyph(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, bool right)
{
    juce::Path arrow;
    const float s = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f;
    const auto c = area.getCentre();
    const float d = right ? 1.0f : -1.0f;
    arrow.startNewSubPath(c.x - d * s * 0.35f, c.y - s * 0.7f);
    arrow.lineTo(c.x + d * s * 0.35f, c.y);
    arrow.lineTo(c.x - d * s * 0.35f, c.y + s * 0.7f);
    g.setColour(colour);
    g.strokePath(arrow, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

constexpr float kCorner = 14.0f;
constexpr int kMargin = 16;
constexpr int kIdentityWidth = 180;
constexpr int kHalfWidth = (kIdentityWidth - 6) / 2;
constexpr int kFilterDivider = 210;
constexpr int kFilterX = 224;
constexpr int kFilterKnob = 64;
constexpr int kDynamicsDivider = 430;
constexpr int kDynamicsX = 444;
constexpr int kToggleWidth = 90;
constexpr int kDynamicsKnobsX = 644;
constexpr int kDynamicsKnob = 50;
constexpr int kRowHeight = 24;
constexpr int kThresholdLabelY = 84;
} // namespace

BandPanel::BandPanel(BandActions& a, const animalaudio::ui::Theme& t)
    : actions(a)
    , theme(t)
    , colours(t.palette)
    , shapeMenu(colours, "Filter shape")
    , slopeMenu(colours, "Slope of cuts, shelves and the band shapes")
    , placementMenu(colours, "Which part of the signal the band processes: stereo, left, right, mid or side")
    , threshold(colours)
{
    addKnob(BandParam::frequency, "FREQ", "Frequency (drag, scroll; double-click for the default)", false);
    addKnob(BandParam::gain, "GAIN", "Gain. The inner arc shows the dynamic range and the dot the live gain", true);
    addKnob(BandParam::q, "Q", "Q / bandwidth", false);
    addKnob(BandParam::range, "RANGE",
            "Dynamic range: how far the gain moves when the trigger is over the threshold (negative = duck)", true);
    addKnob(BandParam::attack, "ATTACK", "Dynamics attack", false);
    addKnob(BandParam::release, "RELEASE", "Dynamics release", false);
    addKnob(BandParam::density, "DENSITY", "Spectral density: how independently neighbouring frequencies move", false);

    addToggle(BandParam::enabled, "", "Band on / off");
    addToggle(BandParam::dynamics, "DYNAMIC", "Dynamic EQ: the gain follows the trigger level");
    addToggle(BandParam::sidechain, "SIDE CHAIN",
              "Trigger from the side-chain input (route another track to Quail's side chain in the host)");
    addToggle(BandParam::spectral, "SPECTRAL",
              "Spectral dynamics: every frequency in the band reacts to its own level in the trigger");
    addToggle(BandParam::spectralTilt, "TILT", "Spectral tilt: weight the trigger 3 dB/oct towards the highs");
    addToggle(BandParam::autoThreshold, "AUTO", "Auto threshold: follow the trigger's average level");
    toggles[BandParam::enabled]->glyph = [](juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
    { drawPowerGlyph(g, area, colour); };
    for (auto param : {BandParam::dynamics, BandParam::sidechain, BandParam::spectral, BandParam::spectralTilt})
        toggles[param]->setShowsLight(true);

    for (auto* menu : {&shapeMenu, &slopeMenu, &placementMenu})
        addAndMakeVisible(*menu);
    addAndMakeVisible(threshold);

    previous.glyph = [](juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
    { drawArrowGlyph(g, area, colour, false); };
    next.glyph = [](juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
    { drawArrowGlyph(g, area, colour, true); };
    previous.setTooltip("Previous band");
    next.setTooltip("Next band");
    previous.onClick = [this]
    {
        if (onNavigate)
            onNavigate(-1);
    };
    next.onClick = [this]
    {
        if (onNavigate)
            onNavigate(1);
    };

    solo.setTooltip("Hold to hear only this band's frequency range");
    solo.onStateChange = [this]
    {
        const bool held = solo.isDown() && band >= 0;
        if ((actions.getProcessor().getSoloBand() == band) != held)
            actions.getProcessor().setSoloBand(held ? band : -1);
        solo.setToggleState(held, juce::dontSendNotification);
    };
    remove.setTooltip("Remove this band");
    remove.onClick = [this]
    {
        if (onRemove)
            onRemove();
    };
    for (auto* button : {&previous, &next, &solo, &remove})
        addAndMakeVisible(*button);

    setSize(kWidth + 2 * kShadow, kHeight + 2 * kShadow);
    setBand(-1);
}

BandPanel::~BandPanel()
{
    if (band >= 0 && actions.getProcessor().getSoloBand() == band)
        actions.getProcessor().setSoloBand(-1);
}

void BandPanel::addKnob(BandParam param, const juce::String& label, const juce::String& tooltip, bool bipolar)
{
    Knob k;
    k.knob = std::make_unique<QuailKnob>(theme);
    k.knob->setTooltip(tooltip);
    k.knob->onValueChange = [this] { repaint(); };
    if (bipolar)
        k.knob->setArcOrigin(0.0);
    k.label = label;
    addAndMakeVisible(*k.knob);
    knobs[param] = std::move(k);
}

void BandPanel::addToggle(BandParam param, const juce::String& text, const juce::String& tooltip)
{
    auto button = std::make_unique<QuailToggle>(text, colours);
    button->setTooltip(tooltip);
    button->onClick = [this, param] { toggle(param); };
    addAndMakeVisible(*button);
    toggles[param] = std::move(button);
}

juce::Slider& BandPanel::getKnob(BandParam param)
{
    return *knobs.at(param).knob;
}

juce::Button& BandPanel::getToggle(BandParam param)
{
    return *toggles.at(param);
}

MenuPill& BandPanel::getMenu(BandParam param)
{
    if (param == BandParam::slope)
        return slopeMenu;
    if (param == BandParam::placement)
        return placementMenu;
    return shapeMenu;
}

void BandPanel::ensureDynamics()
{
    if (actions.get(band, BandParam::dynamics) < 0.5f)
        actions.set(band, BandParam::dynamics, 1.0f);
    if (actions.get(band, BandParam::range) == 0.0f)
        actions.set(band, BandParam::range, -6.0f);
}

void BandPanel::toggle(BandParam param)
{
    if (band < 0)
        return;
    const auto settings = actions.getProcessor().getSettings().bands[static_cast<std::size_t>(band)];
    const bool on = actions.get(band, param) > 0.5f;
    if (param == BandParam::dynamics)
    {
        if (settings.hasDynamics())
            actions.set(band, BandParam::dynamics, 0.0f);
        else
            ensureDynamics();
    }
    else
    {
        actions.set(band, param, on ? 0.0f : 1.0f);
        if (!on && (param == BandParam::sidechain || param == BandParam::spectral))
            ensureDynamics();
    }
    refresh();
}

void BandPanel::applyTint(juce::Colour colour)
{
    for (auto& [param, k] : knobs)
        k.knob->setTint(colour);
    for (auto& [param, toggleButton] : toggles)
        toggleButton->setTint(colour);
    solo.setTint(colour);
    threshold.setTint(colour);
}

void BandPanel::setBand(int newBand)
{
    if (band >= 0 && band != newBand && actions.getProcessor().getSoloBand() == band)
        actions.getProcessor().setSoloBand(-1);
    band = newBand;
    lastKey.clear();
    auto& params = actions.getProcessor().params();
    for (auto& [param, k] : knobs)
    {
        k.attachment.reset();
        if (band >= 0)
            k.attachment = std::make_unique<juce::SliderParameterAttachment>(params.band(band, param), *k.knob);
    }
    if (band < 0)
    {
        shapeMenu.attachTo(nullptr);
        slopeMenu.attachTo(nullptr);
        placementMenu.attachTo(nullptr);
        threshold.attachTo(nullptr, nullptr);
        return;
    }
    applyTint(bandColour(band));
    shapeMenu.attachTo(&params.band(band, BandParam::shape));
    slopeMenu.attachTo(&params.band(band, BandParam::slope));
    placementMenu.attachTo(&params.band(band, BandParam::placement),
                           [this](int item) { return item == 0 || actions.getProcessor().isStereo(); });
    threshold.attachTo(&params.band(band, BandParam::threshold), &params.band(band, BandParam::autoThreshold));
    refresh();
}

juce::String BandPanel::stateKey() const
{
    juce::String key(band);
    for (int i = 0; i < kNumBandParams; ++i)
        key << '|' << actions.get(band, static_cast<BandParam>(i));
    key << '|' << (actions.getProcessor().isSidechainActive() ? 1 : 0);
    return key;
}

void BandPanel::refresh()
{
    if (band < 0)
        return;
    auto& processor = actions.getProcessor();
    const auto settings = processor.getSettings();
    const auto& b = settings.bands[static_cast<std::size_t>(band)];
    const auto& engine = processor.getEngine();
    const bool dynamicShape = dsp::supportsDynamics(b.shape);
    const bool dynamicsOn = b.hasDynamics();

    const auto setEnabled = [](juce::Component& c, bool on)
    {
        if (c.isEnabled() == on)
            return;
        c.setEnabled(on);
        // QuailKnob dims itself; buttons, menus and the threshold bar are dimmed as a whole
        if (dynamic_cast<QuailKnob*>(&c) == nullptr)
            c.setAlpha(on ? 1.0f : 0.35f);
    };
    setEnabled(*knobs[BandParam::gain].knob, shapeHasGain(b.shape));
    setEnabled(*knobs[BandParam::q].knob, shapeHasQ(b.shape, b.slope));
    setEnabled(slopeMenu, shapeHasSlope(b.shape));
    setEnabled(placementMenu, processor.isStereo());
    setEnabled(*toggles[BandParam::dynamics], dynamicShape);
    setEnabled(*knobs[BandParam::range].knob, dynamicShape);
    for (auto param : {BandParam::attack, BandParam::release})
        setEnabled(*knobs[param].knob, dynamicsOn);
    setEnabled(*toggles[BandParam::sidechain], dynamicShape);
    setEnabled(*toggles[BandParam::spectral], dynamicShape && dsp::supportsSpectral(b.shape));
    setEnabled(*toggles[BandParam::spectralTilt], b.isSpectral());
    setEnabled(*knobs[BandParam::density].knob, b.isSpectral());
    setEnabled(*toggles[BandParam::autoThreshold], dynamicsOn);
    setEnabled(threshold, dynamicsOn);

    const auto lit = [&](BandParam param, bool on)
    {
        auto& button = *toggles[param];
        if (button.getToggleState() != on)
            button.setToggleState(on, juce::dontSendNotification);
    };
    lit(BandParam::enabled, b.enabled);
    lit(BandParam::dynamics, dynamicsOn);
    lit(BandParam::sidechain, b.externalSidechain);
    lit(BandParam::spectral, b.spectral && dsp::supportsSpectral(b.shape));
    lit(BandParam::spectralTilt, b.spectralTilt);
    lit(BandParam::autoThreshold, b.autoThreshold);
    toggles[BandParam::sidechain]->setTint(processor.isSidechainActive() ? bandColour(band) : warningColour());

    const float live = b.isSpectral() ? engine.getSpectral().getBandGainDb(band) : engine.getBandDynamicDb(band);
    knobs[BandParam::gain].knob->setDynamicRange(dynamicsOn, b.rangeDb, live);
    threshold.setLevels(engine.getBandTriggerDb(band), engine.getBandThresholdDb(band), dynamicsOn);

    const auto key = stateKey();
    if (key != lastKey)
    {
        lastKey = key;
        repaint();
    }
}

void BandPanel::resized()
{
    const auto o = getCardBounds().getPosition();
    const auto row = [o](int x, int y, int w, int h = kRowHeight) { return juce::Rectangle<int>(x, y, w, h) + o; };
    toggles[BandParam::enabled]->setBounds(row(kMargin, 14, 24));
    previous.setBounds(row(kMargin + kIdentityWidth - 52, 14, 24));
    next.setBounds(row(kMargin + kIdentityWidth - 24, 14, 24));
    shapeMenu.setBounds(row(kMargin, 44, kIdentityWidth));
    slopeMenu.setBounds(row(kMargin, 74, kHalfWidth));
    placementMenu.setBounds(row(kMargin + kIdentityWidth - kHalfWidth, 74, kHalfWidth));
    solo.setBounds(row(kMargin, 104, kHalfWidth));
    remove.setBounds(row(kMargin + kIdentityWidth - kHalfWidth, 104, kHalfWidth));

    int x = kFilterX;
    for (auto param : {BandParam::frequency, BandParam::gain, BandParam::q})
    {
        knobs[param].knob->setBounds(row(x + (kFilterKnob - 60) / 2, 22, 60, 60));
        x += kFilterKnob;
    }

    toggles[BandParam::dynamics]->setBounds(row(kDynamicsX, 14, kToggleWidth));
    toggles[BandParam::sidechain]->setBounds(row(kDynamicsX + kToggleWidth + 6, 14, kToggleWidth));
    toggles[BandParam::spectral]->setBounds(row(kDynamicsX, 44, kToggleWidth));
    toggles[BandParam::spectralTilt]->setBounds(row(kDynamicsX + kToggleWidth + 6, 44, kToggleWidth));
    threshold.setBounds(row(kDynamicsX, 102, 2 * kToggleWidth + 6 - 54, 26));
    toggles[BandParam::autoThreshold]->setBounds(row(kDynamicsX + 2 * kToggleWidth + 6 - 48, 103, 48));

    x = kDynamicsKnobsX;
    for (auto param : {BandParam::range, BandParam::attack, BandParam::release, BandParam::density})
    {
        knobs[param].knob->setBounds(row(x + (kDynamicsKnob - 48) / 2, 26, 48, 48));
        x += kDynamicsKnob;
    }

    shadow = juce::Image(juce::Image::ARGB, juce::jmax(1, getWidth()), juce::jmax(1, getHeight()), true);
    juce::Graphics sg(shadow);
    juce::Path card;
    card.addRoundedRectangle(getCardBounds().toFloat(), kCorner);
    juce::DropShadow(colours.background.withAlpha(0.8f), 16, {0, 5}).drawForPath(sg, card);
}

bool BandPanel::hitTest(int x, int y)
{
    return getCardBounds().contains(x, y);
}

void BandPanel::paint(juce::Graphics& g)
{
    const auto card = getCardBounds().toFloat();
    g.drawImageAt(shadow, 0, 0);
    g.setGradientFill(
        juce::ColourGradient(colours.panelRaised, 0.0f, card.getY(), colours.panel, 0.0f, card.getBottom(), false));
    g.fillRoundedRectangle(card, kCorner);
    g.setColour(colours.text.withAlpha(0.09f));
    g.drawRoundedRectangle(card.reduced(0.5f), kCorner, 1.0f);

    g.setColour(colours.text.withAlpha(0.06f));
    for (int x : {kFilterDivider, kDynamicsDivider})
        g.fillRect(juce::Rectangle<float>(card.getX() + static_cast<float>(x), card.getY() + 18.0f, 1.0f,
                                          card.getHeight() - 36.0f));

    if (band < 0)
        return;
    const auto colour = bandColour(band);
    const auto origin = card.getPosition();
    g.setColour(colour.withAlpha(0.3f));
    g.fillEllipse(juce::Rectangle<float>(14.0f, 14.0f).withCentre(origin + juce::Point<float>(56.0f, 26.0f)));
    g.setColour(colour);
    g.fillEllipse(juce::Rectangle<float>(8.0f, 8.0f).withCentre(origin + juce::Point<float>(56.0f, 26.0f)));
    g.setColour(colours.text);
    g.setFont(valueFont(13.5f).withExtraKerningFactor(0.06f));
    g.drawText("BAND " + juce::String(band + 1),
               juce::Rectangle<float>(origin.x + 68.0f, origin.y + 14.0f, 80.0f, 24.0f),
               juce::Justification::centredLeft, false);

    for (auto& [param, k] : knobs)
    {
        const auto r = k.knob->getBounds().toFloat();
        const float alpha = k.knob->isEnabled() ? 1.0f : 0.35f;
        g.setColour(colours.textDim.withMultipliedAlpha(alpha));
        g.setFont(labelFont(9.0f));
        g.drawText(k.label, juce::Rectangle<float>(r.getCentreX() - 34.0f, r.getBottom() + 2.0f, 68.0f, 12.0f),
                   juce::Justification::centred, false);
        g.setColour(colours.text.withMultipliedAlpha(alpha));
        g.setFont(valueFont(12.0f));
        g.drawText(actions.text(band, param),
                   juce::Rectangle<float>(r.getCentreX() - 34.0f, r.getBottom() + 15.0f, 68.0f, 15.0f),
                   juce::Justification::centred, false);
    }

    g.setColour(colours.textDim.withMultipliedAlpha(threshold.isEnabled() ? 1.0f : 0.35f));
    g.setFont(labelFont(9.0f));
    g.drawText("THRESHOLD",
               juce::Rectangle<float>(origin.x + kDynamicsX + 2.0f, origin.y + kThresholdLabelY, 120.0f, 12.0f),
               juce::Justification::centredLeft, false);
}

void BandPanel::paintOverChildren(juce::Graphics& g)
{
    if (band < 0 || !toggles[BandParam::sidechain]->getToggleState() || actions.getProcessor().isSidechainActive())
        return;
    const auto origin = getCardBounds().toFloat().getPosition();
    g.setColour(warningColour());
    g.setFont(labelFont(8.5f));
    g.drawText(
        "NO SIDE-CHAIN INPUT",
        juce::Rectangle<float>(origin.x + kDynamicsX, origin.y + kThresholdLabelY, 2.0f * kToggleWidth + 6.0f, 12.0f),
        juce::Justification::centredRight, false);
}
} // namespace quail::ui
