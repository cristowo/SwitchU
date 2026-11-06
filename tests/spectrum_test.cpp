#include "core/SpectrumAnalyzer.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <vector>

namespace {

constexpr int kFrames = 4096;

std::vector<std::int16_t> tone(float hz, float amplitude, int rate) {
    std::vector<std::int16_t> samples(kFrames * 2);
    for (int i = 0; i < kFrames; ++i) {
        const auto value = static_cast<std::int16_t>(
            32767.f * amplitude * std::sin(2.f * 3.14159265f * hz * i / rate));
        samples[i * 2] = samples[i * 2 + 1] = value;
    }
    return samples;
}

} // namespace

int main() {
    float out[SpectrumAnalyzer::kBands];

    // Before any audio, and with silence: flat, so nothing moves on its own.
    SpectrumAnalyzer silent;
    silent.read(out);
    assert(*std::max_element(std::begin(out), std::end(out)) == 0.f);
    const std::vector<std::int16_t> nothing(kFrames * 2);
    silent.analyze(nothing.data(), kFrames, 48000);
    silent.read(out);
    assert(*std::max_element(std::begin(out), std::end(out)) == 0.f);

    // A tone lifts its own band only, at the mixer's rate and the console's.
    for (const int rate : {44100, 48000}) {
        SpectrumAnalyzer analyzer;
        const auto samples = tone(1000.f, 0.5f, rate);
        analyzer.analyze(samples.data(), kFrames, rate);
        analyzer.read(out);
        const auto top = std::max_element(std::begin(out), std::end(out));
        const int band = static_cast<int>(top - std::begin(out));
        assert(band >= 14 && band <= 19); // 1 kHz, log spaced from 50 Hz to 16 kHz
        assert(*top > 0.8f);
        assert(out[2] < 0.2f);
        assert(out[30] < 0.2f);
    }

    // Different tones draw different bars: they follow the sound.
    SpectrumAnalyzer low, high;
    float lowOut[SpectrumAnalyzer::kBands], highOut[SpectrumAnalyzer::kBands];
    const auto lowTone = tone(150.f, 0.5f, 48000);
    const auto highTone = tone(6000.f, 0.5f, 48000);
    low.analyze(lowTone.data(), kFrames, 48000);
    high.analyze(highTone.data(), kFrames, 48000);
    low.read(lowOut);
    high.read(highOut);
    assert(std::max_element(std::begin(lowOut), std::end(lowOut)) - std::begin(lowOut)
           < std::max_element(std::begin(highOut), std::end(highOut)) - std::begin(highOut));

    // Volume does not decide their height.
    SpectrumAnalyzer quiet;
    const auto quietTone = tone(1000.f, 0.02f, 48000);
    quiet.analyze(quietTone.data(), kFrames, 48000);
    quiet.read(out);
    assert(*std::max_element(std::begin(out), std::end(out)) > 0.8f);
    return 0;
}
