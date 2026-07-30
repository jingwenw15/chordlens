/* Frontend only: audio capture/upload, API requests, and result presentation. */
const input = document.querySelector('#file-input');
const player = document.querySelector('#player');
const analyse = document.querySelector('#analyse-button');
const record = document.querySelector('#record-button');
const sourceName = document.querySelector('#source-name');
const note = document.querySelector('#recording-note');
const status = document.querySelector('#status');
const results = document.querySelector('#results');
const playProgressionButton = document.querySelector('#play-progression');
const playWithAudioButton = document.querySelector('#play-with-audio');
const stability = document.querySelector('#stability');
let selectedFile, recorder, previewContext, currentSegments = [], progressionActive = false, progressionPaused = false, progressionUsesOriginal = false, activeProgressionVoices = 0;

function setStatus(message, type = '') { status.textContent = message; status.className = `status ${type}`; }
function prettyTime(seconds) { const minutes = Math.floor(seconds / 60); return `${minutes}:${String(Math.floor(seconds % 60)).padStart(2, '0')}`; }
function setFile(file, playbackFile = file, displayName = playbackFile.name || file.name) {
  selectedFile = file; sourceName.textContent = displayName; player.src = URL.createObjectURL(playbackFile); player.load();
  player.hidden = false; analyse.disabled = false; results.hidden = true; setStatus('Ready to identify chords.');
}
function wavFile(chunks, sampleRate) {
  const length = chunks.reduce((total, chunk) => total + chunk.length, 0), wav = new ArrayBuffer(44 + length * 2), view = new DataView(wav);
  const write = (offset, value) => view.setUint32(offset, value, true); view.setUint32(0, 0x52494646, false); write(4, 36 + length * 2); view.setUint32(8, 0x57415645, false); view.setUint32(12, 0x666d7420, false); write(16, 16); view.setUint16(20, 1, true); view.setUint16(22, 1, true); write(24, sampleRate); write(28, sampleRate * 2); view.setUint16(32, 2, true); view.setUint16(34, 16, true); view.setUint32(36, 0x64617461, false); write(40, length * 2);
  let offset = 44; for (const chunk of chunks) for (const sample of chunk) { view.setInt16(offset, Math.max(-1, Math.min(1, sample)) * 0x7fff, true); offset += 2; }
  return new File([wav], `recording-${Date.now()}.wav`, { type: 'audio/wav' });
}
input.addEventListener('change', () => input.files[0] && setFile(input.files[0]));
player.addEventListener('error', () => setStatus('This browser could not decode the recording preview. Try Chrome or Edge, then record again.', 'error'));
record.addEventListener('click', async () => {
  if (recorder) { if (recorder.state === 'recording') recorder.stop(); return; }
  try {
    const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
    const context = new (window.AudioContext || window.webkitAudioContext)(), source = context.createMediaStreamSource(stream), processor = context.createScriptProcessor(4096, 1, 1), silence = context.createGain(), chunks = [], previewParts = [], recordingRate = context.sampleRate;
    silence.gain.value = 0; processor.onaudioprocess = event => chunks.push(new Float32Array(event.inputBuffer.getChannelData(0)));
    source.connect(processor); processor.connect(silence); silence.connect(context.destination); await context.resume();
    // Keep the browser's native recording for reliable playback, and build a
    // PCM WAV copy for libsndfile/C++ analysis in parallel.
    const mediaRecorder = new MediaRecorder(stream);
    recorder = mediaRecorder;
    mediaRecorder.ondataavailable = event => event.data.size && previewParts.push(event.data);
    mediaRecorder.onstop = () => {
      processor.disconnect(); source.disconnect(); stream.getTracks().forEach(track => track.stop()); context.close();
      const analysisFile = wavFile(chunks, recordingRate);
      const preview = new Blob(previewParts, { type: mediaRecorder.mimeType || 'audio/webm' });
      recorder = undefined; note.hidden = true; record.textContent = 'Record from microphone';
      if (!preview.size) { setStatus('The browser did not produce recording data. Please try recording again in Chrome or Edge.', 'error'); return; }
      setFile(analysisFile, preview, `recording-${Date.now()}`);
      setStatus('Recording ready. Press “Identify chords” to analyse it.');
    };
    mediaRecorder.start(250);
    note.hidden = false; record.textContent = 'Stop recording'; setStatus('Listening… play a chord or progression, then stop recording.', 'working');
  } catch (error) { setStatus(`Microphone unavailable: ${error.message}`, 'error'); }
});
function scheduleChord(context, segment, when, duration, onVoiceEnded) {
  const now = when, length = duration;
  [segment.root - 12, ...segment.intervals.map(interval => segment.root + interval), segment.root + 12].forEach((note, index) => {
    const oscillator = context.createOscillator(), gain = context.createGain(), midi = 48 + note;
    oscillator.type = index === 0 ? 'triangle' : 'sine'; oscillator.frequency.value = 440 * 2 ** ((midi - 69) / 12); gain.gain.setValueAtTime(.0001, now); gain.gain.exponentialRampToValueAtTime(index === 0 ? .11 : .075, now + .025); gain.gain.setValueAtTime(index === 0 ? .11 : .075, now + Math.max(.03, length - .08)); gain.gain.exponentialRampToValueAtTime(.0001, now + length); oscillator.connect(gain).connect(context.destination); oscillator.start(now); oscillator.stop(now + length + .02);
    if (onVoiceEnded) oscillator.addEventListener('ended', onVoiceEnded, { once: true });
  });
}
function playChord(segment) {
  previewContext ||= new (window.AudioContext || window.webkitAudioContext)(); previewContext.resume();
  scheduleChord(previewContext, segment, previewContext.currentTime, 1.2);
}
function setProgressionControls() {
  if (!progressionActive) { playProgressionButton.textContent = '▶ Play chord progression'; playWithAudioButton.textContent = '▶ Play with original audio'; return; }
  const label = progressionPaused ? '▶ Resume' : 'Ⅱ Pause';
  (progressionUsesOriginal ? playWithAudioButton : playProgressionButton).textContent = label;
}
function finishProgression() {
  if (!progressionActive) return; progressionActive = false; progressionPaused = false;
  if (progressionUsesOriginal) player.pause(); progressionUsesOriginal = false; setProgressionControls();
}
async function toggleProgression(withOriginal) {
  if (!currentSegments.length) return;
  if (progressionActive) {
    if (progressionPaused) { await previewContext.resume(); if (progressionUsesOriginal) await player.play(); progressionPaused = false; }
    else { await previewContext.suspend(); if (progressionUsesOriginal) player.pause(); progressionPaused = true; }
    setProgressionControls(); return;
  }
  previewContext ||= new (window.AudioContext || window.webkitAudioContext)(); await previewContext.resume();
  const firstTime = currentSegments[0].start;
  if (withOriginal) { player.currentTime = firstTime; await player.play(); }
  const start = previewContext.currentTime + .06;
  progressionActive = true; progressionPaused = false; progressionUsesOriginal = withOriginal;
  activeProgressionVoices = currentSegments.reduce((total, segment) => total + segment.intervals.length + 2, 0);
  const voiceEnded = () => { activeProgressionVoices--; if (activeProgressionVoices === 0) finishProgression(); };
  currentSegments.forEach(segment => scheduleChord(previewContext, segment, start + segment.start - firstTime, Math.max(.1, segment.end - segment.start), voiceEnded));
  setProgressionControls();
}
playProgressionButton.addEventListener('click', () => toggleProgression(false));
playWithAudioButton.addEventListener('click', () => toggleProgression(true));
function render(analysis) {
  const { segments, duration } = analysis; currentSegments = segments; document.querySelector('#summary').textContent = segments.map(segment => segment.label).join('  →  '); document.querySelector('#duration').textContent = `${prettyTime(duration)} analysed`;
  const timeline = document.querySelector('#timeline'), rows = document.querySelector('#chord-rows'); timeline.innerHTML = ''; rows.innerHTML = '';
  segments.forEach(segment => { const block = document.createElement('div'); block.className = 'chord-block'; block.style.flexGrow = String(Math.max(.45, segment.end - segment.start)); block.innerHTML = `<span>${segment.label}</span><small>${prettyTime(segment.start)}</small>`; timeline.append(block); const row = document.createElement('tr'); row.innerHTML = `<td>${prettyTime(segment.start)}</td><td>${prettyTime(segment.end)}</td><td>${segment.label}</td><td>${segment.confidence}%</td>`; const cell = document.createElement('td'), button = document.createElement('button'); button.className = 'button secondary play-chord'; button.type = 'button'; button.textContent = '▶ Play'; button.addEventListener('click', () => playChord(segment)); cell.append(button); row.append(cell); rows.append(row); }); results.hidden = false;
}
analyse.addEventListener('click', async () => {
  if (!selectedFile) return; analyse.disabled = true; results.hidden = true; setStatus('Sending audio to the C++ analyser…', 'working');
  try { const body = new FormData(); body.append('audio', selectedFile); body.append('stability', stability.value); const response = await fetch('/api/analyze', { method: 'POST', body }); const data = await response.json(); if (!response.ok) throw new Error(data.error || 'Analysis failed.'); if (!data.segments.length) throw new Error('No stable chord found. Try a clearer or longer recording.'); render(data); setStatus(`Finished: ${data.segments.length} chord${data.segments.length === 1 ? '' : 's'} detected.`); } catch (error) { setStatus(`Could not analyse this audio: ${error.message}`, 'error'); } finally { analyse.disabled = false; }
});
