// Lightweight regression checks for the browser analyser; run with Node from
// the repository root. The DOM stub is only needed because app.js wires UI
// event listeners when it is loaded.
const assert = require('assert');
const fs = require('fs');

global.document = { querySelector: () => ({ addEventListener() {} }) };
const app = fs.readFileSync('web/app.js', 'utf8');

eval(`${app}
  const makeChord = (notes) => {
    const rate = 44100, size = 8192, samples = new Float32Array(size);
    for (let index = 0; index < size; index++) for (const midi of notes) {
      const frequency = 440 * 2 ** ((midi - 69) / 12);
      samples[index] += Math.sin(2 * Math.PI * frequency * index / rate) / notes.length;
    }
    return classify(featuresForFrame(samples, 0, size, rate).chroma).label;
  };
  assert.strictEqual(makeChord([48, 52, 55]), 'C');
  assert.strictEqual(makeChord([50, 53, 57]), 'Dm');
  assert.strictEqual(makeChord([43, 47, 50, 53]), 'G7');
  assert.strictEqual(makeChord([48, 52, 55, 59]), 'Cmaj7');
`);

console.log('Chord-recognition regression checks passed.');
