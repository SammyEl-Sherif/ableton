#include "Widgets.h"

#include "QuailTheme.h"

namespace quail::ui
{
using animalaudio::ui::Palette;

namespace
{
constexpr float kChipRadius = 7.0f;

juce::Font buttonFont(float height)
{
    return valueFont(height).withExtraKerningFactor(0.06f);
}

void drawChevron(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    const auto c = area.getCentre();
    const float s = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f;
    juce::Path p;
    p.startNewSubPath(c.x - s, c.y - s * 0.45f);
    p.lineTo(c.x, c.y + s * 0.45f);
    p.lineTo(c.x + s, c.y - s * 0.45f);
    g.setColour(colour);
    g.strokePath(p, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void drawChip(juce::Graphics& g, juce::Rectangle<float> bounds, const Palette& palette, bool hot, bool down)
{
    g.setColour(palette.text.withAlpha(down ? 0.1f : hot ? 0.075f : 0.04f));
    g.fillRoundedRectangle(bounds, kChipRadius);
    g.setColour(palette.text.withAlpha(hot ? 0.16f : 0.09f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), kChipRadius, 1.0f);
}
} // namespace

QuailLookAndFeel::QuailLookAndFeel(const Palette& palette)
    : colours(palette)
{
    setColour(juce::PopupMenu::backgroundColourId, palette.panelRaised);
    setColour(juce::PopupMenu::textColourId, palette.text);
    setColour(juce::PopupMenu::headerTextColourId, palette.textDim);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, palette.accent.withAlpha(0.16f));
    setColour(juce::PopupMenu::highlightedTextColourId, palette.text);
    setColour(juce::TooltipWindow::backgroundColourId, palette.panelRaised);
    setColour(juce::TooltipWindow::textColourId, palette.text);
    setColour(juce::TooltipWindow::outlineColourId, palette.outline);
}

juce::Font QuailLookAndFeel::getPopupMenuFont()
{
    return juce::Font(juce::FontOptions(13.5f));
}

void QuailLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    const auto bounds = juce::Rectangle<int>(width, height).toFloat();
    g.setColour(colours.panelRaised);
    g.fillRect(bounds);
    g.setColour(colours.text.withAlpha(0.1f));
    g.drawRect(bounds, 1.0f);
}

void QuailLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto bounds = juce::Rectangle<int>(width, height).toFloat();
    g.setColour(colours.panelRaised);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(colours.text.withAlpha(0.1f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);
    g.setColour(colours.text);
    g.setFont(juce::Font(juce::FontOptions(12.5f)));
    g.drawFittedText(text, bounds.reduced(9.0f, 4.0f).toNearestInt(), juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> QuailLookAndFeel::getTooltipBounds(const juce::String& text, juce::Point<int> screenPos,
                                                        juce::Rectangle<int> parentArea)
{
    const float textWidth = juce::GlyphArrangement::getStringWidth(juce::Font(juce::FontOptions(12.5f)), text);
    const int w = juce::jmin(360, juce::roundToInt(textWidth) + 20);
    const int lines = juce::jmax(1, juce::roundToInt(std::ceil(textWidth / 340.0f)));
    const int h = 10 + lines * 16;
    return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin(parentArea);
}

void drawSegmentTrack(juce::Graphics& g, juce::Rectangle<float> area, const Palette& palette)
{
    g.setColour(palette.text.withAlpha(0.035f));
    g.fillRoundedRectangle(area, kChipRadius + 2.0f);
    g.setColour(palette.text.withAlpha(0.08f));
    g.drawRoundedRectangle(area.reduced(0.5f), kChipRadius + 2.0f, 1.0f);
}

QuailToggle::QuailToggle(const juce::String& text, const Palette& palette, Style s)
    : PillButton(text, palette)
    , colours(palette)
    , style(s)
    , tint(palette.accent)
{
}

void QuailToggle::setTint(juce::Colour colour)
{
    if (colour == tint)
        return;
    tint = colour;
    repaint();
}

void QuailToggle::setShowsLight(bool shown)
{
    light = shown;
    repaint();
}

void QuailToggle::paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    const bool on = getToggleState();
    const bool hot = isMouseOver && isEnabled();
    const float radius = juce::jmin(kChipRadius, bounds.getHeight() * 0.35f);
    const auto& ink = colours.text;

    if (style == Style::chip)
    {
        g.setColour(on ? tint.withAlpha(isButtonDown ? 0.18f : 0.11f)
                       : ink.withAlpha(isButtonDown ? 0.1f
                                       : hot        ? 0.075f
                                                    : 0.04f));
        g.fillRoundedRectangle(bounds, radius);
        g.setColour(on ? tint.withAlpha(0.32f) : ink.withAlpha(hot ? 0.16f : 0.09f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
    }
    else if (on || hot)
    {
        g.setColour(on ? tint.withAlpha(isButtonDown ? 0.22f : 0.15f) : ink.withAlpha(0.05f));
        g.fillRoundedRectangle(bounds, radius);
        if (on)
        {
            g.setColour(tint.withAlpha(0.3f));
            g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
        }
    }

    const auto textColour = on || hot ? ink : colours.textDim;
    const auto glyphColour = on ? tint : textColour;
    const auto text = getButtonText();
    const auto font = buttonFont(juce::jmin(11.0f, bounds.getHeight() * 0.44f));
    const float glyphSize = bounds.getHeight() * 0.42f;
    if (glyph && text.isEmpty())
    {
        glyph(g, juce::Rectangle<float>(glyphSize, glyphSize).withCentre(bounds.getCentre()), glyphColour);
        return;
    }

    constexpr float lightSize = 6.0f;
    constexpr float spacing = 7.0f;
    const float textWidth = juce::GlyphArrangement::getStringWidth(font, text);
    const float lead = (light ? lightSize + spacing : 0.0f) + (glyph ? glyphSize + spacing : 0.0f);
    float x = bounds.getCentreX() - (lead + textWidth) * 0.5f;
    const float y = bounds.getCentreY();
    if (light)
    {
        const auto dot = juce::Rectangle<float>(lightSize, lightSize).withCentre({x + lightSize * 0.5f, y});
        if (on)
        {
            g.setColour(tint.withAlpha(0.28f));
            g.fillEllipse(dot.expanded(2.5f));
        }
        g.setColour(on ? tint : ink.withAlpha(0.2f));
        g.fillEllipse(dot);
        x += lightSize + spacing;
    }
    if (glyph)
    {
        glyph(g, juce::Rectangle<float>(glyphSize, glyphSize).withCentre({x + glyphSize * 0.5f, y}), glyphColour);
        x += glyphSize + spacing;
    }
    g.setColour(textColour);
    g.setFont(font);
    g.drawText(text, juce::Rectangle<float>(x, bounds.getY(), textWidth + 4.0f, bounds.getHeight()),
               juce::Justification::centredLeft, false);
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
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    drawChip(g, bounds, colours, isMouseOver && isEnabled(), isButtonDown);

    juce::String text;
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(parameter))
        text = choice->choices[juce::jlimit(0, choice->choices.size() - 1, index)];

    const float h = bounds.getHeight();
    drawChevron(g, bounds.removeFromRight(h).reduced(h * 0.37f).translated(-2.0f, 0.0f), colours.textDim);
    g.setColour(colours.text);
    g.setFont(buttonFont(juce::jmin(11.0f, h * 0.45f)));
    g.drawFittedText(text.toUpperCase(), bounds.withTrimmedLeft(10.0f).toNearestInt(), juce::Justification::centredLeft,
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
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    drawChip(g, bounds, colours, isMouseOverOrDragging(), false);

    const auto inner = bounds.reduced(10.0f, 0.0f);
    g.setColour(colours.textDim);
    g.setFont(labelFont(9.5f));
    g.drawText(label, inner, juce::Justification::centredLeft, false);
    g.setColour(colours.text);
    g.setFont(valueFont(12.0f));
    const auto text = valueText ? valueText(getValue()) : getTextFromValue(getValue());
    g.drawText(text, inner, juce::Justification::centredRight, false);
}

QuailKnob::QuailKnob(const animalaudio::ui::Theme& t)
    : RotaryKnob(t.palette)
    , theme(t)
    , tint(t.palette.accent)
{
}

void QuailKnob::setTint(juce::Colour colour)
{
    if (colour == tint)
        return;
    tint = colour;
    repaint();
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

float QuailKnob::angleFor(double value)
{
    const double clamped = juce::jlimit(getMinimum(), getMaximum(), value);
    return kStartAngle + static_cast<float>(valueToProportionOfLength(clamped)) * (kEndAngle - kStartAngle);
}

void QuailKnob::paint(juce::Graphics& g)
{
    const float alpha = isEnabled() ? 1.0f : 0.35f;
    if (alpha < 1.0f)
        g.beginTransparencyLayer(alpha);

    const auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    const float outer = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float thickness = juce::jmax(2.0f, outer * 0.065f);
    const float arcRadius = outer - thickness * 1.5f;
    const auto stroke = [](float width)
    { return juce::PathStrokeType(width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };
    const auto arc = [&](float radius, float from, float to)
    {
        juce::Path p;
        p.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, juce::jmin(from, to), juce::jmax(from, to), true);
        return p;
    };

    g.setColour(colours.text.withAlpha(0.08f));
    g.strokePath(arc(arcRadius, kStartAngle, kEndAngle), stroke(thickness));

    const float angle = angleFor(getValue());
    const float origin = angleFor(getArcOrigin().value_or(getMinimum()));
    if (std::abs(angle - origin) > 0.01f)
    {
        const auto value = arc(arcRadius, origin, angle);
        g.setColour(tint.withAlpha(0.1f));
        g.strokePath(value, stroke(thickness * 2.6f));
        g.setColour(tint);
        g.strokePath(value, stroke(thickness));
    }

    if (rangeShown && !juce::exactlyEqual(range, 0.0))
    {
        const float inner = arcRadius - thickness * 2.0f;
        g.setColour(tint.withAlpha(0.55f));
        g.strokePath(arc(inner, angle, angleFor(getValue() + range)), stroke(thickness * 0.8f));
        const float liveAngle = angleFor(getValue() + live);
        const juce::Point<float> dot(centre.x + inner * std::sin(liveAngle), centre.y - inner * std::cos(liveAngle));
        g.setColour(colours.text);
        g.fillEllipse(juce::Rectangle<float>(thickness * 1.7f, thickness * 1.7f).withCentre(dot));
    }

    theme.motif->drawKnobCap(g, centre, arcRadius - thickness * 3.4f, angle - juce::MathConstants<float>::twoPi,
                             colours);
    if (alpha < 1.0f)
        g.endTransparencyLayer();
}

ThresholdBar::ThresholdBar(const Palette& palette)
    : colours(palette)
    , tint(palette.accent)
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

void ThresholdBar::setTint(juce::Colour colour)
{
    if (colour == tint)
        return;
    tint = colour;
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
    return getLocalBounds().toFloat().reduced(0.5f);
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
    const float radius = juce::jmin(kChipRadius, t.getHeight() * 0.35f);
    g.setColour(colours.background.withAlpha(0.85f));
    g.fillRoundedRectangle(t, radius);

    const float manual = threshold != nullptr ? threshold->convertFrom0to1(threshold->getValue()) : -30.0f;
    const float shown = isAuto() && live ? effective : manual;
    const float thresholdX = xForDb(shown);
    if (live && isEnabled())
    {
        const auto inner = t.reduced(2.0f);
        const float levelX = juce::jlimit(inner.getX(), inner.getRight(), xForDb(trigger));
        g.setGradientFill(juce::ColourGradient(tint.withAlpha(0.03f), inner.getX(), 0.0f, tint.withAlpha(0.2f),
                                               inner.getRight(), 0.0f, false));
        g.fillRoundedRectangle(inner.withRight(juce::jmax(inner.getX() + 1.0f, levelX)), radius - 2.0f);
        if (levelX > thresholdX)
        {
            g.setColour(tint.withAlpha(0.42f));
            g.fillRoundedRectangle(inner.withLeft(thresholdX).withRight(levelX), 2.0f);
        }
    }
    g.setColour(colours.text.withAlpha(0.09f));
    g.drawRoundedRectangle(t.reduced(0.5f), radius, 1.0f);

    g.setColour(colours.text);
    g.fillRoundedRectangle(juce::Rectangle<float>(thresholdX - 1.0f, t.getY() + 3.0f, 2.0f, t.getHeight() - 6.0f),
                           1.0f);

    g.setFont(valueFont(10.5f));
    g.setColour(colours.text.withAlpha(0.9f));
    const juce::String text =
        isAuto() ? "AUTO " + (live ? juce::String(shown, 1) : juce::String()) : juce::String(manual, 1) + " dB";
    const bool markerOnRight = thresholdX > t.getCentreX();
    g.drawText(text.trim(), t.reduced(9.0f, 0.0f),
               markerOnRight ? juce::Justification::centredLeft : juce::Justification::centredRight, false);
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
    const int count = sidechainShown ? 3 : 2;
    constexpr float gap = 3.0f;
    const float barWidth =
        juce::jmin(7.0f, (bounds.getWidth() - gap * static_cast<float>(count - 1)) / static_cast<float>(count));
    const float total = barWidth * static_cast<float>(count) + gap * static_cast<float>(count - 1);
    const float left = bounds.getCentreX() - total * 0.5f;

    const auto light = bounds.removeFromTop(5.0f).withX(left).withWidth(total);
    g.setColour(clipped ? collisionColour() : colours.text.withAlpha(0.07f));
    g.fillRoundedRectangle(light, 2.5f);
    bounds.removeFromTop(7.0f);

    const auto drawBar = [&](int index, juce::Colour colour)
    {
        const auto bar = juce::Rectangle<float>(left + static_cast<float>(index) * (barWidth + gap), bounds.getY(),
                                                barWidth, bounds.getHeight());
        const auto i = static_cast<std::size_t>(index);
        g.setColour(colours.text.withAlpha(0.045f));
        g.fillRoundedRectangle(bar, 2.0f);
        const float top = yForDb(levels[i], bar);
        if (top < bar.getBottom() - 0.5f)
        {
            g.setGradientFill(juce::ColourGradient(colour.withAlpha(0.3f), 0.0f, bar.getBottom(), colour, 0.0f,
                                                   yForDb(0.0f, bar), false));
            g.fillRoundedRectangle(bar.withTop(top), 2.0f);
            if (levels[i] > 0.0f)
            {
                g.setColour(collisionColour());
                g.fillRect(bar.withTop(top).withBottom(yForDb(0.0f, bar)));
            }
        }
        if (holds[i] > kMinDb + 0.5f)
        {
            g.setColour(colours.text.withAlpha(0.85f));
            g.fillRect(bar.withTop(yForDb(holds[i], bar) - 1.0f).withHeight(1.5f));
        }
    };
    drawBar(0, colours.accent);
    drawBar(1, colours.accent);
    if (sidechainShown)
        drawBar(2, sidechainColour());

    g.setColour(colours.background.withAlpha(0.6f));
    for (float db : {0.0f, -12.0f, -24.0f, -48.0f})
        g.fillRect(juce::Rectangle<float>(left, yForDb(db, bounds), total, 1.0f));
}
} // namespace quail::ui
