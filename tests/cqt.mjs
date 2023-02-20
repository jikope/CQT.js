var DSP = {
    dsp_init: async function(sr, bins_per_octave, n_bins, f_min) {
        var CQT = {};

        await WebAssembly.instantiateStreaming(fetch("./cqt.wasm"), {
            "env": {
                sin: Math.sin,
                cos: Math.cos,
                log: Math.log,
                power: Math.pow,
                exp: Math.exp,
                ceil: Math.ceil,
                sqrtf: Math.sqrt,
            }
        }).then((w) => {
            var instance = w.instance.exports;
            const fft_size = instance.init(sr, bins_per_octave, n_bins, f_min);
            var buffer = instance.memory.buffer;

            CQT.fft_size = fft_size;
            CQT.input = new Float32Array(buffer, instance.get_input_array(), fft_size);
            CQT.output = new Float32Array(buffer, instance.get_output_array(), n_bins);

            // Exported functions 
            CQT.calc_cqt = instance.calc_cqt;
            CQT.calc_cqt_simd = instance.calc_cqt_simd;
            CQT.detect_silence = instance.detect_silence;
        });

        return CQT;
    }
};

export { DSP };
export default DSP;
