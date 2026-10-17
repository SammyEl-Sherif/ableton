#pragma once

#include "QuailProcessor.h"

namespace quail::ui
{
/** Message-thread band editing on top of the parameters, so every change reaches the host as a gesture. */
class BandActions
{
public:
    explicit BandActions(QuailProcessor& processor);

    QuailProcessor& getProcessor() noexcept { return processor; }

    float get(int band, BandParam param) const noexcept;
    juce::String text(int band, BandParam param) const;
    bool isUsed(int band) const noexcept { return get(band, BandParam::used) > 0.5f; }
    int numUsed() const noexcept;

    /** One complete gesture. */
    void set(int band, BandParam param, float value);
    void begin(int band, BandParam param);
    /** Inside begin()/end(). */
    void move(int band, BandParam param, float value);
    void end(int band, BandParam param);

    /** Lowest free band, or -1 when all 24 are used. */
    int firstFree() const noexcept;
    /** Turns a free band on at `hz` / `gainDb` with defaults for everything else; returns it (-1 if none is free). */
    int create(float hz, float gainDb, dsp::FilterShape shape);
    /** Shape a double-click at `hz` makes: cuts at the extremes, a bell in between. */
    static dsp::FilterShape shapeForNewBand(float hz) noexcept;
    /** Frees the band and puts all its parameters back to their defaults. */
    void remove(int band);
    void removeAll();

private:
    QuailProcessor& processor;
};
} // namespace quail::ui
