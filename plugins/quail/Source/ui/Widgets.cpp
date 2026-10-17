#include "Widgets.h"

#include "QuailTheme.h"

namespace quail::ui
{
using animalaudio::ui::makeFont;
using animalaudio::ui::Palette;

QuailLookAndFeel::QuailLookAndFeel(const Palette& palette)
    : colours(palette)
{
    setColour(juce::PopupMenu::backgroundColourId, palette.panelRaised);
    setColour(juce::PopupMenu::textColourId, palette.text);
    setColour(juce::PopupMenu::headerTextColourId, palette.textDim);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, palette.accent.withAlpha(0.22f));
    setColour(juce::PopupMenu::highlightedTextColourId, palette.text);
    setColour(juce::TooltipWindow::backgroundColourId, palette.panelRaised);
    setColour(juce::TooltipWindow::textColourId, palette.text);
    setColour(juce::TooltipWindow::outlineColourId, palette.outline);
}

juce::Font QuailLookAndFeel::getPopupMenuFont()
{
    return makeFont(14.0f, false);
}

void QuailLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    const auto bounds = juce::Rectangle<int>(width, height).toFloat();
    g.setColour(colours.panelRaised);
    g.fillRect(bounds);
    g.setColour(colours.outline);
    g.drawRect(bounds, 1.0f);
}

void QuailLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto bounds = juce::Rectangle<int>(width, height).toFloat();
    g.setColour(colours.panelRaised);
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(colours.outline);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);
    g.setColour(colours.text);
    g.setFont(makeFont(12.5f, false));
    g.drawFittedText(text, bounds.reduced(8.0f, 4.0f).toNearestInt(), juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> QuailLookAndFeel::getTooltipBounds(const juce::String& text, juce::Point<int> screenPos,
                                                        juce::Rectangle<int> parentArea)
{
    const float textWidth = juce::GlyphArrangement::getStringWidth(makeFont(12.5f, false), text);
    const int w = juce::jmin(360, juce::roundToInt(textWidth) + 18);
    const int lines = juce::jmax(1, juce::roundToInt(std::ceil(textWidth / 342.0f)));
    const int h = 8 + lines * 16;
    return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin(parentArea);
}

MenuPill::MenuPill(const Palette& palette, const juce::String& tooltip)
    : juce::Button("menu")
    , colours(palette)
{
    setTooltip(tooltip);
    // see PillButton: fire on mouse down so a host's pointer mismatch cannot swallow the click
    setTriggeredOnMouseDown(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void MenuPill::attachTo(juce::RangedAudioParameter* p, std::function<bool(int)> itemEnabled)
{
    attachment.reset();
    parameter = p;
    enabledItem = std::move(itemEnabled);
    if (parameter == nullptr)
        return;
    attachment = std::make_unique<juce::ParameterAttachment>(*parameter,
                                                             [this](float value)
                                                             {
                                                                 index = juce::roundToInt(value);
                                                                 repaint();
                                                             });
    attachment->sendInitialUpdate();
}

void MenuPill::choose(int item)
{
    if (attachment != nullptr)
        attachment->setValueAsCompleteGesture(static_cast<float>(item));
}

void MenuPill::clicked()
{
    auto* choice = dynamic_cast<juce::AudioParameterChoice*>(parameter);
    if (choice == nullptr)
        return;
    juce::PopupMenu menu;
    for (int i = 0; i < choice->choices.size(); ++i)
        menu.addItem(i + 1, choice->choices[i], !enabledItem || enabledItem(i), i == index);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(getWidth()),
                       [safe = juce::Component::SafePointer<MenuPill>(this)](int result)
                       {
                           if (safe != nullptr && result > 0)
                               safe->choose(result - 1);
                       });
}

void MenuPill::paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const float alpha = isEnabled() ? 1.0f : 0.35f;
    auto fill = colours.panelRaised;
    if (isMouseOver && isEnabled())
        fill = fill.brighter(0.1f);
    if (isButtonDown)
        fill = fill.darker(0.1f);
    g.setColour(fill.withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(colours.outline.withMultipliedAlpha(alpha));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    juce::String text;
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(parameter))
        text = choice->choices[juce::jlimit(0, choice->choices.size() - 1, index)];

    auto caretArea = bounds.removeFromRight(bounds.getHeight()).reduced(bounds.getHeight() * 0.36f);
    juce::Path caret;
    caret.addTriangle(caretArea.getX(), caretArea.getY(), caretArea.getRight(), caretArea.getY(),
                      caretArea.getCentreX(), caretArea.getBottom());
    g.setColour(colours.accent.withMultipliedAlpha(alpha));
    g.fillPath(caret);

    g.setColour(colours.text.withMultipliedAlpha(alpha));
    g.setFont(makeFont(juce::jmin(13.0f, bounds.getHeight() * 0.55f)));
    g.drawFittedText(text.toUpperCase(), bounds.withTrimmedLeft(9.0f).toNearestInt(), juce::Justification::centredLeft,
                     1, 0.8f);
}

ValueChip::ValueChip(const juce::String& labelText, const Palette& palette)
    : label(labelText)
    , colours(palette)
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    setMouseDragSensitivity(240);
    setScrollWheelEnabled(true);
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
}

void ValueChip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(isMouseOverOrDragging() ? colours.panelRaised.brighter(0.08f) : colours.panelRaised);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(colours.outline);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    auto inner = bounds.reduced(9.0f, 0.0f);
    g.setColour(colours.textDim);
    g.setFont(makeFont(10.0f));
    g.drawText(label, inner, juce::Justification::centredLeft, false);
    g.setColour(colours.text);
    g.setFont(makeFont(13.0f));
    const auto text = valueText ? valueText(getValue()) : getTextFromValue(getValue());
    g.drawText(text, inner, juce::Justification::centredRight, false);
}

QuailKnob::QuailKnob(const animalaudio::ui::Theme& theme)
    : ThemedKnob(theme)
{
}

void QuailKnob::setDynamicRange(bool shown, double rangeDb, double liveDb)
{
    if (shown == rangeShown && juce::exactlyEqual(rangeDb, range) && std::abs(liveDb - live) < 0.05)
        return;
    rangeShown = shown;
    range = rangeDb;
    live = liveDb;
    repaint();
}

void QuailKnob::paint(juce::Graphics& g)
{
    const float alpha = isEnabled() ? 1.0f : 0.35f;
    if (alpha < 1.0f)
        g.beginTransparencyLayer(alpha);
    RotaryKnob::paint(g);
    if (alpha < 1.0f)
        g.endTransparencyLayer();
    if (!rangeShown || range == 0.0)
        return;

    auto bounds = getLocalBounds().toFloat().reduced(4.0f);
    const float outer = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float thickness = outer * 0.1f;
    const float radius = outer - thickness * 1.8f;
    const auto angleOf = [&](double value)
    {
        const double clamped = juce::jlimit(getMinimum(), getMaximum(), value);
        return kStartAngle + static_cast<float>(valueToProportionOfLength(clamped)) * (kEndAngle - kStartAngle);
    };
    const float from = angleOf(getValue());
    const float to = angleOf(getValue() + range);
    juce::Path arc;
    arc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, juce::jmin(from, to), juce::jmax(from, to), true);
    const auto colour = dynamicColour(range > 0.0);
    g.setColour(colour.withAlpha(0.85f));
    g.strokePath(arc,
                 juce::PathStrokeType(thickness * 0.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float liveAngle = angleOf(getValue() + live);
    const juce::Point<float> dot(centre.x + radius * std::sin(liveAngle), centre.y - radius * std::cos(liveAngle));
    g.setColour(colours.text);
    g.fillEllipse(juce::Rectangle<float>(thickness * 1.3f, thickness * 1.3f).withCentre(dot));
}

ThresholdBar::ThresholdBar(const Palette& palette)
    : colours(palette)
{
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    setTooltip("Threshold over the trigger level. Drag to set it, double-click for auto threshold.");
}

void ThresholdBar::attachTo(juce::RangedAudioParameter* thresholdParam, juce::RangedAudioParameter* autoParam)
{
    threshold = thresholdParam;
    autoThreshold = autoParam;
    repaint();
}

void ThresholdBar::setLevels(float triggerDb, float effectiveThresholdDb, bool active)
{
    if (std::abs(triggerDb - trigger) < 0.1f && std::abs(effectiveThresholdDb - effective) < 0.05f && active == live)
        return;
    trigger = triggerDb;
    effective = effectiveThresholdDb;
    live = active;
    repaint();
}

juce::Rectangle<float> ThresholdBar::track() const noexcept
{
    return getLocalBounds().toFloat().reduced(1.0f);
}

float ThresholdBar::xForDb(float db) const noexcept
{
    const auto t = track();
    return t.getX() + (juce::jlimit(kMinDb, kMaxDb, db) - kMinDb) / (kMaxDb - kMinDb) * t.getWidth();
}

float ThresholdBar::dbForX(float x) const noexcept
{
    const auto t = track();
    return juce::jlimit(kMinDb, kMaxDb, kMinDb + (x - t.getX()) / t.getWidth() * (kMaxDb - kMinDb));
}

bool ThresholdBar::isAuto() const noexcept
{
    return autoThreshold != nullptr && autoThreshold->getValue() > 0.5f;
}

void ThresholdBar::paint(juce::Graphics& g)
{
    const auto t = track();
    const float alpha = isEnabled() ? 1.0f : 0.35f;
    g.setColour(colours.background.withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(t, 5.0f);

    const float manual = threshold != nullptr ? threshold->convertFrom0to1(threshold->getValue()) : -30.0f;
    const float shown = isAuto() && live ? effective : manual;
    const float thresholdX = xForDb(shown);
    if (live && isEnabled())
    {
        const float levelX = xForDb(trigger);
        g.setGradientFill(juce::ColourGradient(colours.accent.withAlpha(0.15f), t.getX(), 0.0f,
                                               colours.accent.withAlpha(0.55f), t.getRight(), 0.0f, false));
        g.fillRoundedRectangle(t.withRight(juce::jmax(t.getX() + 2.0f, levelX)), 5.0f);
        if (levelX > thresholdX)
        {
            g.setColour(colours.accentAlt.withAlpha(0.75f));
            g.fillRect(juce::Rectangle<float>(thresholdX, t.getY() + 2.0f, levelX - thresholdX, t.getHeight() - 4.0f));
        }
    }
    g.setColour(colours.outline.withMultipliedAlpha(alpha));
    g.drawRoundedRectangle(t.reduced(0.5f), 5.0f, 1.0f);

    g.setColour(colours.detail.withMultipliedAlpha(alpha));
    g.fillRect(juce::Rectangle<float>(thresholdX - 1.0f, t.getY() + 1.0f, 2.0f, t.getHeight() - 2.0f));

    g.setFont(makeFont(10.5f));
    g.setColour(colours.text.withMultipliedAlpha(alpha));
    const juce::String text =
        isAuto() ? "AUTO " + (live ? juce::String(shown, 1) : juce::String()) : juce::String(manual, 1) + " dB";
    g.drawText(text.trim(), t.reduced(7.0f, 0.0f), juce::Justification::centredRight, false);
}

void ThresholdBar::mouseDown(const juce::MouseEvent& e)
{
    if (threshold == nullptr || autoThreshold == nullptr || e.mods.isPopupMenu())
        return;
    dragging = true;
    threshold->beginChangeGesture();
    if (isAuto())
    {
        autoThreshold->beginChangeGesture();
        autoThreshold->setValueNotifyingHost(0.0f);
        autoThreshold->endChangeGesture();
    }
    mouseDrag(e);
}

void ThresholdBar::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragging)
        return;
    threshold->setValueNotifyingHost(threshold->convertTo0to1(dbForX(e.position.x)));
    repaint();
}

void ThresholdBar::mouseUp(const juce::MouseEvent&)
{
    if (dragging)
        threshold->endChangeGesture();
    dragging = false;
}

void ThresholdBar::mouseDoubleClick(const juce::MouseEvent&)
{
    if (autoThreshold == nullptr)
        return;
    autoThreshold->beginChangeGesture();
    autoThreshold->setValueNotifyingHost(isAuto() ? 0.0f : 1.0f);
    autoThreshold->endChangeGesture();
    repaint();
}

OutputMeter::OutputMeter(const Palette& palette)
    : colours(palette)
{
    setTooltip("Output level (L, R) and side-chain level. Click to clear the clip light.");
}

void OutputMeter::push(float left, float right, float sidechain, bool sidechainActive)
{
    const std::array<float, 3> peaks{left, right, sidechain};
    for (std::size_t i = 0; i < peaks.size(); ++i)
    {
        const float db = juce::Decibels::gainToDecibels(peaks[i], -120.0f);
        levels[i] = juce::jmax(db, levels[i] - 1.4f);
        if (db >= holds[i])
        {
            holds[i] = db;
            holdFrames[i] = 45;
        }
        else if (--holdFrames[i] < 0)
            holds[i] = juce::jmax(db, holds[i] - 1.0f);
        if (i < 2 && db > 0.0f)
            clipped = true;
    }
    sidechainShown = sidechainActive;
    repaint();
}

float OutputMeter::yForDb(float db, juce::Rectangle<float> area) const noexcept
{
    const float t = (juce::jlimit(kMinDb, kMaxDb, db) - kMinDb) / (kMaxDb - kMinDb);
    return area.getBottom() - t * area.getHeight();
}

void OutputMeter::mouseDown(const juce::MouseEvent&)
{
    clipped = false;
    repaint();
}

void OutputMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto light = bounds.removeFromTop(8.0f).reduced(2.0f, 1.0f);
    g.setColour(clipped ? collisionColour() : colours.panelRaised);
    g.fillRoundedRectangle(light, 2.0f);
    bounds.removeFromTop(4.0f);

    const auto bars = bounds;
    const float gap = 2.0f;
    const float scWidth = sidechainShown ? bars.getWidth() * 0.22f : 0.0f;
    const float channelWidth = (bars.getWidth() - scWidth - gap * (sidechainShown ? 2.0f : 1.0f)) * 0.5f;

    const auto drawBar = [&](juce::Rectangle<float> bar, float levelDb, float holdDb, juce::Colour colour)
    {
        g.setColour(colours.background);
        g.fillRoundedRectangle(bar, 2.0f);
        const float top = yForDb(levelDb, bar);
        if (top < bar.getBottom())
        {
            g.setGradientFill(
                juce::ColourGradient(colour.withAlpha(0.5f), 0.0f, bar.getBottom(), colour, 0.0f, bar.getY(), false));
            g.fillRoundedRectangle(bar.withTop(top), 2.0f);
            if (levelDb > 0.0f)
            {
                g.setColour(collisionColour());
                g.fillRect(bar.withTop(top).withBottom(yForDb(0.0f, bar)));
            }
        }
        if (holdDb > kMinDb)
        {
            g.setColour(colours.text.withAlpha(0.8f));
            g.fillRect(bar.withTop(yForDb(holdDb, bar) - 1.0f).withHeight(1.5f));
        }
    };

    auto row = bars;
    drawBar(row.removeFromLeft(channelWidth), levels[0], holds[0], colours.accent);
    row.removeFromLeft(gap);
    drawBar(row.removeFromLeft(channelWidth), levels[1], holds[1], colours.accent);
    if (sidechainShown)
    {
        row.removeFromLeft(gap);
        drawBar(row, levels[2], holds[2], colours.accentAlt);
    }

    g.setColour(colours.textDim.withAlpha(0.5f));
    for (float db : {0.0f, -12.0f, -24.0f, -48.0f})
        g.fillRect(juce::Rectangle<float>(bars.getX(), yForDb(db, bars), bars.getWidth(), 1.0f));
}
} // namespace quail::ui
