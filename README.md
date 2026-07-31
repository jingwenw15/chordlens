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

For overlapping 186 ms audio windows, the C++ analyser runs a tonal-analysis pipeline rather than a plain FFT-to-chroma lookup:

- It converts the recording to mono and analyses overlapping Hann-windowed frames.
- It computes a spectrum for each frame, then applies a simple temporal harmonic/percussive separation pass so short drum hits and other transient spikes are downweighted before chord scoring.
- It maps the remaining tonal energy through a constant-Q-style logarithmic filter bank, which is a better fit for musical pitch than linear FFT bins.
- It folds that log-frequency energy into chroma and compares the result with major, minor, dominant 7th, major 7th, minor 7th, suspended 4th, and diminished chord templates.
- It ignores low-energy frames, then uses temporal decoding with a chord-change penalty so a new segment is emitted only after sustained evidence for a different chord.
- It merges brief runs according to the UI's **Chord change detail** control so you can bias the detector toward either responsiveness or stability.

Each result has a **Play** button that previews the detected chord with a synthesized piano-like voicing.

This is intentionally an explainable baseline rather than a trained transcription model. It is most accurate on isolated guitar/piano chords and simple progressions. Very dense mixes, heavy drums, inversions, altered chords, and detuned audio can make a template-based estimate ambiguous.

## Recognition details

The current detector is still deliberately rule-based, but the scoring path is more musically informed than the first pass:

- Frame size: `8192` samples
- Hop size: `4096` samples
- Tonal front end: log-frequency, constant-Q-style binning over the musically relevant range
- Transient handling: a local temporal median suppresses percussive spikes before chroma extraction
- Chord vocabulary:
  - major
  - minor
  - dominant 7th
  - major 7th
  - minor 7th
  - suspended 4th
  - diminished

The output confidence is derived from the score margin between the chosen chord and the nearest runner-up after temporal decoding. Higher confidence means the spectrum matched one chord template more clearly than the alternatives.

## UI features

- Upload an audio file or record directly from the microphone.
- Adjust **Chord change detail** to trade off between fewer segments and faster chord changes.
- Preview an individual chord with a synthesized voicing.
- Play back the detected chord progression by itself.
- Play the detected chord progression on top of the original recording to sanity-check timing.

## Command-line use

The C++ analyser can also be called directly:

```bash
./build/chord_recognition path/to/audio.wav --json
```
