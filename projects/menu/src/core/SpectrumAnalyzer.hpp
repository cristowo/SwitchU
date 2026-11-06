#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

// Levels per band for the lock screen's bars, from 16-bit stereo audio. Every
// mix is cut into 1024-sample slices; the reader gets the slice being heard now.
// The FFT and slicing come from HomeNX's bars.
class SpectrumAnalyzer {
public:
    static constexpr int kBands = 32;

    void analyze(const std::int16_t* stereo, int frames, int rate) {
        if (rate != m_edgesRate)
            prepare(rate);
        std::array<std::array<float, kBands>, kSlices> out{};
        int count = 0;
        for (int start = 0; start + kFft <= frames && count < kSlices; start += kFft, ++count) {
            for (int i = 0; i < kFft; ++i)
                m_buffer[i] = {(stereo[(start + i) * 2] + stereo[(start + i) * 2 + 1])
                                   * (0.5f / 32768.f) * m_window[i], 0.f};
            fft(m_buffer);
            levels(out[static_cast<std::size_t>(count)]);
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        m_slices = out;
        m_count = count;
        m_rate = rate;
        m_at = std::chrono::steady_clock::now();
    }

    void read(float* out) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_count == 0) {
            std::fill(out, out + kBands, 0.f);
            return;
        }
        const float ms = std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - m_at).count();
        const int slice = std::clamp(static_cast<int>(ms / (1000.f * kFft / m_rate)), 0, m_count - 1);
        const auto& levels = m_slices[static_cast<std::size_t>(slice)];
        std::copy(levels.begin(), levels.end(), out);
    }

private:
    static constexpr int kFft = 1024;
    static constexpr int kSlices = 4;
    // The bars show the song's shape, not its volume: levels are relative to its
    // recent peak, over this many dB. Below the floor is silence.
    static constexpr float kRangeDb = 36.f;
    static constexpr float kSilenceDb = -60.f;
    static constexpr float kPeakDecayDb = 0.05f; // per slice
    static constexpr float kTiltDb = 0.75f;      // per band: music has less treble

    void prepare(int rate) {
        m_edgesRate = rate;
        m_buffer.resize(kFft);
        m_window.resize(kFft);
        for (int i = 0; i < kFft; ++i)
            m_window[i] = 0.5f - 0.5f * std::cos(2.f * 3.14159265f * i / (kFft - 1));
        for (int b = 0; b <= kBands; ++b) { // each band's first bin, log spaced
            const float hz = 50.f * std::pow(16000.f / 50.f, static_cast<float>(b) / kBands);
            m_edges[b] = std::max(b == 0 ? 1 : m_edges[b - 1] + 1, static_cast<int>(hz * kFft / rate));
        }
    }

    static void fft(std::vector<std::complex<float>>& a) { // in place, radix 2
        const std::size_t n = a.size();
        for (std::size_t i = 1, j = 0; i < n; ++i) {
            std::size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(a[i], a[j]);
        }
        for (std::size_t len = 2; len <= n; len <<= 1) {
            const float angle = -2.f * 3.14159265f / static_cast<float>(len);
            const std::complex<float> step(std::cos(angle), std::sin(angle));
            for (std::size_t i = 0; i < n; i += len) {
                std::complex<float> w(1.f, 0.f);
                for (std::size_t k = 0; k < len / 2; ++k) {
                    const std::complex<float> u = a[i + k], v = a[i + k + len / 2] * w;
                    a[i + k] = u + v;
                    a[i + k + len / 2] = u - v;
                    w *= step;
                }
            }
        }
    }

    void levels(std::array<float, kBands>& out) {
        std::array<float, kBands> db{};
        float loudest = -200.f;
        for (int b = 0; b < kBands; ++b) {
            float peak = 0.f;
            for (int i = m_edges[b]; i < std::min(m_edges[b + 1], kFft / 2); ++i)
                peak = std::max(peak, std::abs(m_buffer[i]));
            db[b] = 20.f * std::log10(peak / (kFft / 4.f) + 1e-6f) + kTiltDb * b;
            loudest = std::max(loudest, db[b]);
        }
        m_peakDb = std::max(loudest, m_peakDb - kPeakDecayDb);
        if (m_peakDb < kSilenceDb) {
            out.fill(0.f);
            return;
        }
        // Squared, so a band has to stand out to rise: flat noise stays low.
        for (int b = 0; b < kBands; ++b) {
            const float level = std::clamp((db[b] - m_peakDb + kRangeDb) / kRangeDb, 0.f, 1.f);
            out[b] = level * level;
        }
    }

    // Writer's thread only.
    std::vector<std::complex<float>> m_buffer;
    std::vector<float> m_window;
    std::array<int, kBands + 1> m_edges{};
    int m_edgesRate = 0;
    float m_peakDb = -200.f;

    mutable std::mutex m_mutex;
    std::array<std::array<float, kBands>, kSlices> m_slices{};
    int m_count = 0;
    int m_rate = 44100;
    std::chrono::steady_clock::time_point m_at{};
};
