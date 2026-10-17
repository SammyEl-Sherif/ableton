#include "BandActions.h"

namespace quail::ui
{
BandActions::BandActions(QuailProcessor& p)
    : processor(p)
{
}

float BandActions::get(int band, BandParam param) const noexcept
{
    return processor.params().value(band, param);
}

juce::String BandActions::text(int band, BandParam param) const
{
    return processor.params().band(band, param).getCurrentValueAsText();
}

int BandActions::numUsed() const noexcept
{
    int n = 0;
    for (int band = 0; band < dsp::kNumBands; ++band)
        n += isUsed(band) ? 1 : 0;
    return n;
}

void BandActions::set(int band, BandParam param, float value)
{
    begin(band, param);
    move(band, param, value);
    end(band, param);
}

void BandActions::begin(int band, BandParam param)
{
    processor.params().band(band, param).beginChangeGesture();
}

void BandActions::move(int band, BandParam param, float value)
{
    auto& p = processor.params().band(band, param);
    p.setValueNotifyingHost(p.convertTo0to1(value));
}

void BandActions::end(int band, BandParam param)
{
    processor.params().band(band, param).endChangeGesture();
}

int BandActions::firstFree() const noexcept
{
    for (int band = 0; band < dsp::kNumBands; ++band)
        if (!isUsed(band))
            return band;
    return -1;
}

dsp::FilterShape BandActions::shapeForNewBand(float hz) noexcept
{
    if (hz < 35.0f)
        return dsp::FilterShape::LowCut;
    if (hz > 15000.0f)
        return dsp::FilterShape::HighCut;
    return dsp::FilterShape::Bell;
}

int BandActions::create(float hz, float gainDb, dsp::FilterShape shape)
{
    const int band = firstFree();
    if (band < 0)
        return -1;
    remove(band);
    set(band, BandParam::shape, static_cast<float>(shape));
    set(band, BandParam::frequency, juce::jlimit(dsp::kMinHz, dsp::kMaxHz, hz));
    if (animalaudio::dsp::shapeHasGain(shape))
        set(band, BandParam::gain, juce::jlimit(-dsp::kMaxGainDb, dsp::kMaxGainDb, gainDb));
    set(band, BandParam::used, 1.0f);
    return band;
}

void BandActions::remove(int band)
{
    for (int i = 0; i < kNumBandParams; ++i)
    {
        auto& p = processor.params().band(band, static_cast<BandParam>(i));
        if (!juce::exactlyEqual(p.getValue(), p.getDefaultValue()))
        {
            p.beginChangeGesture();
            p.setValueNotifyingHost(p.getDefaultValue());
            p.endChangeGesture();
        }
    }
}

void BandActions::removeAll()
{
    for (int band = 0; band < dsp::kNumBands; ++band)
        if (isUsed(band))
            remove(band);
}
} // namespace quail::ui
