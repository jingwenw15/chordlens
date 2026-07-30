/* Local chroma-template chord recognition. Source audio never leaves the browser. */
const input = document.querySelector('#file-input');
const player = document.querySelector('#player');
const analyse = document.querySelector('#analyse-button');
const record = document.querySelector('#record-button');
const sourceName = document.querySelector('#source-name');
const note = document.querySelector('#recording-note');
const status = document.querySelector('#status');
const results = document.querySelector('#results');
let selectedFile, recorder, recordingParts = [], previewContext;

const PITCHES = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'];
const TEMPLATES = [
  ['major', [0, 4, 7]], ['minor', [0, 3, 7]], ['7', [0, 4, 7, 10]],
  ['maj7', [0, 4, 7, 11]], ['min7', [0, 3, 7, 10]], ['sus4', [0, 5, 7]], ['dim', [0, 3, 6]]
];
const DISPLAY_SUFFIX = { major: '', minor: 'm', '7': '7', maj7: 'maj7', min7: 'm7', sus4: 'sus4', dim: 'dim' };

function setStatus(message, type = '') { status.textContent = message; status.className = `status ${type}`; }
function prettyTime(seconds) { const m = Math.floor(seconds / 60); return `${m}:${String(Math.floor(seconds % 60)).padStart(2, '0')}`; }
function setFile(file) {
  selectedFile = file; sourceName.textContent = file.name;
  player.src = URL.createObjectURL(file); player.hidden = false; analyse.disabled = false;
  results.hidden = true; setStatus('Ready to identify chords.');
}
input.addEventListener('change', () => input.files[0] && setFile(input.files[0]));

record.addEventListener('click', async () => {
  if (recorder?.state === 'recording') { recorder.stop(); return; }
  try {
    const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
    recordingParts = []; recorder = new MediaRecorder(stream);
    recorder.ondataavailable = event => event.data.size && recordingParts.push(event.data);
    recorder.onstop = () => {
      stream.getTracks().forEach(track => track.stop()); note.hidden = true; record.textContent = 'Record from microphone';
      setFile(new File(recordingParts, `recording-${Date.now()}.webm`, { type: recorder.mimeType || 'audio/webm' }));
      setStatus('Recording ready. Press “Identify chords” to analyse it.');
    };
    recorder.start(); note.hidden = false; record.textContent = 'Stop recording';
    setStatus('Listening… play a chord or progression, then stop recording.', 'working');
  } catch (error) { setStatus(`Microphone unavailable: ${error.message}`, 'error'); }
});

function fft(real) {
  const n = real.length, re = real.slice(), im = new Float32Array(n);
  for (let i = 1, j = 0; i < n; i++) { let bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) [re[i], re[j]] = [re[j], re[i]]; }
  for (let len = 2; len <= n; len <<= 1) {
    const angle = -2 * Math.PI / len, wrStep = Math.cos(angle), wiStep = Math.sin(angle);
    for (let i = 0; i < n; i += len) { let wr = 1, wi = 0;
      for (let j = 0; j < len / 2; j++) { const a = i + j, b = a + len / 2, br = re[b] * wr - im[b] * wi, bi = re[b] * wi + im[b] * wr; re[b] = re[a] - br; im[b] = im[a] - bi; re[a] += br; im[a] += bi; [wr, wi] = [wr * wrStep - wi * wiStep, wr * wiStep + wi * wrStep]; }
    }
  }
  return { re, im };
}

function featuresForFrame(samples, start, size, rate) {
  const frame = new Float32Array(size); let mean = 0, squareSum = 0;
  for (let i = 0; i < size; i++) { const sample = samples[start + i] || 0; mean += sample; squareSum += sample * sample; }
  mean /= size;
  for (let i = 0; i < size; i++) frame[i] = ((samples[start + i] || 0) - mean) * (0.5 - 0.5 * Math.cos(2 * Math.PI * i / (size - 1)));
  const { re, im } = fft(frame), chroma = new Float32Array(12);
  for (let bin = 2; bin < size / 2; bin++) {
    const frequency = bin * rate / size;
    if (frequency < 55 || frequency > 1800) continue;
    const magnitude = Math.hypot(re[bin], im[bin]) / Math.sqrt(frequency);
    // Split energy between neighbouring semitones. This avoids a pitch changing
    // identity merely because its FFT peak landed on the other side of a bin.
    const midi = 69 + 12 * Math.log2(frequency / 440), lower = Math.floor(midi), fraction = midi - lower;
    chroma[((lower % 12) + 12) % 12] += magnitude * (1 - fraction);
    chroma[(((lower + 1) % 12) + 12) % 12] += magnitude * fraction;
  }
  const norm = Math.hypot(...chroma);
  return { chroma: norm ? chroma.map(value => value / norm) : chroma, rms: Math.sqrt(squareSum / size) };
}

function templateProfile(root, intervals) {
  const profile = new Float32Array(12);
  intervals.forEach(interval => { profile[(root + interval) % 12] = interval === 0 ? 1.25 : 1; });
  const norm = Math.hypot(...profile);
  return profile.map(value => value / norm);
}
function classify(chroma) {
  let best = { score: -Infinity }, runnerUp = { score: -Infinity };
  for (let root = 0; root < 12; root++) for (const [suffix, intervals] of TEMPLATES) {
    const profile = templateProfile(root, intervals);
    let score = 0; for (let pc = 0; pc < 12; pc++) score += chroma[pc] * profile[pc];
    // Harmonics of a simple triad can coincide with a seventh. Prefer the
    // simpler triad unless the fourth chord tone adds meaningful evidence.
    score -= 0.03 * (intervals.length - 3);
    const candidate = { root, suffix, intervals, score };
    if (score > best.score) { runnerUp = best; best = candidate; } else if (score > runnerUp.score) runnerUp = candidate;
  }
  const margin = Math.max(0, best.score - runnerUp.score);
  return { ...best, label: `${PITCHES[best.root]}${DISPLAY_SUFFIX[best.suffix]}`, confidence: Math.min(99, Math.round(42 + margin * 260)) };
}
function smoothFrames(frames) {
  return frames.map((frame, index) => {
    const nearby = frames.slice(Math.max(0, index - 2), Math.min(frames.length, index + 3));
    const counts = {}; nearby.forEach(item => counts[item.label] = (counts[item.label] || 0) + 1);
    const label = Object.entries(counts).sort((a, b) => b[1] - a[1])[0][0];
    return { ...nearby.find(item => item.label === label), label };
  });
}
function toSegments(frames, hopSeconds, duration) {
  const out = [];
  for (const frame of smoothFrames(frames)) {
    if (frame.label === 'No chord') continue;
    const last = out.at(-1);
    if (last && last.label === frame.label) { last.end = frame.time + hopSeconds; last.confidence += frame.confidence; last.count++; }
    else out.push({ ...frame, start: frame.time, end: frame.time + hopSeconds, count: 1 });
  }
  return out.filter(segment => segment.end - segment.start >= 0.45).map(segment => ({ ...segment, end: Math.min(segment.end, duration), confidence: Math.round(segment.confidence / segment.count) }));
}
async function decode(file) { const context = new (window.AudioContext || window.webkitAudioContext)(); try { return await context.decodeAudioData(await file.arrayBuffer()); } finally { await context.close(); } }

function playChord(segment) {
  previewContext ||= new (window.AudioContext || window.webkitAudioContext)();
  const context = previewContext, now = context.currentTime, notes = [segment.root, ...segment.intervals.map(interval => segment.root + interval), segment.root + 12];
  context.resume();
  notes.forEach((noteNumber, index) => {
    const oscillator = context.createOscillator(), gain = context.createGain(), midi = 48 + noteNumber + (index === 0 ? -12 : 0);
    oscillator.type = index === 0 ? 'triangle' : 'sine'; oscillator.frequency.value = 440 * 2 ** ((midi - 69) / 12);
    gain.gain.setValueAtTime(0.0001, now); gain.gain.exponentialRampToValueAtTime(index === 0 ? 0.11 : 0.075, now + 0.025); gain.gain.exponentialRampToValueAtTime(0.0001, now + 1.15);
    oscillator.connect(gain).connect(context.destination); oscillator.start(now); oscillator.stop(now + 1.2);
  });
}
function render(segments, duration) {
  document.querySelector('#summary').textContent = segments.map(segment => segment.label).join('  →  ');
  document.querySelector('#duration').textContent = `${prettyTime(duration)} analysed`;
  const timeline = document.querySelector('#timeline'), rows = document.querySelector('#chord-rows'); timeline.innerHTML = ''; rows.innerHTML = '';
  segments.forEach(segment => {
    const block = document.createElement('div'); block.className = 'chord-block'; block.style.flexGrow = String(Math.max(.45, segment.end - segment.start));
    block.innerHTML = `<span>${segment.label}</span><small>${prettyTime(segment.start)}</small>`; timeline.append(block);
    const row = document.createElement('tr');
    row.innerHTML = `<td>${prettyTime(segment.start)}</td><td>${prettyTime(segment.end)}</td><td>${segment.label}</td><td>${segment.confidence}%</td>`;
    const previewCell = document.createElement('td'), button = document.createElement('button'); button.className = 'button secondary play-chord'; button.type = 'button'; button.textContent = '▶ Play'; button.setAttribute('aria-label', `Play ${segment.label}`); button.addEventListener('click', () => playChord(segment)); previewCell.append(button); row.append(previewCell); rows.append(row);
  });
  results.hidden = false;
}
analyse.addEventListener('click', async () => {
  if (!selectedFile) return; analyse.disabled = true; results.hidden = true; setStatus('Decoding audio…', 'working');
  try {
    const buffer = await decode(selectedFile), mono = new Float32Array(buffer.length);
    for (let channel = 0; channel < buffer.numberOfChannels; channel++) { const data = buffer.getChannelData(channel); for (let i = 0; i < mono.length; i++) mono[i] += data[i] / buffer.numberOfChannels; }
    const size = 8192, hop = 4096, rawFrames = [];
    if (mono.length < size) throw new Error('Audio is too short—record at least one second.');
    for (let start = 0; start + size <= mono.length; start += hop) rawFrames.push({ ...featuresForFrame(mono, start, size, buffer.sampleRate), time: start / buffer.sampleRate });
    // A relative threshold handles both quiet and loud recordings while excluding
    // leading/trailing room noise, which otherwise looks like a random chord.
    const peakRms = Math.max(...rawFrames.map(frame => frame.rms)), silenceThreshold = Math.max(0.003, peakRms * 0.12);
    const frames = rawFrames.map(frame => frame.rms < silenceThreshold ? { ...frame, label: 'No chord', confidence: 0 } : { ...frame, ...classify(frame.chroma) });
    const segments = toSegments(frames, hop / buffer.sampleRate, buffer.duration);
    if (!segments.length) throw new Error('No stable chord found. Try a clearer or longer recording.');
    render(segments, buffer.duration); setStatus(`Finished: ${segments.length} chord${segments.length === 1 ? '' : 's'} detected.`);
  } catch (error) { console.error(error); setStatus(`Could not analyse this audio: ${error.message}`, 'error'); } finally { analyse.disabled = false; }
});
