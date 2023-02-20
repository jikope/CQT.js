#ifndef CQT_H
#define CQT_H
// Compiler attributes
#define WASM_IMPORT extern __attribute__((__nothrow__))
#define WASM_EXPORT extern __attribute__((__visibility__("default")))
#define DECLARE_ALIGNED(n) __attribute__((__aligned__(n)))
typedef float f32x4 __attribute__((__vector_size__(16), __aligned__(16)));

// Maths
#define M_PI 3.14159265358979323846
#define M_LN2 0.693147180559945309417
WASM_IMPORT double sin(double);
WASM_IMPORT double cos(double);
WASM_IMPORT double log(double);
WASM_IMPORT double power(double, double);
WASM_IMPORT double exp(double);
WASM_IMPORT double ceil(double);
WASM_IMPORT double floor(double);
WASM_IMPORT float sqrtf(float);

// CQT Related
#define BINS_PER_OCTAVE 24
#define CQT_MAX_BIN BINS_PER_OCTAVE * 10
#define MAX_SAMPLES 20000

typedef struct DSP {
    int   fft_size;
    int   n_bins;
    int   bins_per_octave;

    // Time-domain Kernel
    float time_kernel_real[CQT_MAX_BIN][MAX_SAMPLES];
    float time_kernel_imag[CQT_MAX_BIN][MAX_SAMPLES];
    int   starts[CQT_MAX_BIN];
    int   lengths[CQT_MAX_BIN];

    // Pipes
    float input[MAX_SAMPLES];
    float output[CQT_MAX_BIN];
} DSP;

#endif /* CQT_H */
