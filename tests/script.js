import DSP from "./cqt.mjs";

// Global vars
// CQT 
const FPS               = 60;
const BINS_PER_OCTAVE   = 24;
const n_bins            = BINS_PER_OCTAVE * 8
const f_min             = 30.87; // # B0 so index 1 start C1

// HTML ELEMENTS
let container           = null;
let visualizer          = null;
let notes               = null;
let audioElement        = null;

// Canvas contexts
let visCtx              = null;
let notesCtx            = null;

// Webaudio
let audioCtx            = null;
let analyzer            = null;
let dsp                 = null;

async function init() {
    const btn_div = document.getElementById("buttons");

    // Create some buttons
    const btn_cqt = document.createElement("button");
    btn_cqt.innerText = "Draw spectrum";
    btn_cqt.onclick = () => {
        audioCtx.resume();
        audioElement.play();
        render();
    };
    btn_div.appendChild(btn_cqt)

    const btn_benchmark = document.createElement("button");
    btn_benchmark.innerText = "Benchmark";
    btn_benchmark.onclick = benchmark;
    btn_div.appendChild(btn_benchmark)

    // Grab canvas and other elements
    notes        = document.querySelector("#notes");
    audioElement = document.querySelector("#player");
    container    = document.querySelector("#container");
    visualizer   = document.querySelector("#visualizer");

    // Visualizer canvas setup
    visualizer.width = window.innerWidth;
    visualizer.height = window.innerHeight;
    visCtx = visualizer.getContext('2d');
    visCtx.font = "normal 24px serif";

    // Notes canvas setup
    notes.width = container.clientWidth;
    notes.height = 50;
    notesCtx = notes.getContext('2d');

    // Web Audio context
    audioCtx = new AudioContext();
    analyzer = audioCtx.createAnalyser();

    // DSP Wasm module
    dsp = await DSP.dsp_init(audioCtx.sampleRate, BINS_PER_OCTAVE, n_bins, f_min);
    analyzer.fftSize = dsp.fft_size;

    const track = audioCtx.createMediaElementSource(audioElement);
    track.connect(analyzer);
    analyzer.connect(audioCtx.destination);

    // draw_notes();
    draw_chromatic();
}

function render() {
    setTimeout(() => {
        requestAnimationFrame(render);
    }, 1000 / FPS);

    // Fill CQT output buffer
    analyzer.getFloatTimeDomainData(dsp.input);

    if (dsp.detect_silence(1e-10)) {
        return;
    }

    let t0 = performance.now()
    dsp.calc_cqt();
    let t1 = performance.now()
    let cqt_ms = t1 - t0;

    visCtx.fillStyle = "rgba(255, 205, 0, 1)";
    t0 = performance.now()
    visCtx.clearRect(0, 0, visualizer.width, visualizer.height);
    t1 = performance.now()
    let clear_ms = t1 - t0;

    var y = visualizer.height;
    var x_interval = (visualizer.width / n_bins);
    var x = 0;

    t0 = performance.now()
    for (var i = 0; i < n_bins; i++) {
        var barHeight = (dsp.output[i] + 170) * 15;
        visCtx.fillStyle = `rgb(${Math.floor(barHeight)}, ${Math.floor(i * barHeight)}, 50)`;
        visCtx.fillRect(
            x,
            (y - barHeight) + 20,
            x_interval - (x_interval / 2),
            barHeight);

        x += x_interval;
    }
    t1 = performance.now()
    let drawing_ms = t1 - t0;

    visCtx.fillStyle = "#EEEEEE";
    visCtx.fillText(`CQT : ${cqt_ms} ms`, visualizer.width - 200, 20);
    visCtx.fillText(`Clear : ${clear_ms} ms`, visualizer.width - 200, 60);
    visCtx.fillText(`Drawing : ${drawing_ms} ms`, visualizer.width - 200, 100);
}

function draw_chromatic() {
    notesCtx.fillStyle = 'rgb(0, 0, 0)';
    notesCtx.fillRect(0, 0, notes.width, notes.height);
    notesCtx.fillStyle = 'rgb(255, 255, 255)';

    const chromatic_24 = {2: 'C', 6: 'D', 10:"E", 12:'F', 16:'G', 20:'A', 24:'B'};

    notesCtx.font = 'normal 24px serif';
    let counter = 1;
    let octave_counter = 1;
    const interval = notes.width / n_bins;
    var x_pos = 0;
    for (var i = 1; i <= n_bins; i++) {
        if (counter > BINS_PER_OCTAVE) {
            counter = 1;
            octave_counter++;
        }

        notesCtx.font = 'normal 24px serif';
        if (i % 2 == 0) 
            notesCtx.fillText('|', x_pos + (interval / 2), 5);

        if (chromatic_24[counter]) {
            notesCtx.font = 'normal 20px serif';
            notesCtx.fillText(chromatic_24[counter], x_pos, notes.height - 20);
            notesCtx.font = 'normal 12px serif';
            notesCtx.fillText(octave_counter, x_pos + interval + 5, notes.height - 15);
        }
        x_pos += interval;
        counter++;
    }
}

function benchmark() {
    const N = 60;
    let inputs = new Array(N);
    audioCtx.resume();

    audioElement.play();
    for (let i = 0; i < N; i++) {
        let signal = new Float32Array(analyzer.fftSize);
        analyzer.getFloatTimeDomainData(signal);
        inputs[i] = signal;
    }

    audioElement.pause();

    let t0 = performance.now();
    for (let i = 0; i < N; i++) {
        dsp.input = inputs[i];
        dsp.calc_cqt();
    }
    let t1 = performance.now();
    let cqt_ms = t1 - t0;
    console.log(dsp.output);
    console.log(`CQT (WASM) calculation ${N} total of ${analyzer.fftSize} done in ${cqt_ms} ms`);
}

window.addEventListener('load', init);
