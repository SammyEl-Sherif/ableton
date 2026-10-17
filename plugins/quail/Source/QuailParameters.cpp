#include "QuailParameters.h"

#include <cmath>

namespace quail
{
namespace
{
constexpr std::array<const char*, kNumBandParams> kBandSuffixes{
    "Used", "On",     "Freq",       "Gain",   "Q",       "Shape", "Slope",    "Stereo",  "Range",
    "Dyn",  "Thresh", "AutoThresh", "Attack", "Release", "SC",    "Spectral", "Density", "Tilt"};

constexpr std::array<const char*, kNumBandParams> kBandNames{"Used",
                                                             "Enabled",
                                                             "Frequency",
                                                             "Gain",
                                                             "Q",
                                                             "Shape",
                                                             "Slope",
                                                             "Stereo Placement",
                                                             "Dynamic Range",
                                                             "Dynamics",
                                                             "Threshold",
                                                             "Auto Threshold",
                                                             "Attack",
                                                             "Release",
                                                             "External Side Chain",
                                                             "Spectral",
                                                             "Spectral Density",
                                                             "Spectral Tilt"};

constexpr std::array<const char*, kNumGlobalParams> kGlobalIds{"outputGain",  "outputPan", "gainScale",  "autoGain",
                                                               "phaseInvert", "bypass",    "spectralRes"};

/** Logarithmic range from low to high (no skew guessing: equal ratios per slider distance). */
juce::NormalisableRange<float> logRange(float low, float high)
{
    return {low, high, [](float start, float end, float t) { return start * std::pow(end / start, t); },
            [](float start, float end, float v) { return std::log(v / start) / std::log(end / start); },
            [](float start, float end, float v) { return juce::jlimit(start, end, v); }};
}

/** Linear dB range that snaps to exact tenths: JUCE's interval snap (start + n * interval) lands 0 dB on ~2.7e-7,
    exact tenths keep a flat band bit-exact. */
juce::NormalisableRange<float> tenthsRange(float low, float high)
{
    juce::NormalisableRange<float> range(
        low, high, [](float start, float end, float t) { return start + t * (end - start); },
        [](float start, float end, float v) { return (v - start) / (end - start); },
        [](float start, float end, float v) { return juce::jlimit(start, end, std::round(v * 10.0f) / 10.0f); });
    range.interval = 0.1f;
    return range;
}

float parseHz(const juce::String& text)
{
    const float value = text.getFloatValue();
    return text.containsIgnoreCase("k") ? value * 1000.0f : value;
}

juce::String formatMs(float ms)
{
    return ms < 10.0f ? juce::String(ms, 1) + " ms" : juce::String(juce::roundToInt(ms)) + " ms";
}

juce::String formatPercent(float v)
{
    return juce::String(juce::roundToInt(v)) + "%";
}

juce::ParameterID pid(const juce::String& id)
{
    return {id, 1};
}

std::unique_ptr<juce::AudioParameterFloat> dbParameter(const juce::String& id, const juce::String& name, float low,
                                                       float high, float def)
{
    return std::make_unique<juce::AudioParameterFloat>(
        pid(id), name, tenthsRange(low, high), def,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction([](float v, int) { return formatDb(v); })
            .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue(); }));
}

std::unique_ptr<juce::AudioParameterBool> boolParameter(const juce::String& id, const juce::String& name, bool def)
{
    return std::make_unique<juce::AudioParameterBool>(pid(id), name, def);
}
} // namespace

juce::String bandParamId(int band, BandParam param)
{
    return "b" + juce::String(band + 1) + kBandSuffixes[static_cast<std::size_t>(param)];
}

juce::String globalParamId(GlobalParam param)
{
    return kGlobalIds[static_cast<std::size_t>(param)];
}

const juce::StringArray& shapeNames()
{
    static const juce::StringArray names{"Bell",  "Low Shelf", "Low Cut",    "High Shelf", "High Cut",
                                         "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt",  "All Pass"};
    return names;
}

const juce::StringArray& slopeNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray n;
        for (int s : animalaudio::dsp::kFilterSlopes)
            n.add(juce::String(s) + " dB/oct");
        return n;
    }();
    return names;
}

const juce::StringArray& placementNames()
{
    static const juce::StringArray names{"Stereo", "Left", "Right", "Mid", "Side"};
    return names;
}

const juce::StringArray& resolutionNames()
{
    static const juce::StringArray names{"Low", "Medium", "High"};
    return names;
}

float defaultBandFrequency(int band)
{
    return 50.0f * std::pow(16000.0f / 50.0f, static_cast<float>(band) / static_cast<float>(dsp::kNumBands - 1));
}

juce::String formatHz(float hz)
{
    if (hz >= 1000.0f)
        return juce::String(hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String(hz, hz < 100.0f ? 1 : 0) + " Hz";
}

juce::String formatDb(float db)
{
    return (db > 0.04f ? "+" : "") + juce::String(db, 1) + " dB";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(dbParameter(globalParamId(GlobalParam::output), "Output Level", -36.0f, 36.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        pid(globalParamId(GlobalParam::pan)), "Output Pan", juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int)
            {
                const int p = juce::roundToInt(v);
                return p == 0 ? juce::String("C") : juce::String(std::abs(p)) + (p < 0 ? " L" : " R");
            })));
    layout.add(
        std::make_unique<juce::AudioParameterFloat>(pid(globalParamId(GlobalParam::gainScale)), "Gain Scale",
                                                    juce::NormalisableRange<float>(0.0f, 200.0f, 1.0f), 100.0f,
                                                    juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                                                        [](float v, int) { return formatPercent(v); })));
    layout.add(boolParameter(globalParamId(GlobalParam::autoGain), "Auto Gain", false));
    layout.add(boolParameter(globalParamId(GlobalParam::phaseInvert), "Phase Invert", false));
    layout.add(boolParameter(globalParamId(GlobalParam::bypass), "Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(pid(globalParamId(GlobalParam::spectralResolution)),
                                                            "Spectral Resolution", resolutionNames(), 1));

    for (int b = 0; b < dsp::kNumBands; ++b)
    {
        const auto name = [b](BandParam p)
        { return "Band " + juce::String(b + 1) + " " + kBandNames[static_cast<std::size_t>(p)]; };
        const auto id = [b](BandParam p) { return bandParamId(b, p); };

        layout.add(boolParameter(id(BandParam::used), name(BandParam::used), false));
        layout.add(boolParameter(id(BandParam::enabled), name(BandParam::enabled), true));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            pid(id(BandParam::frequency)), name(BandParam::frequency), logRange(dsp::kMinHz, dsp::kMaxHz),
            defaultBandFrequency(b),
            juce::AudioParameterFloatAttributes()
                .withLabel("Hz")
                .withStringFromValueFunction([](float v, int) { return formatHz(v); })
                .withValueFromStringFunction(parseHz)));
        layout.add(dbParameter(id(BandParam::gain), name(BandParam::gain), -dsp::kMaxGainDb, dsp::kMaxGainDb, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            pid(id(BandParam::q)), name(BandParam::q), logRange(dsp::kMinQ, dsp::kMaxQ), 1.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                [](float v, int) { return juce::String(v, v < 1.0f ? 3 : 2); })));
        layout.add(std::make_unique<juce::AudioParameterChoice>(pid(id(BandParam::shape)), name(BandParam::shape),
                                                                shapeNames(), 0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(pid(id(BandParam::slope)), name(BandParam::slope),
                                                                slopeNames(), 1));
        layout.add(std::make_unique<juce::AudioParameterChoice>(pid(id(BandParam::placement)),
                                                                name(BandParam::placement), placementNames(), 0));
        layout.add(
            dbParameter(id(BandParam::range), name(BandParam::range), -dsp::kMaxRangeDb, dsp::kMaxRangeDb, 0.0f));
        layout.add(boolParameter(id(BandParam::dynamics), name(BandParam::dynamics), true));
        layout.add(dbParameter(id(BandParam::threshold), name(BandParam::threshold), -80.0f, 0.0f, -30.0f));
        layout.add(boolParameter(id(BandParam::autoThreshold), name(BandParam::autoThreshold), true));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            pid(id(BandParam::attack)), name(BandParam::attack), logRange(0.5f, 500.0f), 10.0f,
            juce::AudioParameterFloatAttributes().withLabel("ms").withStringFromValueFunction(
                [](float v, int) { return formatMs(v); })));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            pid(id(BandParam::release)), name(BandParam::release), logRange(5.0f, 5000.0f), 150.0f,
            juce::AudioParameterFloatAttributes().withLabel("ms").withStringFromValueFunction(
                [](float v, int) { return formatMs(v); })));
        layout.add(boolParameter(id(BandParam::sidechain), name(BandParam::sidechain), false));
        layout.add(boolParameter(id(BandParam::spectral), name(BandParam::spectral), false));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            pid(id(BandParam::density)), name(BandParam::density), juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
            50.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int)
                                                                              { return formatPercent(v); })));
        layout.add(boolParameter(id(BandParam::spectralTilt), name(BandParam::spectralTilt), true));
    }
    return layout;
}

ParameterSet::ParameterSet(juce::AudioProcessorValueTreeState& apvts)
{
    for (int b = 0; b < dsp::kNumBands; ++b)
        for (int p = 0; p < kNumBandParams; ++p)
        {
            const auto id = bandParamId(b, static_cast<BandParam>(p));
            bandParams[static_cast<std::size_t>(b)][static_cast<std::size_t>(p)] = apvts.getParameter(id);
            bandValues[static_cast<std::size_t>(b)][static_cast<std::size_t>(p)] = apvts.getRawParameterValue(id);
            jassert(bandParams[static_cast<std::size_t>(b)][static_cast<std::size_t>(p)] != nullptr);
        }
    for (int p = 0; p < kNumGlobalParams; ++p)
    {
        const auto id = globalParamId(static_cast<GlobalParam>(p));
        globalParams[static_cast<std::size_t>(p)] = apvts.getParameter(id);
        globalValues[static_cast<std::size_t>(p)] = apvts.getRawParameterValue(id);
        jassert(globalParams[static_cast<std::size_t>(p)] != nullptr);
    }
}

juce::RangedAudioParameter& ParameterSet::band(int index, BandParam param) const noexcept
{
    return *bandParams[static_cast<std::size_t>(index)][static_cast<std::size_t>(param)];
}

juce::RangedAudioParameter& ParameterSet::global(GlobalParam param) const noexcept
{
    return *globalParams[static_cast<std::size_t>(param)];
}

float ParameterSet::value(int index, BandParam param) const noexcept
{
    return bandValues[static_cast<std::size_t>(index)][static_cast<std::size_t>(param)]->load(
        std::memory_order_relaxed);
}

dsp::Settings ParameterSet::read() const noexcept
{
    dsp::Settings s;
    const auto flag = [](float v) { return v >= 0.5f; };
    const auto choice = [](float v, int count) { return juce::jlimit(0, count - 1, juce::roundToInt(v)); };
    for (int b = 0; b < dsp::kNumBands; ++b)
    {
        const auto v = [&](BandParam p) { return value(b, p); };
        auto& band = s.bands[static_cast<std::size_t>(b)];
        band.used = flag(v(BandParam::used));
        band.enabled = flag(v(BandParam::enabled));
        band.frequency = v(BandParam::frequency);
        band.gainDb = v(BandParam::gain);
        band.q = v(BandParam::q);
        band.shape = static_cast<dsp::FilterShape>(choice(v(BandParam::shape), animalaudio::dsp::kNumFilterShapes));
        band.slope = animalaudio::dsp::kFilterSlopes[static_cast<std::size_t>(
            choice(v(BandParam::slope), static_cast<int>(animalaudio::dsp::kFilterSlopes.size())))];
        band.placement = static_cast<dsp::Placement>(choice(v(BandParam::placement), dsp::kNumPlacements));
        band.rangeDb = v(BandParam::range);
        band.dynamicsEnabled = flag(v(BandParam::dynamics));
        band.thresholdDb = v(BandParam::threshold);
        band.autoThreshold = flag(v(BandParam::autoThreshold));
        band.attackMs = v(BandParam::attack);
        band.releaseMs = v(BandParam::release);
        band.externalSidechain = flag(v(BandParam::sidechain));
        band.spectral = flag(v(BandParam::spectral));
        band.spectralDensity = v(BandParam::density) / 100.0f;
        band.spectralTilt = flag(v(BandParam::spectralTilt));
    }
    const auto g = [&](GlobalParam p)
    { return globalValues[static_cast<std::size_t>(p)]->load(std::memory_order_relaxed); };
    s.global.outputDb = g(GlobalParam::output);
    s.global.pan = g(GlobalParam::pan) / 100.0f;
    s.global.gainScale = g(GlobalParam::gainScale) / 100.0f;
    s.global.autoGain = flag(g(GlobalParam::autoGain));
    s.global.phaseInvert = flag(g(GlobalParam::phaseInvert));
    s.global.bypass = flag(g(GlobalParam::bypass));
    s.global.resolution = static_cast<dsp::SpectralResolution>(choice(g(GlobalParam::spectralResolution), 3));
    return s;
}
} // namespace quail
