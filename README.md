# Chord Lens

A local browser app that identifies chord changes in an uploaded audio file or a microphone recording. The frontend sends the selected file only to a C++ analyser running on your own computer; it is never sent to an external service.

**Disclaimer**: Generated with Codex.

## Run it

From this repository root, build the C++ analyser and start the local backend:

```bash
cmake -S . -B build
cmake --build build
python3 backend/server.py
```

Open [http://localhost:8000](http://localhost:8000). Use **Upload audio** for a local audio file, or **Record from microphone** and allow the browser's microphone permission. The microphone is converted to WAV in the browser, then analysed by the C++ executable through the local API.

The app uses the Web Audio API, so audio formats your browser can play should work. Chrome or Edge are good choices for `.webm` microphone recordings; desktop Safari is also supported for common audio formats.

## Architecture

```
web/ (HTML, CSS, UI JavaScript)
        │ multipart audio POST
        ▼
backend/server.py (local HTTP API)
        │ invokes
        ▼
src/ (C++ audio reader and chord detector)
```

`web/app.js` contains no chord-recognition DSP. It handles recording/uploading, displaying JSON results, and the optional synthesized chord preview. `src/dsp/chord_detector.cpp` owns the recognition algorithm and `src/main.cpp` exposes it as a CLI that returns JSON with `--json`.

## How recognition works

For overlapping 186 ms audio windows, the C++ analyser computes a spectrum, folds pitch energy into the twelve musical pitch classes (chroma), and compares that profile with major, minor, dominant 7th, major 7th, minor 7th, suspended 4th, and diminished chord templates. It ignores low-energy frames, then uses temporal decoding with a chord-change penalty so a new segment is emitted only after sustained evidence for a different chord. Use the UI's **Chord change detail** control to choose the appropriate grouping for the recording. Each result has a **Play** button that previews the detected chord with a synthesized piano-like voicing.

This is intentionally an explainable baseline rather than a trained transcription model. It is most accurate on isolated guitar/piano chords and simple progressions. Very dense mixes, heavy drums, inversions, altered chords, and detuned audio can make a template-based estimate ambiguous.

## Command-line use

The C++ analyser can also be called directly:

```bash
./build/chord_recognition path/to/audio.wav --json
```
