#include <animalaudio/dsp/SampleFifo.h>

#include <catch2/catch_test_macros.hpp>

#include <thread>
#include <vector>

using animalaudio::dsp::SampleFifo;

TEST_CASE("SampleFifo pushes the channel average and pops in order")
{
    SampleFifo<16> fifo;
    const float left[] = {1.0f, 2.0f, 3.0f};
    const float right[] = {3.0f, 4.0f, 5.0f};
    const float* channels[] = {left, right};
    REQUIRE(fifo.pushMixdown(channels, 2, 3) == 3);
    REQUIRE(fifo.getNumReady() == 3);

    float out[8] = {};
    REQUIRE(fifo.pop(out, 8) == 3);
    REQUIRE(out[0] == 2.0f);
    REQUIRE(out[1] == 3.0f);
    REQUIRE(out[2] == 4.0f);
    REQUIRE(fifo.getNumReady() == 0);
    REQUIRE(fifo.pop(out, 8) == 0);
}

TEST_CASE("SampleFifo drops what does not fit instead of overwriting")
{
    SampleFifo<8> fifo;
    std::vector<float> ramp(12);
    for (std::size_t i = 0; i < ramp.size(); ++i)
        ramp[i] = static_cast<float>(i);
    const float* channels[] = {ramp.data()};
    REQUIRE(fifo.pushMixdown(channels, 1, 12) == 8);
    REQUIRE(fifo.pushMixdown(channels, 1, 1) == 0);

    float out[8] = {};
    REQUIRE(fifo.pop(out, 8) == 8);
    for (int i = 0; i < 8; ++i)
        REQUIRE(out[i] == static_cast<float>(i));
}

TEST_CASE("SampleFifo wraps around indefinitely")
{
    SampleFifo<8> fifo;
    float next = 0.0f;
    float expected = 0.0f;
    for (int round = 0; round < 1000; ++round)
    {
        float block[5];
        for (auto& x : block)
            x = next++;
        const float* channels[] = {block};
        REQUIRE(fifo.pushMixdown(channels, 1, 5) == 5);
        float out[5];
        REQUIRE(fifo.pop(out, 5) == 5);
        for (float x : out)
            REQUIRE(x == expected++);
    }
}

TEST_CASE("SampleFifo delivers an ordered stream across threads")
{
    SampleFifo<256> fifo;
    constexpr int total = 200000;

    std::thread producer(
        [&]
        {
            float block[32];
            int value = 0;
            while (value < total)
            {
                for (int i = 0; i < 32; ++i)
                    block[i] = static_cast<float>(value + i);
                const float* channels[] = {block};
                value += fifo.pushMixdown(channels, 1, 32);
                std::this_thread::yield();
            }
        });

    float last = -1.0f;
    int received = 0;
    float out[64];
    while (received < total)
    {
        const int n = fifo.pop(out, 64);
        for (int i = 0; i < n; ++i)
        {
            REQUIRE(out[i] == last + 1.0f);
            last = out[i];
        }
        received += n;
    }
    producer.join();
    REQUIRE(last == static_cast<float>(total - 1));
}
