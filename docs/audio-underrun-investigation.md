# Audio "no sound / underrun" investigation — 2026-07-02

Reinvestigation of the recurring **audio regression** after bug-fix pass #3.
Source of the log: `docs/DESKTOP-V5MIMQC.log` (run while the user changed
audio devices and tapped the on-screen keyboard).

## 0. Ground truth established

- `git log` confirms HEAD = `dd3c92a` (diagnostic commit) on top of
  `0965e00` ("make audio thread fully lock-free"). The lock-free
  `MidiOutputRouter` fix described in `AGENTS.md` **IS in the build that
  produced the log**. So the earlier root-cause theory (MidiOutputRouter
  `CriticalSection` on the audio thread) is already eliminated, yet the
  symptom persists. The investigation below explains why.

## 1. Full audio-thread path audit (result: no blocking)

Every routine that runs inside `getNextAudioBlock` was inspected:

| Path | Locking on audio thread | Verdict |
|------|-------------------------|---------|
| `MainComponent::getNextAudioBlock` | none; atomics only | clean |
| `SynthAudioSource::getNextAudioBlock` | none | clean |
| `MidiTrack::getNextAudioBlock` | `juce::ScopedTryLock` on `clip.lock` (skip-on-contend) | clean |
| `TracksViewport::tryRenderAll` | `std::atomic<std::shared_ptr<vector<AudioTrack*>>>::load` | lock-free-ish (see §5) |
| `AudioTrack::renderInto` | `pluginLock` `ScopedTryLockType` (skip-on-contende) | clean |
| `AudioTrackSource::getNextAudioBlock` | `bufferLock` `ScopedTryLockType` (skip-on-contend) | clean |
| `MidiOutputRouter::sendBuffer` | `std::atomic<MidiOutput*>` load only | clean |
| `Recorder::audioDeviceIOCallbackWithContext` | `bufferLock` `ScopedLock` **but only while recording** (`recording=false` by default ⇒ early return) | clean at idle |

**Conclusion: the audio thread no longer enters the kernel or blocks on
any lock during normal playback.** The lock-free pass (#3) is effective.

## 2. Bug #1 — startup `grabKeyboardFocus` assertion

Log line 4: `JUCE Assertion failure in components/juce_Component.cpp:3022`.

`juce_Component.cpp:3022` is:

```cpp
void Component::grabKeyboardFocus() {
    ...
    jassert (isShowing() || isOnDesktop());   // <-- line 3022
}
```

`Main.cpp:161-162` calls `setWantsKeyboardFocus(true); grabKeyboardFocus();`
**inside the `MainComponent` constructor**, before the component has been
added to the `DocumentWindow` / shown on the desktop, so `isShowing()` is
false and `isOnDesktop()` is false ⇒ assertion fires (printed, non-fatal
in Debug's logger path, execution continues).

Effect: harmless to audio, but proves the focus grab does **not** take
effect (spacebar transport only works once the window later gives it
focus), and it pollutes the run log at `T+0.238s`, right before the
first `prepareToPlay`.

## 3. Bug #2 — the "underrun" diagnostic is wrong AND runs on the audio thread

`Main.cpp:264-299` is the diagnostic added in commit `dd3c92a`:

```cpp
const bool blockNonZero = blockPeak > 0.0f;
if (blockNonZero) { ... "First non-zero output block" ... hadAudio=true; silentStreak=0; }
else if (hadAudio.load()) {
    const int streak = silentStreak.fetch_add(1) + 1;
    if (streak == 1) Logger::writeToLog("[DAW] Audio underrun suspected ...");
    if (streak == 4800) Logger::writeToLog("[DAW] Output buffer has been silent for ~5s ...");
}
```

### 3a. The logic cannot distinguish idle silence from a real underrun

It flags a silent block as an "underrun" whenever it follows *any* non-zero
block, regardless of whether audio should be playing. A momentary
key-tap on the on-screen keyboard produces exactly one non-zero block
(`peak = velocity * 0.15`; the log's `0.055` ⇒ velocity ≈ 0.37, a normal
click) and then **release-silence**, which the diagnostic misreports as an
underrun. The three logs at `5.698 / 5.990 / 6.298` are three quick taps,
**not** three dropped buffers.

### 3b. `Logger::writeToLog` is called ON the audio thread

`Logger` takes a `CriticalSection` and, on Windows, may write to the
debugger / a file = **blocking I/O on the real-time thread**. Once the
diagnostic starts firing it can feed back into a genuine underrun
cascade: silent block → blocking log → the *next* block's deadline is
missed → device inserts silence → silent block → log → …

So the diagnostic is not merely noisy — it is itself a real-time
contract violation that can manufacture the very underruns it claims to
detect.

### 3c. Why the AGENTS.md "ROOT CAUSE FIXED" note was misleading

The lock-free change removed one concrete blocker, but the regression
*report* ("meter moves, no sound, underrun log floods") was being
produced by this diagnostic, not (necessarily) by a real device
underrun. Removing the lock may have fixed a real problem *and* left the
false-positive diagnostic still screaming. The log from today proves the
diagnostic is the dominant remaining signal.

## 4. What the log CAN'T prove

The diagnostic only ever observed **brief blips**, never a sustained
held note or sequencer passage. Therefore:

- We **cannot** confirm sustained playback works end-to-end.
- We **cannot** confirm a genuine recurring device underrun, because
  the detector infers from buffer *content* instead of from callback
  *timing*.

The measured peak `0.055` is taken **post-master-gain**, i.e. it is
audible-level audio actually sitting in the device's output buffer. So:
- meter flashes after a tap ⇒ correct — that block really did contain audio;
- if, while **holding** a key, the meter sustains ~0.05 but you still
  hear nothing ⇒ that *would* point at device delivery (WASAPI
  shared/exclusive, output routing, sample-rate mismatch — note the log
  shows one run at 44100 Hz exclusive), and the current diagnostic could
  not have told you that.

## 5. Minor concern — `std::atomic<std::shared_ptr>` is not lock-free on MSVC

`TracksViewport::tracksSnapshot` is `std::atomic<std::shared_ptr<const
std::vector<AudioTrack*>>>`. On MSVC this specialization is **not**
lock-free (`is_always_lock_free == false`); it serializes load/store
through an internal spinlock. The audio thread calls `.load()` every
block. Uncontended it is a cheap user-mode spinlock (nanoseconds), and
`publishSnapshot` only runs on add/remove/reorder, so contention is
rare. Not a likely cause of the reported symptom, but it is technically
on the audio path and is not truly lock-free. Revisit if real
timing-based detection ever shows periodic spikes correlated with track
add/remove. Alternatives: raw `std::atomic<AudioTrack**>` + epoch
reclamation, a triple buffer, or JUCE `SpinLock` with `ScopedTryLockType`
(skip render on contention like the other paths).

## 6. Proposed fixes (NOT yet applied — awaiting approval)

### Fix A — startup focus assertion (`src/Main.cpp`)

Remove the constructor `grabKeyboardFocus()`. Keep
`setWantsKeyboardFocus(true)` in the constructor (safe). Call
`grabKeyboardFocus()` from `visibilityChanged()` instead:

```cpp
void visibilityChanged() override {
    if (isShowing() && ! hasKeyboardFocus(true))
        grabKeyboardFocus();
}
```

### Fix B — remove audio-thread logging (`src/Main.cpp:264-299`)

Delete every `juce::Logger::writeToLog(...)` inside `getNextAudioBlock`.
Replace with atomic counters incremented on the audio thread and a
message-thread summary emitted from the existing 500 ms `Timer`:

```cpp
// members
std::atomic<int> audioBlocksProduced{0};
std::atomic<int> audioBlocksSilent{0};
std::atomic<int> realUnderruns{0};
std::atomic<double> lastCallbackMs{0.0};

// in getNextAudioBlock (audio thread, NO Logger):
const double nowMs = juce::Time::getMillisecondCounterHiRes();
const double prevMs = lastCallbackMs.exchange(nowMs);
// ... render ...
++audioBlocksProduced;
if (blockPeak <= 0.0f) ++audioBlocksSilent;

// in timerCallback (message thread, Logger SAFE):
const int produced = audioBlocksProduced.exchange(0);
const int silent   = audioBlocksSilent.exchange(0);
const int underrun = realUnderruns.exchange(0);
if (underrun > 0)
    juce::Logger::writeToLog("[DAW] " + juce::String(underrun) + " real underruns in last 500 ms");
```

### Fix C — timing-based underrun detection (replaces §3a content inference)

Real underruns are deadline misses, not silent buffers. Stamp the
callback arrival time and compare to the expected period:

```cpp
// in getNextAudioBlock (audio thread):
const double nowMs   = juce::Time::getMillisecondCounterHiRes();
const double prevMs  = lastCallbackMs.exchange(nowMs);
const double periodMs = double(bufferSize) / sampleRate * 1000.0;
if (prevMs > 0.0 && (nowMs - prevMs) > periodMs * 1.5)
    ++realUnderruns;                 // atomic; reported from Timer
```

`bufferSize` / `sampleRate` are captured in `prepareToPlay` (already
available via `deviceManager.getCurrentAudioDevice()`).

### Fix D — silence is only suspicious when audio is expected

Only increment `audioBlocksSilent`-as-suspicious when something should be
audible:

```cpp
const bool audioExpected = midiTrack.isPlaying()
    || synthSource.keyboardState.getNumberOfKeyDowns() > 0
    || tracksViewport.anyTrackAudible();   // helper: a track that isPlaying() && isLoaded()
```

Idle silence (nothing held/playing) must never log, never count, never
touch `Logger`. This single guard eliminates the entire false-positive
class seen in the log.

### Fix E — (optional) make `tracksSnapshot` lock-free or try-lock

See §5. Defer unless timing detection implicates it.

## 7. Verification protocol (do this with the fixed diagnostic)

1. Launch the app. Confirm the startup `juce_Component.cpp:3022`
   assertion no longer prints.
2. **Hold** a mouse key on the on-screen keyboard for ~5 s.
   - Meter sustains ~0.05 **and** a steady tone is audible ⇒ audio path
     is fine; the previous "no sound" was brief-taps + the misleading
     diagnostic. Done.
   - Meter sustains ~0.05 **but** silence ⇒ real device delivery issue.
     Check: WASAPI shared vs exclusive; the Komplete Audio 1's direct
     monitor / headphone routing; any sample-rate mismatch (the log
     shows a 44100 Hz exclusive run — switching devices can re-pitch /
     re-clock but should not silence).
3. Start the sequencer (spacebar). Meter should oscillate with the
   C-scale demo melody. `realUnderruns` should stay 0 at 480/48k with
   no tracks loaded. If it climbs, profile per-track work (plugins,
   `applyGainRamp`, `getMagnitude`) and the §5 snapshot load.
4. Re-run with the same audio-device cycling seen in the log; the
   false "underrun suspected" lines should now be gone unless a true
   deadline miss occurs.

## 8. Documentation correction vs AGENTS.md

- AGENTS.md says the Recorder "pre-allocates 300 s in
  `audioDeviceAboutToStart`". The **actual** `Recorder.cpp` only stores
  `recordSampleRate` / `deviceBufferSize` there; allocation is lazy on
  the first recording block (and never happens while `recording=false`).
  Update AGENTS.md to match code.
- AGENTS.md's "Audio regression — ROOT CAUSE FIXED" entry should be
  amended: the lock-free change removed a real blocker, but **did not**
  silence the diagnostic, which independently (a) misreports idle/tap
  silence as underruns and (b) violates the audio-thread I/O rule. Pass
  #4 (this document) is the actual close-out for the reported symptom.