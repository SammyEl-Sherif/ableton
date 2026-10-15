#pragma once

#include <animalaudio/dsp/TempoSync.h>

#include <juce_audio_basics/juce_audio_basics.h>

namespace animalaudio::core
{
/** Reads what the host says about the transport. Audio-thread safe; a null playhead is fine. */
inline dsp::TransportInfo readTransport(juce::AudioPlayHead* playHead)
{
    dsp::TransportInfo transport;
    if (playHead == nullptr)
        return transport;

    if (const auto position = playHead->getPosition())
    {
        transport.playing = position->getIsPlaying();
        if (const auto bpm = position->getBpm())
        {
            transport.hasBpm = true;
            transport.bpm = *bpm;
        }
        if (const auto ppq = position->getPpqPosition())
        {
            transport.hasPpq = true;
            transport.ppq = *ppq;
        }
    }
    return transport;
}
} // namespace animalaudio::core
