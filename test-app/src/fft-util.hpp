#pragma once
#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>

namespace nexrx {

// Simple Radix-2 In-place FFT
// n must be a power of 2
inline void fftInPlace(std::vector<std::complex<double>>& a, bool invert) {
    size_t n = a.size();

    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }

    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = 2.0 * M_PI / len * (invert ? 1 : -1);
        std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (size_t j = 0; j < len / 2; j++) {
                std::complex<double> u = a[i+j], v = a[i+j+len/2] * w;
                a[i+j] = u + v;
                a[i+j+len/2] = u - v;
                w *= wlen;
            }
        }
    }

    if (invert) {
        for (auto& x : a) x /= (double)n;
    }
}

} // namespace nexrx
