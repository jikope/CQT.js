#include "cqt.h"

#define SIMDE_ENABLE_NATIVE_ALIASES
#undef SIMDE_X86_SSE2_NATIVE
#include "simde/wasm/simd128.h"

static DECLARE_ALIGNED(1024) DSP dsp;

/* INTERNAL FUNCTION */
double hamming_window(int N, int n) {
    // hamming(len)= 0.46-0.54*cos(2*pi*(0:len-1)’/len)
    static double PI_2 = 2 * M_PI;
    return 0.46 - 0.54 * cos(PI_2 * n / N);
}

void init_time_domain_kernel(int fs, float f_min) {
    // References
    // - https://www.researchgate.net/publication/230554907_An_efficient_algorithm_for_the_calculation_of_a_constant_Q_transform
    //   An efficient algorithm for the calculation of a constant Q transform
    // 
    // - https://arxiv.org/pdf/1912.12055.pdf
    //   nnAudio: An on-the-fly GPU Audio to Spectrogram Conversion Toolbox Using 1D Convolutional Neural Networks
    // 
    // - https://doc.ml.tu-berlin.de/bbci/material/publications/Bla_constQ.pdf
    //   The Constant Q Transform [Benjamin Blankertz] 

    /** Some important formulaes
     * k = bins
     * j = imaginary
     *
     * - Q 
     *   Q = 1 / ( 2 ^ (1.0 / bins_per_octave)) - 1);
     *
     * - Find frequency value with index of n_bins
     *   fk(k) = f_min . 2 ^ (k / bins_per_octae)
     *
     * - Window size of some frequency filter
     *   Nk(k) = Q . (fs / fk(k))
     *
     * - Direct CQT calculation (Slow)
     *   X      = input signal
     *   win    = window function (Hann, Hamming)
     *   CQT(k) = (1 / Nk) . sum(n < Nk) { X[n] . win(Nk, n) . (e ^ -2 * PI * j * n * Q / Nk) } 
     *
     * - Precalculate Time-Domain Kernel [1], because modern computer's memory able to store
     *   a lot of data.
     *   T* = (1 / Nk) . win(Nk, n) . (e ^ 2 * PI * j * n * Q / Nk) if n < Nk
     *   T* = 0                                                     if n >= Nk
     **/

    const int bins_per_octave = dsp.bins_per_octave;
    const int n_bins = dsp.n_bins;
    /* const double f_min = 32.71f; */
    /* const double f_min = 29.14f; */
    int fftSize = dsp.fft_size;

    const double Q = 1 / ( power(2, 1.0 / bins_per_octave) - 1);
    const double alpha = power(2, (1.0f / bins_per_octave)) - 1;

    // Time-Domain kernel
    for (int k = 0; k < n_bins; k++) {
        double  fk  = f_min * power(2, (double)k / bins_per_octave);
        int len = ceil(Q * fs / (fk + 0 / alpha) );

        if (len > fftSize)
            len = fftSize;

        dsp.lengths[k] = len;
        float signal_real[len];
        float signal_imag[len];

        int start;
        // Center win len
        if (len % 2 == 1) {
            start = (int)(ceil(fftSize / 2.0 - len / 2.0)) - 1;
        } else {
            start = (int)(ceil(fftSize / 2.0 - len / 2.0));
        }

        // Signal model
        // sig = window * np.exp(np.r_[-l // 2 : l // 2] * 1j * 2 * np.pi * freq / fs) / l
        dsp.starts[k] = start;

        int s = -len / 2;

        // Creating signal
        for (int i = 0; i < len; i++) {
            double W = 2 * M_PI * s * fk / fs;
            double win = hamming_window(len, i);
            signal_real[i] = win * cos(W) / len;
            signal_imag[i] = win * sin(W) / len;
            s++;
        }

        s = 0;
        int end = start + len;

        // Place signal in center of kernel
        for (int i = 0; i < fftSize; i++) {
            if (i < start || i > end) {
                dsp.time_kernel_real[k][i] = 0;
                dsp.time_kernel_imag[k][i] = 0;
                continue;
            }
            dsp.time_kernel_real[k][i] = signal_real[s];
            dsp.time_kernel_imag[k][i] = signal_imag[s];
            s++;
        }
    }
}

/* EXPORTED FUNCTIONS */
WASM_EXPORT float* get_input_array() {
    return dsp.input;
}

WASM_EXPORT float* get_output_array() {
    return dsp.output;
}

WASM_EXPORT int detect_silence(float threshold) {
    for (int x = 0; x < dsp.fft_size; x++) {
        if (dsp.input[x] * dsp.input[x] > threshold) 
            return 0;
    }

    return 1;
}
/**
 * SIMD Resources
 * - https://thewolfsound.com/simd-in-dsp/
 * - https://github.com/WebAssembly/simd/blob/main/proposals/simd/ImplementationStatus.md
 */

WASM_EXPORT void calc_cqt() {
    const float* inputs = dsp.input;

    for (int k = 0; k < dsp.n_bins; k++) {
        f32x4 sum_realx4 = simde_wasm_f32x4_make(0, 0, 0, 0);
        f32x4 sum_imagx4 = simde_wasm_f32x4_make(0, 0, 0, 0);
        const float* kernel_real = (dsp.time_kernel_real[k]);
        const float* kernel_imag = (dsp.time_kernel_imag[k]);

        for (int n = dsp.starts[k]; n < dsp.starts[k] + dsp.lengths[k]; n += 4) {
            f32x4 input = simde_wasm_v128_load(inputs + n);
            f32x4 k_real = simde_wasm_v128_load(kernel_real + n);
            f32x4 k_imag = simde_wasm_v128_load(kernel_imag + n);

            sum_realx4 = simde_wasm_f32x4_add(sum_realx4, simde_wasm_f32x4_mul(input, k_real));
            sum_imagx4 = simde_wasm_f32x4_sub(sum_imagx4, simde_wasm_f32x4_mul(input, k_imag));
        }

        float sum_real = sum_realx4[0] + sum_realx4[1] + sum_realx4[2] +sum_realx4[3];
        float sum_imag = sum_imagx4[0] + sum_imagx4[1] + sum_imagx4[2] +sum_imagx4[3];
        // Magnitude
        dsp.output[k] = sqrtf(sum_real * sum_real +
                              sum_imag * sum_imag );
        // Scaling
        // https://dsp.stackexchange.com/a/32122
        // 10 is enough ?
        dsp.output[k] = 10 * log(dsp.output[k] / 32768);
    }
}

WASM_EXPORT int init(int rate, int bins_per_octave, int n_bins, float f_min) {
    /* dsp.fft_size = 16384; */
    dsp.fft_size = power(2, ceil(log(44100 * 0.33) / M_LN2));
    dsp.n_bins = n_bins;
    dsp.bins_per_octave = bins_per_octave;

    init_time_domain_kernel(rate, f_min);

    return dsp.fft_size;
}
