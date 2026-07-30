# Chord Lens

A local browser app that identifies chord changes in an uploaded audio file or a microphone recording. It runs entirely in the browser: the audio is not uploaded anywhere.

## Run it

From this repository root, start a static web server:

```bash
python3 -m http.server 8000 --directory web
```

Open [http://localhost:8000](http://localhost:8000). Use **Upload audio** for a local audio file, or **Record from microphone** and allow the browser's microphone permission. Stop the recording, then click **Identify chords**.

The app uses the Web Audio API, so audio formats your browser can play should work. Chrome or Edge are good choices for `.webm` microphone recordings; desktop Safari is also supported for common audio formats.

## How recognition works

For overlapping 186 ms audio windows, the app computes a spectrum, folds pitch energy into the twelve musical pitch classes (chroma), and compares that profile with major, minor, dominant 7th, major 7th, minor 7th, suspended 4th, and diminished chord templates. It ignores low-energy frames, smooths adjacent decisions, and presents the resulting chord timeline. Each result has a **Play** button that previews the detected chord with a synthesized piano-like voicing.

This is intentionally an explainable baseline rather than a trained transcription model. It is most accurate on isolated guitar/piano chords and simple progressions. Very dense mixes, heavy drums, inversions, altered chords, and detuned audio can make a template-based estimate ambiguous.

## Existing C++ prototype

The original CMake project remains intact under `src/`; it reads audio with libsndfile and exports RMS values. The browser app is independent of it so it can accept recorded browser audio and common upload formats with no extra native dependencies.

## Verify the analyser

The small regression suite synthesizes C, Dm, G7, and Cmaj7 chords and checks that the analyser returns each expected label:

```bash
node tests/chord-recognition-test.js
```
