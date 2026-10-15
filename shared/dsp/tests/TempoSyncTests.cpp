#include <animalaudio/dsp/TempoSync.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace animalaudio::dsp;
using Catch::Approx;

TEST_CASE("Division lengths in beats")
{
    REQUIRE(beatsPerCycle(Division::Eighth) == 0.5);
    REQUIRE(beatsPerCycle(Division::Quarter) == 1.0);
    REQUIRE(beatsPerCycle(Division::Half) == 2.0);
    REQUIRE(beatsPerCycle(Division::Whole) == 4.0);
}

TEST_CASE("Samples per cycle at 120 BPM / 48 kHz")
{
    // one beat = 0.5 s = 24000 samples
    REQUIRE(samplesPerCycle(120.0, 48000.0, Division::Eighth) == Approx(12000.0));
    REQUIRE(samplesPerCycle(120.0, 48000.0, Division::Quarter) == Approx(24000.0));
    REQUIRE(samplesPerCycle(120.0, 48000.0, Division::Half) == Approx(48000.0));
    REQUIRE(samplesPerCycle(120.0, 48000.0, Division::Whole) == Approx(96000.0));
}

TEST_CASE("Samples per cycle scales inversely with BPM")
{
    REQUIRE(samplesPerCycle(60.0, 44100.0, Division::Quarter) == Approx(44100.0));
    REQUIRE(samplesPerCycle(240.0, 44100.0, Division::Quarter) == Approx(11025.0));
}

TEST_CASE("phaseFromPpq wraps per division")
{
    REQUIRE(phaseFromPpq(0.0, Division::Quarter) == Approx(0.0));
    REQUIRE(phaseFromPpq(0.25, Division::Quarter) == Approx(0.25));
    REQUIRE(phaseFromPpq(1.0, Division::Quarter) == Approx(0.0));
    REQUIRE(phaseFromPpq(1.0, Division::Eighth) == Approx(0.0));
    REQUIRE(phaseFromPpq(0.25, Division::Eighth) == Approx(0.5));
    REQUIRE(phaseFromPpq(1.0, Division::Whole) == Approx(0.25));
    REQUIRE(phaseFromPpq(3.0, Division::Half) == Approx(0.5));
    REQUIRE(phaseFromPpq(-0.5, Division::Quarter) == Approx(0.5));
}

TEST_CASE("divisionFromIndex clamps")
{
    REQUIRE(divisionFromIndex(-3) == Division::Eighth);
    REQUIRE(divisionFromIndex(99) == Division::Whole);
    REQUIRE(divisionFromIndex(2) == Division::Half);
}

TEST_CASE("PhaseTracker locks to the host while playing")
{
    PhaseTracker tracker;
    tracker.prepare(48000.0);

    TransportInfo t;
    t.playing = true;
    t.hasBpm = true;
    t.bpm = 120.0;
    t.hasPpq = true;
    t.ppq = 2.5;

    const auto b = tracker.beginBlock(t, Division::Quarter, 90.0, 512);
    REQUIRE(b.source == SyncSource::Host);
    REQUIRE(b.startPhase == Approx(0.5));
    REQUIRE(b.increment == Approx(1.0 / 24000.0));
}

TEST_CASE("PhaseTracker free-runs with the fallback BPM when the host gives nothing")
{
    PhaseTracker tracker;
    tracker.prepare(48000.0);

    TransportInfo t; // not playing, no bpm, no ppq
    const auto first = tracker.beginBlock(t, Division::Quarter, 120.0, 12000);
    REQUIRE(first.source == SyncSource::FreeRun);
    REQUIRE(first.startPhase == Approx(0.0));
    REQUIRE(first.increment == Approx(1.0 / 24000.0));

    const auto second = tracker.beginBlock(t, Division::Quarter, 120.0, 12000);
    REQUIRE(second.startPhase == Approx(0.5)); // half a beat later
}

TEST_CASE("PhaseTracker free-runs at the host BPM when stopped")
{
    PhaseTracker tracker;
    tracker.prepare(48000.0);

    TransportInfo t;
    t.hasBpm = true;
    t.bpm = 60.0;
    const auto b = tracker.beginBlock(t, Division::Quarter, 120.0, 100);
    REQUIRE(b.source == SyncSource::FreeRun);
    REQUIRE(b.increment == Approx(1.0 / 48000.0));
}

TEST_CASE("PhaseTracker continues from the host phase when playback stops")
{
    PhaseTracker tracker;
    tracker.prepare(48000.0);

    TransportInfo playing;
    playing.playing = true;
    playing.hasPpq = true;
    playing.ppq = 0.0;
    tracker.beginBlock(playing, Division::Quarter, 120.0, 12000);

    TransportInfo stopped;
    const auto b = tracker.beginBlock(stopped, Division::Quarter, 120.0, 100);
    REQUIRE(b.startPhase == Approx(0.5));
}
