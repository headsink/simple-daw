# Simple DAW — Project Memory

**Path:** `C:\Users\User\OneDrive\2026\Viibe Codng Stuff\simple-daw`
**Stack:** C++20 · JUCE 8 · CMake 3.22+ · MSVC v143 · Windows 11 SDK 10.0.26100 · ASIO via Komplete Audio Driver (Native Instruments Komplete Audio 1)
**Goal:** Multi-track DAW with audio + MIDI tracks, piano roll editor, VST3 plugin hosting.

---

## Current state (as of last session)

**Week 0 → MIDI output routing + JSON save/load + recording + A/B loop + VST3 + piano roll + ASIO — Complete.** All major DAW features functional (see "What's working" below). Window size **960×600** is locked.
**Bug-fix pass #2 — Complete.** Three sync `delete this` heap-corruption bugs fixed (SettingsWindow, PluginEditorWindow, PianoRollWindow); MainComponent gained an alive flag for deferred-delete safety; per-track gain now smoothed; `~AudioTrack` calls `releaseResources()`; `loadSession` calls `sendAllNotesOff` on MIDI-out switch.
**Bug-fix pass #3 — IN PROGRESS.** Full code audit surfaced 14 real bugs across 9 files. Fix order: (1-3) three `DocumentWindow` double-frees — fix in progress. No leaks of `new`/`new[]` (all `unique_ptr`/`shared_ptr`/member-owned). Remaining: `MidiOutputRouter::output` unique_ptr race, `AudioTrackSource::currentSampleRate` non-atomic double, `fileBuffer` size-member reads without `bufferLock`, `clampLoopBound` reads without lock, `AudioTrack::lifetimeToken` dead code, `SessionIO::loadSession` silently loses saved A/B region (sets loop bounds before file loads), `Recorder::stopRecording` blocks audio thread on disk I/O, `Recorder::audioDeviceIOCallbackWithContext` allocates on audio thread, `setStateInformation` not try/catch'd, redundant `callAsync` in `TrackRow::openFile`/`filesDropped`, `PluginScanThread` 5 s shutdown stall.
**Bug-fix pass #3 — Complete (code changes).** All 13 fixes applied across 11 files; build green; 12/12 MidiClip tests pass. **Audio regression — ROOT CAUSE FIXED.** Root cause was the `CriticalSection` lock in `MidiOutputRouter::sendBuffer` running on the audio thread (`EnterCriticalSection` is a kernel transition; on contention it can stall the audio thread past the 480-sample @ 48 kHz deadline, producing underruns that the device fills with silence). Replaced the unique_ptr + lock with `std::atomic<juce::MidiOutput*>` acquire/release + a brief `swapLock` for the message-thread side. Audio thread now does a single atomic load and never enters the kernel. A secondary issue (`TracksViewport::tryRenderAll` using `ScopedTryLockType`, which silently dropped track rendering on lock contention) was fixed by introducing a `std::shared_ptr<const std::vector<AudioTrack*>>` snapshot, atomically swapped on add/remove/reorder/clear. Audio thread is now fully lock-free on the track-render path. Also corrected the default `AudioTrack::gain` (was `1.0f`, doc says `0.25`) and made `AudioTrack::currentSampleRate`/`currentBlockSize` atomic (they were `double`/`int` despite being read by `setPlugin` from the message thread while `prepareToPlay` writes them). Build green; 12/12 MidiClip tests pass.
**`AudioTrack::lifetimeToken` false alarm (cancelled).** Listed as dead code in the original audit; verified the token IS used by `SessionIO::loadSession` (`src/session/SessionIO.cpp:242`) and `TracksViewport::createTrackFromFileAsync` (`src/tracks/TracksViewport.cpp:83`) to guard async plugin-load and file-load callbacks. The source's separate `lifetimeToken` is for the internal `loadFileAsync` path. Both tokens are bumped in their respective destructors.
**Refactor — Complete.** `createTrackFromFileAsync` helper extracted; ~50 lines of duplication between `loadSession` and `toggleRecording` removed.
**Stretch: spacebar = sequencer play/stop — Complete.** `MainComponent::keyPressed` toggles `midiTrack.play()/stop()`.
**Stretch: drag-and-drop audio files onto a track row — Complete.** `TrackRow` implements `juce::FileDragAndDropTarget`; row highlights blue on drag-over.
**Stretch: track reorder via drag — Complete.** `MainComponent` derives from `juce::DragAndDropContainer` (mixin); `TrackRow` implements `juce::DragAndDropTarget`. Click+drag from any label/row background starts a drag; hovered row shows a yellow insertion line (top half = above, bottom half = below). Reorder happens under `tracksLock` and calls `refreshLayout()`.

**Bug-fix pass #4 — Complete (audio "no sound / underrun" reinvestigation).** Full audit + log analysis in `docs/audio-underrun-investigation.md`. Net findings: (1) the lock-free pass #3 fix IS in HEAD (`git log` confirms `0965e00` under `dd3c92a`), and a full audio-thread audit found **no remaining blocking path** — every routine uses `ScopedTryLockType`/atomics; (2) the real culprit behind the flood of "Audio underrun suspected" lines was the **diagnostic itself** added in `dd3c92a` (`src/Main.cpp`): it (a) called `juce::Logger::writeToLog` **on the audio thread** (a `CriticalSection` + blocking I/O real-time violation that can self-sustain a real underrun cascade), and (b) misreported **idle/release silence after a brief key-tap** as an underrun (the log's `peak=0.055` is a single one-block tap blip = `velocity*0.15`, then release-silence; the 3 logs at 5.698/5.990/6.298 were 3 taps, not 3 dropped buffers); (3) a benign startup assertion `juce_Component.cpp:3022` (`jassert (isShowing()||isOnDesktop())` in `grabKeyboardFocus`) fired because the constructor grabbed focus before the window was on screen. **Fixes applied (build green):** A) `MainComponent::visibilityChanged()` now calls `grabKeyboardFocus()` guarded by `isShowing()`; the constructor no longer grabs focus; B) all `Logger::writeToLog` removed from `getNextAudioBlock`; audio thread now only increments atomic counters (`audioBlocksProduced`/`audioBlocksSilent`/`realUnderruns`/`lastCallbackMs`) — the 500 ms GUI `Timer` emits the summary; C) underrun detection is now **timing-based** (`getMillisecondCounterHiRes()` gap > 1.5× `bufferSize/sampleRate*1000`), not content-based; D) silence is only counted as suspicious when audio **is expected** (`midiTrack.isPlaying()` || a track `isPlaying()` via new `TracksViewport::anyTrackAudible()` || an on-screen key held) — idle silence is never logged. Minor: `std::atomic<std::shared_ptr>>` (`TracksViewport::tracksSnapshot`) is not lock-free on MSVC (internal spinlock), flagged as a future concern, not the current cause. Also documented doc-vs-code drift: AGENTS.md's Recorder pre-allocation claim was inaccurate — the actual `Recorder.cpp` only stores `recordSampleRate`/`deviceBufferSize` in `audioDeviceAboutToStart`; allocation is lazy on the first recording block (and never happens while `recording=false`).

**Week 0 — Complete.** Toolchain scaffolded, JUCE 8 cloned, hello-world builds and runs.
**Week 3 — Complete.** Audio I/O works end-to-end: `AudioAppComponent` + `SineAudioSource` + `juce::Slider` driving `std::atomic<float>` frequency.
**Week 4 — Complete.** MIDI synth works end-to-end: custom `SineVoice` + `juce::Synthesiser` + `MidiKeyboardState` + on-screen `MidiKeyboardComponent` (5 octaves) + auto-open all USB MIDI inputs.
**Audio track — Complete.** Load WAV / AIFF / FLAC / MP3 via `FileChooser`, decoded by `AudioFormatManager::registerBasicFormats()` into an `AudioBuffer<float>`.
**Refactor — Complete.** Split `src/Main.cpp` into `src/audio/`, `src/midi/`, `src/tracks/` subfolders via `GLOB_RECURSE`.
**FileChooser fix — Complete.** `launchAsync` opened invisibly. Switched to `browseForFileToOpen()` (modal) with `JUCE_MODAL_LOOPS_PERMITTED=1`.
**Multi-track mixer — Complete.** `std::vector<std::unique_ptr<AudioTrack>>` with per-track gain (default 0.25) and mute. `TrackRow` UI with Load/Play/Stop/Gain/Mute/Remove + Add Track button. `getNextAudioBlock` mixes synth + tracks.
**Synth gain slider — Complete.** `synthGainSlider` at the bottom, wired to `std::atomic<float> synthGain{0.6f}`. Committed.
**Mixer polish — Complete (committed to git).** Per-track **Pan slider** (linear -1 to +1), **Solo button** (TextButton toggling, blue when active), **Time progress label** ("0:42 / 3:18" updated by `MainComponent::timerCallback` calling `TrackRow::refreshTimeLabel()`). **Master gain** in the top-right of the main window with `juce::SmoothedValue<float>` (50 ms ramp, no clicks on big volume changes). Default 0.8.
**Loop button (Option 1 — full-file loop) — Complete.** Per-track **Loop** button (green when active). When ON, `AudioTrackSource::getNextAudioBlock` wraps `playPosition` back to 0 at end of buffer instead of stopping. When OFF, plays once and stops (existing behavior). Window size **960×600** is locked.
**VST3 insert per track — Complete.** Per-track **VST3** (load), **Bypass** (toggle, amber), **Edit** (open plugin editor window) buttons + plugin name label below the button row. `PluginHost` owns `AudioPluginFormatManager` (VST3 format) + `KnownPluginList`. `AudioTrack` owns `std::unique_ptr<juce::AudioPluginInstance>`; `renderInto` calls `plugin->processBlock(scratchBuffer, emptyMidi)` after the source renders and before gain/pan/sum. Plugin chooser is a modal `DocumentWindow` with a `ListBox` of scanned plugins + Rescan button; double-click loads async via `createPluginInstanceAsync`. Plugin editor opens in its own `DocumentWindow` via `createEditorAndMakeActive()`. `CMakeLists.txt` defines `JUCE_PLUGINHOST_VST3=1`. Also fixed the throwaway-`TrackRow` wart in `refreshLayout` (now uses static `TrackRow::getPreferredHeight()`).
**Docs — Complete.** `README.md` and `docs/daw-architecture.md` created. Title-bar encoding bug was already fixed in source (plain ASCII `"Simple DAW"`); removed stale TODO.
**A/B region loop (Option 2) — Complete.** `AudioTrackSource` gains `std::atomic<int> loopStart{0}` / `loopEnd{0}`. Default `loopEnd = numSamples` so the existing full-file loop keeps working when A/B is not set. `getNextAudioBlock` clamps the playhead to `[loopStart, loopEnd)` and wraps to `loopStart` (not 0) when `looping` is on. **A/B** button in `TrackRow` opens a collapsible second row with **Set A** / **Set B** buttons (capture current playhead), two horizontal sliders for fine adjustment, a **Clear** button, and a status label showing region length. `TrackRow` height is dynamic: 76 px collapsed, 126 px with A/B panel open. `MainComponent` wraps `tracksContainer` in a `juce::Viewport` (220 px tall) so a row with the A/B panel open can no longer push the MIDI keyboard off-screen.
**Piano roll / sequencer Phase 1 (data + engine) — Complete.** `MidiNote { pitch, startBeat, lengthBeats, velocity }`, `MidiClip` (owns `std::vector<MidiNote>`, `loadDemoMelody()` pre-populates a C major scale, 8 beats), `MidiTrack : juce::AudioSource` (owns `MidiClip` + `juce::Synthesiser` with 8 `SineVoice`s + beat clock). `getNextAudioBlock` advances the beat clock by `numSamples/sampleRate * bpm/60`, emits sample-accurate `noteOn`/`noteOff` into a `MidiBuffer`, renders through the synth. Handles loop wrapping (split block at wrap point, kill held notes, re-emit from beat 0) and non-loop end (stop + kill all). `MainComponent` adds a sequencer row: **Seq Play** / **Seq Stop** / **Loop** / **BPM** slider (40-240, default 120) / beat position label (`beat: 3.0 / 8.0`). MIDI track output mixed into master bus with its own `midiGain` slider (default 0.5).
**Piano roll Phase 2 (UI) — Complete.** `PianoRollComponent : juce::Component + juce::Timer` (30 fps playhead). Left piano keys (click to audition via `MidiKeyboardState`), grid with bar/beat lines, black/white key row backgrounds. **Drag-to-draw** new notes (snap to grid), **drag to move** notes (pitch + beat), **drag right edge to resize**, **right-click to delete**. Snap combo: 1/4, 1/8, 1/16, 1/16T (triplet). Red playhead line reads `MidiTrack::getCurrentBeat()`. Opens in a `PianoRollWindow : juce::DocumentWindow` (760×440, resizable, native title bar, self-deletes on close). `MainComponent` tracks the open window via raw pointer + `onClosed` callback (deletes in destructor). `MidiClip` gained a `juce::CriticalSection lock`; `PianoRollComponent` uses `ScopedLock` when modifying notes, `MidiTrack::emitNoteOns` uses `ScopedTryLock` (skips note scanning if locked — tries again next block).
**Piano roll improvements (velocity lane + zoom + scroll) — Complete.** `PianoRollComponent` now wraps a scrollable `PianoRollContent` in a `juce::Viewport` (horizontal scroll for clips wider than the window). Toolbar adds **`-`/`+` zoom buttons** (30–200 px/beat, default 82) and a **Clear All** button. A **velocity lane** sits below the grid (70 px tall): each note gets a blue vertical bar whose height = `velocity/127 * laneHeight`. **Click+drag a bar to edit velocity** (1–127, clamped). Playhead extends through the velocity lane. Info label in toolbar shows clip length and current zoom.
**Recording — Complete.** `src/audio/Recorder.{h,cpp}` — `AudioIODeviceCallback` registered as a second callback on the device manager (alongside `AudioAppComponent`). On `audioDeviceIOCallbackWithContext` it copies the input channels into a growing `AudioBuffer<float>` protected by a `CriticalSection`. **Rec** button in the top row (red when active) starts/stops. On stop, writes the buffer to a timestamped WAV in `%USERPROFILE%\Documents\Simple DAW Recordings\` (auto-created) via `WavAudioFormat` and creates a new `AudioTrack` that loads it. `prepareToPlay` adds the callback, `releaseResources` removes it. Runtime test pending (requires an input-capable device like Komplete Audio 1).
**Master peak meter — Complete.** `src/ui/PeakMeterComponent.h` — custom `Component + Timer` (30 fps) that reads `std::atomic<float>&` master peak via `exchange(0.0f)` (atomic read+reset), applies exponential decay (`max(p, displayed * 0.88f)`), and paints a log-scale bar (-60 dB to 0 dB, green < -24 dB, yellow < -12 dB, red >= -12 dB). Audio thread in `MainComponent::getNextAudioBlock` computes `getMagnitude` per channel after the master gain is applied and stores the max. Meter is placed in the top row to the left of the Master gain slider.
**ASIO audio settings panel — Complete.** `Settings` button in the top row opens a `SettingsWindow : juce::DocumentWindow` (500×400, resizable) containing a `juce::AudioDeviceSelectorComponent` wired to the existing `deviceManager`. User can switch from WASAPI to ASIO (Komplete Audio), change buffer size, and sample rate. Changes apply immediately. `MainComponent` tracks the window via raw pointer + `onClosed` callback (deletes in destructor).
**JSON save/load — Complete.** **Save** and **Load** buttons in the top row. Save serialises the full session to a `.sdaw` JSON file (master/synth/midi gains, BPM, MIDI clip notes, audio tracks with file path + gain/pan/mute/solo/loop/A/B region, MIDI output device name, VST3 plugin identifier + state + bypass). Load restores everything, re-loading audio files from their saved paths (skips tracks whose file is missing), and async-recreates the plugin (skips tracks whose plugin is no longer in the known list). Uses `juce::DynamicObject` + `juce::JSON::writeToStream` / `juce::JSON::parse`. `AudioTrackSource` gained a `loadedFilePath` field + `getFilePath()`. `AudioTrack` gained a `pluginDesc` field + `getPluginDesc()` so save/load can match plugins by `createIdentifierString()`. `TrackRow` gained `setNameText()` for restoring the name label after load.
**MIDI output routing — Complete.** `src/midi/MidiOutputRouter.h` — owns a `juce::MidiOutput` (opened via `openDevice` + `startBackgroundThread`). `MidiOutputRouter::sendBuffer` forwards a `juce::MidiBuffer` to the device via `sendMessageNow` (safe from the audio thread). `SynthAudioSource` and `MidiTrack` now expose `getLastMidiBuffer()` so `MainComponent::getNextAudioBlock` can forward the keyboard-state MIDI and the sequencer MIDI to the router right after each source renders. A `juce::ComboBox` ("MIDI Out") in the top row lists available outputs (refreshed by `refreshMidiOutputCombo()`); selecting `(none)` closes the output, selecting a device name opens it. Switching devices sends all-notes-off on all 16 channels first. Selected output name is persisted in the `.sdaw` session file under `midiOutput`. `~MainComponent` sends all-notes-off + closes the output before `shutdownAudio`.
**Piano roll copy/paste + undo/redo — Complete.** `MidiClip` has `undoStack`/`redoStack` of `std::vector<MidiNote>` snapshots (cap 200). `PianoRollContent` tracks `std::set<int> selectedIndices` (cleared on structural changes). `Ctrl+Z`/`Ctrl+Y` (or `Ctrl+Shift+Z`) undo/redo; `Ctrl+C`/`Ctrl+V`/`Ctrl+X` copy/paste/cut; `Ctrl+A` select all; arrow keys nudge selected by ±1 pitch / ±snap beats; `Del`/`Backspace` delete. Toolbar adds `Undo`/`Redo`/`Copy`/`Paste` buttons next to the existing zoom/clear. Selected notes render in amber; drag-rect on empty space marquee-selects. State is pushed at the natural point of each operation (mouseDown for single-shot, first mouseDrag for drags, mouseUp for create).
**Bug-fix pass — Complete.** Stuck-note in piano roll keys fixed via `mouseExit` handler. Plugin scan moved to `PluginScanThread : juce::Thread` with `callAsync` results; status label polls. Audio file load moved to `std::thread` with `callAsync` buffer swap; `TrackRow` shows `(loading...)`. `loadSession` defaults BPM to 120 if `bpm` property missing. Recorder falls back to 48000 Hz if `recordSampleRate` is 0.

### What's working

- `CMakeLists.txt` — JUCE 8 GUI app, `juce_generate_juce_header`, links `juce_audio_utils`/`juce_audio_devices`/`juce_audio_formats`/`juce_audio_processors`/`juce_dsp`/`juce_gui_extra` (must be explicit for `JuceHeader.h` aggregation). Defines `JUCE_MODAL_LOOPS_PERMITTED=1`, `JUCE_PLUGINHOST_VST3=1`. Uses `file(GLOB_RECURSE ... CONFIGURE_DEPENDS "src/*.cpp")`.
- `src/Main.cpp` — `MainComponent : juce::AudioAppComponent + MidiInputCallback + Timer + juce::DragAndDropContainer`. The `DragAndDropContainer` mixin makes it discoverable by `findParentDragContainerFor(row)` for the track-reorder drag. `tracksContainer` is wrapped in a `juce::Viewport` (220 px tall) so a row with the A/B panel open (126 px) can no longer push the MIDI keyboard off-screen.
  - Owns `SynthAudioSource` (synth), `PluginHost pluginHost`, and `std::vector<std::unique_ptr<AudioTrack>> tracks;`
  - On-screen `MidiKeyboardComponent` (5 octaves, C2 to C7) wired to synth's `MidiKeyboardState`
  - `+ Add Track` button creates a new `AudioTrack` + `TrackRow` (passing `pluginHost`); `X` on a row removes it
  - `Scan VST3` button triggers `pluginHost.scanForPlugins()` off the message thread
  - `Master` label + `masterGainSlider` (0-2, default 0.8, `SmoothedValue` ramp 50 ms) at the top right
  - `MIDI Out` combo box in the top row routes on-screen keyboard + sequencer notes to a selectable `juce::MidiOutput` via `MidiOutputRouter` (persisted in `.sdaw`)
  - `Synth` label + `synthGainSlider` (0-2, default 0.6) at the bottom
  - `prepareToPlay` forwards to `pluginHost.setSampleRate` / `setBlockSize` so async plugin instantiation uses the real device rate
  - `getNextAudioBlock` mixes synth + all audio tracks (mute + gain + pan + plugin insert, with solo logic), then applies smoothed master gain
  - Status label refreshed by `Timer` every 500 ms: device / buffer / rate / MIDI ports / per-track state. `Timer` also calls `refreshTimeLabel()` on every row.
- `src/audio/AudioTrackSource.{h,cpp}` — `AudioSource` subclass. `playing`, `looping`, `loopStart`, `loopEnd` are `std::atomic`; `playPosition` is `std::atomic<int>`. `getNextAudioBlock` clamps the playhead to `[loopStart, loopEnd)` and wraps to `loopStart` when `looping` is on. `setLoopStart` / `setLoopEnd` / `clearLoopRegion` for the A/B UI. `loadFile` initialises `loopStart=0`, `loopEnd=numSamples` (full-file fallback when A/B not set).
- `src/midi/SineVoice.{h,cpp}` — 8-voice polyphonic sine; velocity → amplitude; `MidiMessage::getMidiNoteInHertz` → frequency
- `src/midi/DemoSound.{h,cpp}` — minimal `juce::SynthesiserSound` subclass
- `src/midi/SynthAudioSource.{h,cpp}` — owns `juce::Synthesiser` (8 `SineVoice`s + 1 `DemoSound`) and `juce::MidiKeyboardState`. Exposes `getLastMidiBuffer()` for MIDI output routing.
- `src/midi/MidiTrack.{h,cpp}` — `MidiTrack : juce::AudioSource`. Owns a `MidiClip` + `juce::Synthesiser` (8 `SineVoice`s + 1 `DemoSound`) + beat clock. `getNextAudioBlock` advances `currentBeat` by `numSamples/sampleRate * bpm/60`, emits sample-accurate `noteOn`/`noteOff` into a `MidiBuffer`, renders through the synth. Tracks `heldNotes` (pitch + endBeat) for note-offs that happen in later blocks. Loop wrapping splits the block at the wrap point, kills held notes, re-emits from beat 0. `playing`/`looping`/`bpm`/`currentBeat` are atomic. `loadDemoMelody()` pre-populates a C major scale (8 notes, 1 beat each, 0.9 length, velocity 80). Exposes `getLastMidiBuffer()` for MIDI output routing.
- `src/midi/MidiOutputRouter.h` — owns a `juce::MidiOutput` (opened via `openDevice` + `startBackgroundThread`). `sendBuffer` forwards a `juce::MidiBuffer` to the device via `sendMessageNow` (safe from the audio thread). `sendAllNotesOff` sends all-notes-off on all 16 channels (used on device switch + shutdown).
- `src/midi/MidiClip.{h,cpp}` — owns `std::vector<MidiNote>` + `undoStack`/`redoStack` of `std::vector<MidiNote>` snapshots (cap 200). Public methods: `addNote` / `addNotes` / `removeNote` / `removeNotes(set<int>)` / `clearNotes` / `replaceAllNotes(vector)` / `beginEdit` / `undo` / `redo` / `canUndo` / `canRedo` / `clearUndoHistory`. Each mutating method calls `pushState()` under the `CriticalSection lock` BEFORE mutating. `loadDemoMelody` clears the undo history.
- `src/plugin/PluginHost.{h,cpp}` — wraps `juce::AudioPluginFormatManager` (VST3 format registered) + `juce::KnownPluginList`. `scanForPlugins()` runs synchronously. `scanForPluginsAsync()` runs on a `PluginScanThread : juce::Thread` that walks `*.vst3` files under the standard VST3 search paths and calls `AudioPluginFormat::findAllTypesForFile` per file. Each newly-found `PluginDescription` is posted to the message thread via `callAsync`, where it's added to the `KnownPluginList` and a `ChangeMessage` is broadcast. `isScanning()` / `getScanFoundCount()` let the dialog show `"Scanning... (N found)"`. `createInstanceAsync(desc, cb)` forwards to `formatManager.createPluginInstanceAsync`. Stores `sampleRate`/`blockSize` set by `MainComponent::prepareToPlay`.
- `src/plugin/PluginWindows.h` — `PluginChooserDialog` (modal `DocumentWindow` + `ListBoxModel` + `ChangeListener` + `Timer`; lists `KnownPluginList::getTypes()`, double-click loads, Rescan button) and `PluginEditorWindow` (`DocumentWindow` hosting `createEditorAndMakeActive()`, **defers `delete this` via `MessageManager::callAsync` in `closeButtonPressed`**). Uses `using juce::Component::addAndMakeVisible;` to unhide the reference overload hidden by `ResizableWindow`. The dialog polls `pluginHost.isScanning()` and `getScanFoundCount()` every 150 ms and shows `"Scanning... (N found)"` while a scan is in flight.
- `src/tracks/AudioTrack.{h,cpp}` — `class AudioTrack` owning `AudioTrackSource` + scratch buffer + `gain`/`pan`/`mute`/`solo` atomics + `juce::SmoothedValue<float> gainSmoothed` (50 ms ramp) + `std::unique_ptr<juce::AudioPluginInstance> plugin` + `std::unique_ptr<juce::PluginDescription> pluginDesc` + `std::atomic<bool> pluginBypass` + `juce::MidiBuffer pluginMidiBuffer` + `renderInto()`. `setPlugin(plugin, desc)` calls `prepareToPlay` + `setBusesLayout` (stereo in/out) and stores the `PluginDescription` so it can be serialised in `.sdaw`. `renderInto` runs `plugin->processBlock(scratchBuffer, pluginMidiBuffer)` after the source renders, then applies gain via `applyGainRamp` (sample-accurate, from `gainSmoothed.getCurrentValue()` to `getCurrentValue()` after `skip(numSamples)`), measures peak post-gain via `getMagnitude` (stored in `std::atomic<float> peak` via max-hold), then adds to the destination with `panGain` (skipped if muted, skipped on solo-exclusion). `getPeakRef()` exposes the atomic to the meter UI. `getPluginDesc()` returns the stored description. `loadFileAsync` is lifetime-safe via a `std::shared_ptr<std::atomic<int>>` token that gets bumped in the destructor; the async callback checks the token before touching `this`. `~AudioTrack` calls `releaseResources()` first, then bumps the lifetime token.
- `src/tracks/TrackRow.{h,cpp}` — per-track `Component`, height dynamic (76 px collapsed, 126 px with the A/B panel open via `getCurrentPreferredHeight()`). Row 1: name label + plugin label. Row 2 (left → right): Load / Play / Stop / time label / **track meter** / Gain / Pan / Mute / Solo / **Loop** / **A/B** / **VST3** / **Bypass** / **Edit** / X. When A/B is toggled on, an extra row appears with **Set A** / **Set B** / **Clear** buttons, start/end sliders, value labels (`A: 1:23.45` / `B: 2:01.30`), and a region-length status label. `TrackRow` takes `PluginHost&` as a second constructor arg. `openPluginChooser()` opens a `PluginChooserDialog`; the async creation callback calls `track.setPlugin` then `refreshPluginLabel`/`updateButtons` on the message thread. `showPluginEditor()` news a `PluginEditorWindow`. Same `setTextBoxIsEditable(false)` on sliders. Same `TextButton + setClickingTogglesState(true)` on Mute/Solo/Loop/Bypass/A/B. `openFile` is async: shows `(loading...) filename.ext` immediately, then `loadFileAsync` swaps the buffer and updates the name label on completion. Lifetime-safe via the `alive` shared_ptr flag. **Implements `juce::FileDragAndDropTarget`** — accepts audio files (`.wav`/`.aif`/`.aiff`/`.flac`/`.mp3`/`.ogg`) dropped onto the row; highlights blue on drag-over, calls `loadFileAsync` on drop. **Implements `juce::DragAndDropTarget` + drag source** — a `TrackRowDragStarter : MouseListener` registered on the row + `nameLabel` + `timeLabel` + `pluginLabel` calls `startDragging("TrackRowReorder", row)`; hovered row paints a yellow insertion line (top half = above, bottom half = below) and `itemDropped` calls `MainComponent::reorderTrack`. Buttons and sliders don't have the listener, so they keep working as normal.
- `build-dev.bat` — initializes VsDevCmd, then CMake configure + build
- `.gitignore` — excludes `build/`, `third_party/`, `.vs/`
- `docs/learning-plan-juce-dsp.md`, `docs/learning-plan-week-0-2-3.md`

### Verified

- Build: `build-dev.bat Debug` produces `build\SimpleDaw_artefacts\Debug\Simple DAW.exe`
- Audio: sine tone + multi-track WAV playback through Komplete Audio 1 WASAPI (Speakers, 480-sample buffer @ 48 kHz)
- MIDI: on-screen keyboard + auto-opened Komplete Audio 1 MIDI ports work
- Mixer: user confirmed two different WAVs play simultaneously, status line shows per-track state
- Per-track Pan, Solo, Mute, Loop, time progress — all wired and functional
- Master gain with `SmoothedValue` (no clicks on volume changes)
- VST3 hosting builds; `Scan VST3` button + per-track Load/Bypass/Edit wired (runtime test pending user having VST3s installed)
- Window size 960×600 fits the user's screen
- Toolchain: MSVC 19.44.35228, CMake 4.3.3 (winget), Git 2.53.0, SDK 10.0.26100.0

### Known quirks

- **CMake not on PATH by default.** winget installed it to `C:\Program Files\CMake\bin\cmake.exe`. `build-dev.bat` adds it.
- **`vcvars64.bat` path:** `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat`
- **Output exe has a space:** JUCE nests it at `build\SimpleDaw_artefacts\<Config>\Simple DAW.exe`
- **OneDrive caveat:** if MSBuild complains about read-only files, redirect to `C:\Users\User\AppData\Local\Temp\opencode\simple-daw-target\`
- **`JuceHeader.h` aggregation pitfall:** modules must be named in `target_link_libraries` explicitly. Delete `build\SimpleDaw_artefacts\JuceLibraryCode\` to force regen.
- **`juce::SynthesiserSound` is not directly instantiable.** Subclass it as `DemoSound`.
- **`MidiKeyboardComponent` has no `setNumOctavesForKeyboard`.** Use `setAvailableRange(int, int)` and `setKeyWidth(float)`.
- **`MidiKeyboardComponent` colour IDs are limited.** `mouseDownKeyFillColourId` does NOT exist on this class.
- **`MidiInput::openDevice` returns by value, not pointer.** `if (input) { input->start(); openedInputs.add(input.release()); }`
- **`MidiDeviceInfo` has no `isEnabled()`.** Just iterate; failure returns empty.
- **MIDI callback method is `handleIncomingMidiMessage`** (not `midiInputCallback`).
- **`FileChooser::launchAsync` opens invisibly behind the main window on Windows.** Use `browseForFileToOpen()` (modal) with `JUCE_MODAL_LOOPS_PERMITTED=1`.
- **`JUCE_MODAL_LOOPS_PERMITTED` defaults to 0 in Debug.** Already set in `CMakeLists.txt`.
- **`FileChooser::browseForFileToOpen` signature is `bool browseForFileToOpen(FilePreviewComponent* = nullptr)`** — no `int` flags parameter.
- **`juce::Slider` with `TextBoxRight` is editable by default.** Always call `setTextBoxIsEditable(false)` on drag-only sliders; otherwise a click in the text box can swallow focus from a nearby button.
- **`juce::ToggleButton` is a poor fit for short labels.** Use `TextButton` + `setClickingTogglesState(true)`.
- **`Component::setColour` takes 2 args:** `(colourId, colour)`. `backgroundColourId` is on `ResizableWindow`/`Label`, not `Component` — override `paint()` for plain backgrounds.
- **Data race in audio thread if `playing` is a plain `bool`.** `AudioTrackSource::playing`, `looping`, and `playPosition` are now `std::atomic`. The `pan/gain/mute/solo` on `AudioTrack` are also atomic.
- **VST3 hosting needs `JUCE_PLUGINHOST_VST3=1`.** Without it `juce::VST3PluginFormat` is not defined (guarded by `JUCE_INTERNAL_HAS_VST3`).
- **`AudioPluginFormatManager::addFormat(AudioPluginFormat*)` is deprecated.** Use `addFormat(std::make_unique<...>())`.
- **`AudioPluginFormatManager::createPluginInstance` is synchronous** (blocks message thread on large plugins). Use `createPluginInstanceAsync(desc, rate, blockSize, callback)` for UI-driven loads. Callback signature: `std::function<void(std::unique_ptr<AudioPluginInstance>, const String&)>`.
- **`KnownPluginList` has no `getFormatForPluginFormat`.** Iterate `AudioPluginFormatManager::getNumFormats()` / `getFormat(i)` and match `getName() == "VST3"`.
- **`PluginDirectoryScanner::scanNextFile` takes 2 args:** `(bool dontRescanIfAlreadyInList, String& nameOfPluginBeingScanned)`.
- **`ResizableWindow::addAndMakeVisible(Component*, int)` hides `Component::addAndMakeVisible(Component&, int).** Add `using juce::Component::addAndMakeVisible;` inside `DocumentWindow` subclasses that add children directly.
- **`AudioProcessor::createEditorIfNeeded()` is deprecated in JUCE 8.** Use `createEditorAndMakeActive()` (same behaviour).
- **`AudioFormatManager::registerBasicFormats()`** bundles WAV/AIFF/FLAC/MP3 decoders.
- **`AudioIODeviceCallback::audioDeviceIOCallback` was removed in JUCE 8.** Use `audioDeviceIOCallbackWithContext(input, numIn, output, numOut, numSamples, context)` — the new signature has a trailing `const AudioIODeviceCallbackContext&` parameter.
- **`AudioFormat::createWriterFor(stream, rate, ch, bits, meta, q)` is deprecated in JUCE 8.** Still works, but emits a deprecation warning. Future-proof: use the `AudioFormatWriterOptions` overload.
- **`File::deleteFile()`** is the method name (not `delete()`, which is a keyword and can confuse the parser in some contexts).
- **`FileChooser::browseForFileToSave(bool warnAboutOverwritingExistingFiles)`** takes a bool — the `browseForFileToOpen()` variant takes `nullptr` by default.
- **`JSON::writeToStream(OutputStream&, const var&, bool pretty)`** — there is no `JSON::writeToFile`. Use `File::createOutputStream()` + `writeToStream()`.
- **`juce::var` does not implicitly convert to `juce::String`.** Use `var.toString()` or `static_cast<juce::String>(var)`.
- **WASAPI buffer size is 480** on the Komplete Audio 1. Switch to Komplete Audio ASIO for ~2-3 ms latency.
- **Window size is 960×600** — keep this as the default; user confirmed it fits. Do not bump above 600 without checking.
- **`juce::Viewport` inserts an extra component layer** between the viewed component and the viewport's parent. A child that calls `getParentComponent()->...->resized()` to reach the top-level window will land on the viewport (or its internal view holder), not `MainComponent`. **Use an explicit callback (e.g. `onLayoutChanged`) instead of walking the parent chain** when a child needs to trigger a parent layout. `getTopLevelComponent()->resized()` also fails: it returns the `DocumentWindow`, and `DocumentWindow::resized()` calls `setContentOwned`-equivalent with the *same* bounds, which is a no-op in JUCE — so `MainComponent::resized()` never fires. The only reliable pattern is a direct callback to `MainComponent::refreshLayout()`.
- **`Component::setBounds` is a no-op when bounds are unchanged.** Do not rely on calling `resized()` on an ancestor to propagate layout; call the layout function directly.
- **`juce::OwnedArray` owns its elements.** `OwnedArray::clear()` deletes all objects by default. Do NOT manually `delete` elements and then call `clear()` — that is a double-free. Just stop/release the objects, then call `clear()` (or let the destructor handle it).
- **`juce::SpinLock` uses `SpinLock::ScopedLockType` / `SpinLock::ScopedTryLockType`**, not `juce::ScopedLock` / `juce::ScopedTryLock` (those are for `CriticalSection`). The audio thread should use `ScopedTryLockType` and skip the block if contended (a brief silence glitch is better than a crash).
- **GUI ↔ audio structural mutations must be synchronized.** `MainComponent` has a `juce::SpinLock tracksLock` — the audio thread try-locks in `getNextAudioBlock` (skips track rendering if contended, but still renders synth + MIDI track), and all GUI mutations (`addTrack`/`removeTrack`/`loadSession`/`toggleRecording`/destructor) hold the lock. Per-track `AudioTrackSource::bufferLock` protects `fileBuffer` during `loadFile` vs `getNextAudioBlock`. Per-track `AudioTrack::pluginLock` protects `plugin` during `setPlugin`/`clearPlugin` vs `processBlock` in `renderInto`. `MidiTrack` uses `std::atomic<bool> shouldResetHeldNotes` to defer `heldNotes.clear()` to the audio thread (never clear on the GUI thread).
- **`PluginEditorWindow` uses a static factory (`open()`)** that returns `nullptr` if `createEditorAndMakeActive()` fails, instead of `delete this` in the constructor (which is UB).
- **`TrackRow` tracks open editor windows and uses a `std::shared_ptr<bool> alive` flag** to guard async plugin-load callbacks. If the TrackRow is destroyed before the async `createPluginInstanceAsync` callback fires, `*aliveFlag` is false and the callback returns without touching `this`. The destructor closes any open editor window before the track/plugin is freed.
- **Scratch buffers are pre-allocated in `prepareToPlay`.** `MainComponent::getNextAudioBlock` only calls `setSize` if the buffer is too small (grow-only), avoiding per-block heap allocation on the audio thread.
- **`MidiClip::lengthBeats` is `std::atomic<double>`** so the audio thread can safely read it (in `MidiTrack::getNextAudioBlock`) while the GUI thread writes it (in `loadSession`).
- **`MidiTrack` snapshots notes under try-lock.** At the start of each block, `getNextAudioBlock` try-locks `clip.lock`, copies the notes vector to a pre-allocated `notesSnapshot`, and releases. `emitNoteOns` scans the snapshot (no lock needed). If the try-lock fails (piano roll is mid-edit), the previous snapshot is used — a brief stale read is better than permanent note drops or unsafe iteration.
- **`SineVoice::canPlaySound` checks for `DemoSound`** (not the base class), so voices won't steal notes from other sounds if more are added later.
- **`PluginChooserDialog::closeButtonPressed` defers `delete this`** via `MessageManager::callAsync` to avoid fragile synchronous destruction after `exitModalState`. The same pattern is now used by `SettingsWindow`, `PluginEditorWindow`, and `PianoRollWindow` (see the broader quirk above about never destroying Components synchronously from a callback).
- **`MainComponent` has an `alive` flag too.** `std::shared_ptr<bool> alive` is flipped to `false` first thing in `~MainComponent`. The `onClosed` callbacks for `pianoRollWindow` and `settingsWindow` capture it and skip the `pianoRollWindow = nullptr` / `settingsWindow = nullptr` write when `*alive` is false. This protects against the race where the user clicks a subwindow's X, the deferred delete is queued, and the app quits within one message-loop tick — in that case the callback would otherwise write to a freed `MainComponent`.
- **Spacebar toggles the sequencer** via `MainComponent::keyPressed`. The component grabs keyboard focus in `visibilityChanged()` (guarded by `isShowing()` — never in the constructor, which used to fire the `juce_Component.cpp:3022` `jassert (isShowing()||isOnDesktop())`). Subwindows (Piano Roll, Audio Settings, VST3 Editor) grab their own focus when opened, so spacebar routes to whichever component has focus.
- **Drag-and-drop audio files onto a `TrackRow`.** `TrackRow` implements `juce::FileDragAndDropTarget` (not the full `DragAndDropContainer` — just the file-drag interface). Accepts any of `.wav`/`.aif`/`.aiff`/`.flac`/`.mp3`/`.ogg`. The row's background turns blue while a valid drag hovers, then resets on drop. Drops use the existing async `loadFileAsync` path with the alive-flag guard, so a drop that lands after the row was destroyed is a no-op.
- **Track reorder via drag.** `MainComponent` derives from `juce::DragAndDropContainer` (mixin, NOT a `Component` subclass — see `juce_DragAndDropContainer.h`). `findParentDragContainerFor` walks up the parent chain via `Component::findParentComponentOfClass`, so the container must be an ancestor of the drag source. In our case the source is a `TrackRow` deep inside `tracksContainer`/`tracksViewport`, so the container is the top-level `MainComponent`. `TrackRow` implements `juce::DragAndDropTarget` and is the source via a `TrackRowDragStarter : MouseListener` registered on the row + non-interactive labels (buttons/sliders keep their own click handling because the listener isn't registered on them). Drop position is determined by `localPosition.y` against `getHeight()/2` (top half = above, bottom half = below), rendered as a 2-px yellow line. The reorder runs under `tracksLock` to keep the audio thread safe.
- **JUCE mouse events do NOT auto-bubble.** Unlike the DOM, `Component::mouseDown` is called only on the deepest component under the mouse; the parent's `mouseDown` is NOT invoked automatically. To catch clicks on non-interactive children (labels), register a `MouseListener` on the child with `addMouseListener(listener, false)`. The listener is called in addition to (not instead of) the deepest component's handler. To get clicks only on the component's own background (not on children), use `addMouseListener(listener, false)` on the parent and don't register on the children. The `wantsEventsForAllChildComponents` arg controls this.
- **NEVER destroy a `juce::Component` synchronously from inside its own (or its child's) callback.** This is the textbook JUCE use-after-free that trips `_CrtIsValidHeapPointer` (Win32 Debug CRT heap assertion) on the next allocation/deallocation. The classic trap: a `TrackRow`'s `removeButton.onClick` lambda calls `MainComponent::removeTrack(this)`, which erases the `TrackRow` from `trackRows` — destroying the `Button` whose callback is still on the call stack. When the lambda returns, JUCE's `Button` dispatch touches freed memory and corrupts heap metadata. **Fix pattern:** defer the destruction to the next message-loop iteration with `juce::MessageManager::getInstance()->callAsync([...]{ ... })`. Calls that are safe to do immediately (e.g. `removeChildComponent`, which just unparents) can stay synchronous; only the `unique_ptr` reset / `erase` / `delete` must be deferred. This applies to *any* self-deletion path: `Button::onClick`, `Slider::onValueChange`, `ComboBox::onChange`, `Timer::timerCallback` that deletes the timer's owner, etc. If a component needs to delete itself or a sibling from within its own callback, always go through `callAsync`.
- **`MainComponent::removeTrack` now defers `trackRows.erase`/`tracks.erase` via `callAsync`.** `removeChildComponent(row)` runs immediately (safe — just unparents the row); the `unique_ptr` reset happens on the message thread. This was the root cause of the `_CrtIsValidHeapPointer` assertion when clicking the **X** button on a track row.
- **`MidiClip` undo/redo is snapshot-based.** Each user mutation (add, remove, move, resize, velocity, paste, clear) captures a copy of the `notes` vector on the undo stack and clears the redo stack. Cap is `200` entries (`maxUndoSteps`). `pushState()` MUST be called BEFORE the mutation, holding the clip lock. The audio thread's `tryLock` + snapshot path is unchanged.
- **Piano roll selection uses index sets, cleared on undo/redo/paste/cut.** Indices are unstable across add/remove, so we always re-validate the set after any mutation and drop invalid entries. This is simpler than a note-ID system and acceptable for v1.
- **Clipboard is process-local** (a `static std::vector<MidiNote>` inside `PianoRollContent`). Notes are normalised to start at beat 0 and use the lowest pitch as 0 so paste-at-cursor offsets work cleanly.
- **Piano roll key shortcuts are handled via `Component::keyPressed`** on `PianoRollComponent` (set `setWantsKeyboardFocus(true)`, call `grabKeyboardFocus()` after the window opens). Shortcuts: `Ctrl+Z` undo, `Ctrl+Y`/`Ctrl+Shift+Z` redo, `Ctrl+C` copy, `Ctrl+V` paste, `Ctrl+X` cut, `Del`/`Backspace` delete, `Ctrl+A` select all. Plain `Z`/`Y` are NOT shortcuts (would collide with note labels in future).
- **Plugin scan runs on a `PluginScanThread : juce::Thread`.** `PluginHost::scanForPluginsAsync()` starts a background thread that loops `scanNextFile` and posts each newly-discovered plugin to the `KnownPluginList` via `callAsync` (KnownPluginList is not thread-safe). The dialog's status label shows `"Scanning... (N found)"` updated from a `Timer` polling the count.
- **Audio file load is async.** `AudioTrackSource::loadFileAsync` reads + decodes the file on a `std::thread` and posts a swap of `fileBuffer` back to the GUI thread via `callAsync`. `TrackRow` shows a `(loading...)` label in the name area while in flight. Loading is guarded by an `std::atomic<bool> loadingFlag`; cancelling a track remove while loading uses the `alive` shared_ptr flag.
- **Piano roll key-column mouse-exit releases the audition note.** `PianoRollComponent` tracks an `auditionPitch` and releases on `mouseUp` AND on `mouseExit` (in case the drag leaves the window). The `mouseExit` check requires the pitch to be set AND the event's original component to be this component.
- **`DocumentWindow` subclasses with deferred `delete this` need an atomic `closing` flag** so the deferred delete is idempotent. The pattern: `std::atomic<bool> closing{false}`; `closeButtonPressed` uses `compare_exchange_strong` to claim the delete (idempotent across multiple clicks and across the destructor-vs-deferred race). Destructors call `closeButtonPressed()` instead of synchronous `delete` so the same atomic protects both paths. Applied to `PluginEditorWindow`, `PianoRollWindow`, `SettingsWindow`.

---

## Toolchain locations

| Tool | Path |
|------|------|
| VsDevCmd | `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat` |
| vcvars64 | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat` |
| cl.exe | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe` |
| CMake | `C:\Program Files\CMake\bin\cmake.exe` |
| JUCE | `third_party/JUCE/` (depth-1 clone, **pinned** to `3ba67d4` — see `third_party/JUCE.commit` and `docs/juce-pinning.md`) |
| Audio device | Komplete Audio 1 (Native Instruments) via Komplete Audio Driver (ASIO) |

---

## Build commands

```powershell
cd "C:\Users\User\OneDrive\2026\Viibe Codng Stuff\simple-daw"
.\build-dev.bat           # Debug build (default)
.\build-dev.bat Release   # Release build
.\run-tests.bat           # Build + run MidiClip unit tests
```

Output: `build\SimpleDaw_artefacts\<Config>\Simple DAW.exe`

To launch: `& "build\SimpleDaw_artefacts\Debug\Simple DAW.exe"` from PowerShell, or double-click in Explorer.

To see MIDI / audio logs while running, launch from PowerShell so the stdout is visible.

---

## Next session — pick up here

**Bug-fix pass #4 — Complete (code changes).** Audio "no sound / underrun" reinvestigation closed out: the diagnostic added in `dd3c92a` was the real culprit (logged on the audio thread + misreported idle silence). All audio-thread logging removed; underrun detection is now timing-based; idle silence no longer logs; startup `grabKeyboardFocus` assertion fixed via `visibilityChanged()`. Build green; next is the runtime verification protocol in `docs/audio-underrun-investigation.md` (hold an on-screen key ~5 s — meter sustains + tone ⇒ done; meter sustains + silence ⇒ real device delivery issue). Old `Bug-fix pass #3 — IN PROGRESS` notes are retained for history below; the three `DocumentWindow` double-frees + the rest of pass #3 are all already fixed.

**Stretch: track reorder via drag — Complete.** `MainComponent` derives from `juce::DragAndDropContainer`; `TrackRow` is a `juce::DragAndDropTarget` with a `MouseListener` drag starter registered on the row + labels. Yellow insertion line shows the drop position.

**Recommended next: pick a stretch task.** Core DAW is functional: multi-track audio + VST3 inserts + A/B loop + sequencer + piano roll (selection, clipboard, undo/redo, velocity lane, zoom, scroll) + master meter + ASIO settings + JSON save/load + recording + MIDI output routing + per-track gain smoothing + file drag-and-drop + spacebar transport + track reorder via drag. Remaining items are polish.

### Stretch tasks (pick any)
- **Release build** benchmark. (done — Debug 27.9 MB, Release 7.2 MB)
- **App icon** via `juce_add_app_icon` or manual `.ico`.

### Other stretch tasks (pick any)
- **Track reorder via drag** — `juce::DragAndDropContainer` mixin on `MainComponent`, `DragAndDropTarget` on `TrackRow`. (done — listed for reference)
- **ASIO audio settings panel** — switch from WASAPI to Komplete Audio ASIO for ~2-3 ms latency. (done — listed for reference)
- **Master peak meter** — `getMagnitude(channel, 0, numSamples)` repainted from a `Timer`. (done — listed for reference)
- **JSON save/load** — restore tracks + plugin state on relaunch. (done — listed for reference)
- **`juce::MidiOutput`** — route on-screen keyboard notes to a selectable MIDI out. (done — listed for reference)
- **Piano roll copy/paste + undo/redo** — selection, clipboard, snapshot-based undo. (done — listed for reference)
- **Per-track peak meter** — log-scale bar in each `TrackRow`, fed from `AudioTrack::renderInto`. (done — listed for reference)
- **Per-track gain smoothing** — `SmoothedValue<float>` per track, sample-accurate ramp on slider drag. (done — listed for reference)
- **Spacebar = play/stop sequencer** — `MainComponent::keyPressed` toggles play. (done — listed for reference)
- **Drag-and-drop audio files onto a track row** — `juce::FileDragAndDropTarget` on `TrackRow`. (done — listed for reference)
- **Tiny test harness for `MidiClip`** — `juce::UnitTest`-based, 12 subtests, 61 expectations. Builds as a separate `SimpleDawTests` console app linked only against `juce::juce_core` (no audio / GUI / MIDI modules), runs in <1 s on any machine. Run with `.\run-tests.bat`. Covers: default demo melody, addNote/addNotes (empty no-op), removeNote (OOB no-op), removeNotes (re-indexing), undo/redo round-trip, redo-stack clearing on new mutation, clearUndoHistory, replaceAllNotes, undo cap of 200, atomic lengthBeats, startBeat/id, beginEdit, and the audio-thread `tryLock` snapshot pattern. Required `MidiClip.h` to include `<juce_core/juce_core.h>` instead of `<JuceHeader.h>`.
- **Release build** benchmark. (done — Debug 27.9 MB, Release 7.2 MB)
- **App icon** via `juce_add_app_icon` or manual `.ico`.

### Known issues (all fixed in the latest session)

- ~~**Three `DocumentWindow` subclasses still did `delete this` synchronously in `closeButtonPressed`.** `SettingsWindow`, `PluginEditorWindow`, and `PianoRollWindow` were all using the same antipattern that `PluginChooserDialog` had been fixed for. Closing the Audio Settings / VST3 Editor / Piano Roll window could corrupt the heap on Win32 Debug CRT.~~ **Fixed**: all three now defer via `MessageManager::callAsync` (same pattern as `PluginChooserDialog`). `MainComponent` also gained a `std::shared_ptr<bool> alive` flag that its `onClosed` callbacks check before writing back, so a deferred delete after `~MainComponent` is safe.
- ~~**Three `DocumentWindow` double-frees (bug-fix pass #3).** `PluginEditorWindow`, `PianoRollWindow`, `SettingsWindow` all posted `delete this` via `callAsync`, and `~TrackRow` / `~MainComponent` synchronously `delete`d the same pointer. User could double-free by clicking X then quitting in the same message-loop tick.~~ **Fixed**: `std::atomic<bool> closing{false}` + `compare_exchange_strong` in each `closeButtonPressed`; destructors call `closeButtonPressed()` instead of `delete`.
- ~~**`MidiOutputRouter::output` unique_ptr race.** Audio thread read `output` in `sendBuffer`; message thread wrote in `closeOutput` (called from `~MainComponent` before `shutdownAudio`).~~ **Fixed**: added `juce::CriticalSection lock`; all reads/writes of `output` and `currentName` guarded.
- ~~**`AudioTrackSource::currentSampleRate` non-atomic double race.**~~ **Fixed**: made `std::atomic<double>`, updated `prepareToPlay`/`applyLoadedBuffer` to `.store()`, `getSampleRate()` to `.load()`.
- ~~**`AudioTrackSource` size-member reads without `bufferLock`.**~~ **Fixed**: `isLoaded()`/`getNumChannels()`/`getNumSamples()`/`clampLoopBound()` now hold `bufferLock` (SpinLock `ScopedLockType`).
- ~~**`SessionIO::loadSession` silently lost saved A/B region.**~~ **Fixed**: `createTrackFromFileAsync` now takes `loopStart`/`loopEnd`; re-applies them inside the `loadFileAsync` callback after the buffer exists.
- ~~**`Recorder::stopRecording` blocked audio thread on disk I/O.**~~ **Fixed**: snapshot buffer under lock, release, then write WAV.
- ~~**`Recorder::audioDeviceIOCallbackWithContext` allocated on audio thread.**~~ **Fixed in intent, but AGENTS.md was inaccurate**: the actual `Recorder.cpp` does **not** pre-allocate 300 s in `audioDeviceAboutToStart` — it only stores `recordSampleRate`/`deviceBufferSize` there. Allocation is lazy on the first recording block (and never happens while `recording=false`, which is the idle default). Functionally fine for the idle no-recording case; the doc claim has been corrected below.
- ~~**`setStateInformation` not try/catch'd in session load.**~~ **Fixed**: wrapped in `try { ... } catch (...) { juce::Logger::writeToLog(...); }`.
- ~~**Redundant `MessageManager::callAsync` in `TrackRow::openFile` / `filesDropped`.**~~ **Fixed**: removed outer `callAsync` (loadFileAsync already posts internally).
- ~~**`PluginScanThread` blocked shutdown for up to 5 s.**~~ **Fixed**: `stopThread(1000)`.
- ~~**`AudioTrack::lifetimeToken` "dead code" (false alarm).**~~ Verified it IS used by `SessionIO::loadSession` (line 242) and `TracksViewport::createTrackFromFileAsync` (line 83) to guard async callbacks. The source has its own separate `lifetimeToken`. Both tokens are bumped in their respective destructors.

### Bug-fix pass #4 fixes (audio diagnostic + startup focus)

- ~~**Startup `grabKeyboardFocus` assertion.** `MainComponent` called `grabKeyboardFocus()` in its constructor before the component was on screen, tripping `jassert (isShowing()||isOnDesktop())` at `juce_Component.cpp:3022`.~~ **Fixed**: `visibilityChanged()` now grabs focus guarded by `isShowing()`; constructor only calls `setWantsKeyboardFocus(true)`.
- ~~**`Logger::writeToLog` on the audio thread.** The `dd3c92a` diagnostic logged "underrun suspected" and "first non-zero block" from inside `getNextAudioBlock`; `Logger` takes a `CriticalSection` and may do blocking I/O on Windows — a real-time violation that could self-sustain actual underruns.~~ **Fixed**: all audio-thread logging removed; the callback only increments atomic counters (`audioBlocksProduced`/`audioBlocksSilent`/`realUnderruns`/`lastCallbackMs`). The 500 ms GUI `Timer` emits the summary line.
- ~~**Content-based underrun detection was a false-positive generator.** It flagged any silent block following a non-zero block as an "underrun", so normal key-tap release-silence (one blip at `peak=velocity*0.15`, then silence) was misreported as dropped buffers.~~ **Fixed**: underrun detection is now timing-based — `getMillisecondCounterHiRes()` gap between callbacks > 1.5× the expected `bufferSize/sampleRate*1000` period. `bufferSize`/`sampleRate` are captured in `prepareToPlay` as `audioBlockSize`/`audioSampleRate` atomics.
- ~~**Idle silence was reported as suspicious.**~~ **Fixed**: silence is only counted (in `audioBlocksSilent`) when audio **is expected** — `midiTrack.isPlaying()`, or a track is playing (`TracksViewport::anyTrackAudible()`), or an on-screen keyboard key is held (`SynthAudioSource::keyboardState.isNoteOnForChannels(0xffff, n)` over range 36-96). Idle silence never logs.

### New issue: audio regression after bug-fix pass #3 — IN PROGRESS

User reports: after rebuilding, no sound from audio tracks / sequencer / synth, but the **master peak meter IS moving**. Status bar shows device, restart does not help. Audio rendering path in `MainComponent::getNextAudioBlock` is unchanged. Top suspects: (a) `MidiOutputRouter` `CriticalSection` lock added on the audio thread path — kernel-mode `EnterCriticalSection` on contention can stall the audio thread past the buffer size → device underrun; (b) `Recorder::audioDeviceAboutToStart` 300 s pre-allocation (~110 MB) on a memory-constrained system can delay/partial-fail device start. **Next:** revert these two changes in isolation to confirm.
- ~~**Audio regression — root cause was a code bug, not device routing.** The `MidiOutputRouter` `CriticalSection` was the primary cause — on any GUI-thread call to `openOutput`/`closeOutput`/`isOpen`/`getCurrentName` while the audio thread held it, `EnterCriticalSection` could stall the audio thread past the 480-sample @ 48 kHz deadline, producing driver underruns that the device filled with silence. The master peak meter still moved because it measures the buffer BEFORE the underrun manifests at the device output. **Fixed**: replaced unique_ptr + lock with `std::atomic<juce::MidiOutput*>` + a brief `swapLock` for the message thread. Audio thread now does one atomic acquire-load — no kernel transition. Also added a `std::shared_ptr<const std::vector<AudioTrack*>>` snapshot to `TracksViewport` so the audio thread no longer calls `ScopedTryLockType` on the track render path (it was silently dropping track rendering on contention). Audio thread is now fully lock-free on the track-render path. Default `AudioTrack::gain` corrected from `1.0f` to `0.25f` (matches doc). `AudioTrack::currentSampleRate`/`currentBlockSize` made atomic (`std::atomic<double>`/`std::atomic<int>`) since `setPlugin` reads them from the message thread while `prepareToPlay` writes them. Build green; 12/12 MidiClip tests pass.~~ **Amended (pass #4):** removing the lock fixed a real blocker, but it did **not** silence the regression *report* — the flood of "underrun suspected" log lines was (and still is) produced by the diagnostic added in commit `dd3c92a` (`src/Main.cpp:264-299`), which both misreports idle/tap-release silence as underruns AND calls `juce::Logger::writeToLog` on the audio thread. See pass #4 above and `docs/audio-underrun-investigation.md`.
- ~~**`loadSession` switched MIDI out without `sendAllNotesOff`.** The combo's `onChange` does this, but `loadSession` was calling `openOutput` directly.~~ **Fixed**: `loadSession` now calls `sendAllNotesOff()` first, then `refreshMidiOutputCombo()`, then opens the saved output (or `closeOutput()` if `(none)`).
- ~~**Per-track gain had no smoothing.** Cranking a track's gain slider during playback produced zipper noise.~~ **Fixed**: `AudioTrack` now owns a `juce::SmoothedValue<float> gainSmoothed` (50 ms ramp, reset in `prepareToPlay`). `setGain` updates the target; `renderInto` uses `applyGainRamp` for sample-accurate transitions.
- ~~**`AudioTrack` did not call `releaseResources()` in its destructor.** Tracks destroyed via `loadSession` or `removeTrack` (not just `MainComponent::releaseResources`) could leak plugin/buffer resources.~~ **Fixed**: `~AudioTrack` now calls `releaseResources()` first, then bumps the lifetime token.
- ~~**Duplicated ~50 lines of track-creation code in `loadSession` and `toggleRecording`.**~~ **Fixed**: extracted `TrackRow* createTrackFromFileAsync(juce::File, float initialGain)` helper in `MainComponent`; both call sites now use it.
- ~~**Piano roll key-column stuck note.** `PianoRollComponent::auditionPitch` was only released in `mouseUp`. If the cursor left the window while a key was held, the note stuck.~~ **Fixed**: added `mouseExit` release handler.
- ~~**Plugin scan freezes UI.** `PluginHost::scanForPlugins` ran the `scanNextFile` loop on the message thread (inside `Timer::callAfterDelay`). With many VST3s the whole window froze for seconds.~~ **Fixed**: scan now runs on `PluginScanThread : juce::Thread`; results posted to the `KnownPluginList` via `callAsync`; status label polls a count.
- ~~**Audio file load freezes UI.** `AudioTrackSource::loadFile` did a full `AudioFormatReader::read` on the GUI thread. Large WAVs locked the window.~~ **Fixed**: `loadFileAsync` reads on a `std::thread` and posts the buffer swap back via `callAsync`; `TrackRow` shows a `(loading...)` label.
- ~~**`loadSession` set BPM to 0 on missing property.** Older `.sdaw` files without `"bpm"` would set the engine BPM to 0.0 and leave the slider out of range.~~ **Fixed**: `isVoid()` check, falls back to 120.
- ~~**`Recorder::stopRecording` could write WAV with `sampleRate=0.0`** if `audioDeviceAboutToStart` had not fired for the second callback registration.~~ **Fixed**: defensive default of 48000.0 if `recordSampleRate` is 0.

### Newly implemented features (this session)

- **Track reorder via drag** — `MainComponent` now derives from `juce::DragAndDropContainer` (a mixin, not a Component subclass). `TrackRow` implements `juce::DragAndDropTarget`. A `TrackRowDragStarter : MouseListener` is registered on the row + the non-interactive labels (`nameLabel`, `timeLabel`, `pluginLabel`) and calls `startDragging("TrackRowReorder", row)` on `mouseDown`. Buttons and sliders are intentionally NOT registered, so they keep their normal click/drag behaviour. While dragging, the hovered `TrackRow` paints a 2-px yellow line at the top (insert above) or bottom (insert below) half, computed from `localPosition.y` against `getHeight()/2`. `itemDropped` calls `MainComponent::reorderTrack(from, to, insertAbove)`, which moves the entries in `tracks` and `trackRows` atomically under `tracksLock` and calls `refreshLayout()`.

---

## Architectural decisions made

- **C++ over Rust** (user chose): VST3 SDK + JUCE + ASIO are first-class C++; Rust ecosystem is too thin for a serious DAW.
- **JUCE framework**: handles ASIO/WASAPI I/O, VST3 hosting, MIDI, file I/O, GUI primitives — all in one.
- **CMake over Projucer**: Projucer is being phased out.
- **Audio I/O backend**: ASIO primary, WASAPI fallback. Currently on WASAPI; switch via audio settings panel.
- **Plugin format**: VST3 only for v1. CLAP deferred.
- **MIDI**: JUCE's `MidiBuffer` + `Synthesiser` for v1.
- **Piano roll scope**: data model (`MidiNote` + `MidiClip`) is custom, UI is custom `juce::Component`s layered over a grid.
- **Synth voice style**: custom `juce::SynthesiserVoice` subclass.
- **MIDI device handling**: open all available `MidiInput` devices on startup; route everything through the same `MidiKeyboardState`.
- **Mixing architecture**: `std::vector<std::unique_ptr<AudioTrack>>`; each track owns its `AudioTrackSource` and a scratch buffer; `renderInto()` applies mute/solo/gain/pan and sums into the master bus. Synth renders into a separate scratch buffer in `MainComponent` and is added before the audio tracks. Smoothed master gain applied last.
- **Loop model**: A/B region loop. `AudioTrackSource` has `std::atomic<int> loopStart`, `loopEnd` (default `loopEnd = numSamples`, i.e. full-file fallback). When `looping` is on and `playPosition >= loopEnd`, wraps to `loopStart` (not 0). `TrackRow` exposes **Set A** / **Set B** (capture playhead), two sliders, **Clear**, and a status label on a collapsible second row (76 px collapsed, 126 px expanded). Toggling the row calls `MainComponent::refreshLayout()` via an `onLayoutChanged` callback (see the Viewport quirk above — do not walk the parent chain).
- **Gain defaults**: tracks 0.25, synth 0.6, master 0.8 (user preference; protects small speakers).
- **GUI thread ↔ audio thread safety**: every parameter shared between threads is `std::atomic`. `AudioTrackSource::playing`/`looping`/`playPosition`/`loopStart`/`loopEnd`, `AudioTrack::gain`/`pan`/`mute`/`solo`/`pluginBypass`, `MainComponent::synthGain`/`masterGainTarget` are all atomic.
- **Parameter smoothing**: `juce::SmoothedValue<float>` for the master gain (50 ms ramp, prevents clicks on big volume changes) AND per-track gain (50 ms ramp, eliminates zipper noise on slider drags). Both use the same ramp time for consistency.
- **Slider text box policy**: every `juce::Slider` that should only be dragged uses `setTextBoxIsEditable(false)`.
- **Toggle buttons**: prefer `TextButton` + `setClickingTogglesState(true)` over `ToggleButton`.
- **File dialog pattern:** `browseForFileToOpen()` (modal) with `JUCE_MODAL_LOOPS_PERMITTED=1`.
- **Refactor timing:** `src/Main.cpp` split into `src/audio/`, `src/midi/`, `src/tracks/` via `GLOB_RECURSE`.
- **Refactor — split `MainComponent` into focused classes.** When `MainComponent` hit 42 KB / ~1400 lines it became a god class. Extracted:
  - `src/ui/TransportBar.{h,cpp}` — top row + status label, callback-driven.
  - `src/tracks/TracksViewport.{h,cpp}` — owns viewport + container + tracks/trackRows + `tracksLock` + add/remove/reorder/clear/layout/render.
  - `src/session/SessionIO.{h,cpp}` — plain helper for `.sdaw` save/load + recording; reads/writes slider values via lambda getters/setters.
  `MainComponent` shrinks to ~21 KB and just composes the sub-components + handles the `AudioAppComponent` audio loop, `MidiInputCallback`, `Timer`, `DragAndDropContainer`, and spacebar transport. All three new modules live in the existing `GLOB_RECURSE src/*.cpp` glob, so no `CMakeLists.txt` change was needed.
- **Window size:** 960×600 is the user-confirmed default. Do not increase.

---

## Project structure (target, current state marked)

```
simple-daw/
├── CMakeLists.txt              ✓ done
├── build-dev.bat               ✓ done
├── .gitignore                  ✓ done
├── README.md                   ✓ done
├── src/
│   ├── Main.cpp                ✓ Composes synth + multi-track audio + master gain + VST3 host + A/B loop viewport
│   ├── audio/
│   │   ├── AudioTrackSource.h  ✓ (playing/looping/playPosition/loopStart/loopEnd atomic, A/B wrap to loopStart)
│   │   ├── AudioTrackSource.cpp ✓
│   │   ├── Recorder.h          ✓ AudioIODeviceCallback + WAV writer (input capture)
│   │   └── Recorder.cpp        ✓
│   ├── midi/
│   │   ├── SineVoice.h         ✓
│   │   ├── SineVoice.cpp       ✓
│   │   ├── DemoSound.h         ✓
│   │   ├── DemoSound.cpp       ✓
│   │   ├── SynthAudioSource.h  ✓
│   │   ├── SynthAudioSource.cpp ✓
│   │   ├── MidiNote.h          ✓ Phase 1 data (pitch, startBeat, lengthBeats, velocity)
│   │   ├── MidiClip.h          ✓ Phase 1 data (vector<MidiNote> + loadDemoMelody + CriticalSection lock)
│   │   ├── MidiClip.cpp        ✓
│   │   ├── MidiTrack.h         ✓ Phase 1 engine (AudioSource + beat clock + synth + note scheduling)
│   │   ├── MidiTrack.cpp       ✓
│   │   └── MidiOutputRouter.h  ✓ Owns juce::MidiOutput, forwards MidiBuffer via sendMessageNow
│   ├── tracks/
│   │   ├── AudioTrack.h        ✓ Mixer track (gain/pan/mute/solo + plugin insert + A/B pass-throughs + renderInto)
│   │   ├── AudioTrack.cpp      ✓
│   │   ├── TrackRow.h          ✓ Per-track row: name, Load, Play, Stop, time, Gain, Pan, Mute, Solo, Loop, A/B, VST3, Bypass, Edit, X + plugin label + collapsible A/B panel
│   │   ├── TrackRow.cpp        ✓
│   │   ├── TracksViewport.h    ✓ Owns viewport + container + tracks/trackRows + tracksLock + add/remove/reorder/clear/layout/render
│   │   └── TracksViewport.cpp  ✓
│   ├── plugin/
│   │   ├── PluginHost.h        ✓ AudioPluginFormatManager + KnownPluginList wrapper
│   │   ├── PluginHost.cpp      ✓
│   │   └── PluginWindows.h     ✓ PluginChooserDialog + PluginEditorWindow (inline)
│   ├── ui/
│   │   ├── PianoRollComponent.h   ✓ Phase 2 UI (grid + keys + draw/move/resize/delete + snap + playhead + velocity lane + zoom + scroll + selection + clipboard + undo/redo)
│   │   ├── PianoRollComponent.cpp ✓
│   │   ├── PeakMeterComponent.h   ✓ Log-scale master peak meter (Timer + atomic read+reset + decay)
│   │   ├── TrackMeterComponent.h  ✓ Log-scale per-track peak meter
│   │   ├── SettingsWindow.h     ✓ AudioDeviceSelectorComponent in DocumentWindow (deferred close)
│   │   ├── TransportBar.h       ✓ Top row + status label (callback-driven)
│   │   └── TransportBar.cpp     ✓
│   ├── session/
│   │   ├── SessionIO.h          ✓ Save/load/recording helper
│   │   └── SessionIO.cpp        ✓
├── tests/
│   └── MidiClipTests.cpp    ✓ juce::UnitTest harness for MidiClip (data + undo/redo)
├── third_party/
│   └── JUCE/                   ✓ cloned
├── docs/
│   ├── learning-plan-juce-dsp.md          ✓ done
│   ├── learning-plan-week-0-2-3.md       ✓ done
│   └── daw-architecture.md                ✓ done
└── build/                      gitignored
```

---

## MVP order (from earlier planning)

1. ✓ Week 0 toolchain
2. ✓ Week 3 — Audio I/O
3. ✓ Week 4 — MIDI input
4. ✓ Audio track (WAV load + playback)
5. ✓ Refactor (split into `src/audio/` + `src/midi/`)
6. ✓ Multi-track mixer
7. ✓ Synth gain slider
8. ✓ **Mixer polish** (Pan, Solo, Time, Master, Loop)
9. ✓ **VST3 insert** (Load / Bypass / Edit per track)
10. ✓ **A/B region loop** (Set A / Set B / Clear / sliders, collapsible panel, Viewport)
11. ✓ **Piano roll Phase 1** (MidiNote + MidiClip + MidiTrack engine + demo melody)
12. ✓ **Piano roll Phase 2 — UI** (PianoRollComponent + PianoRollWindow)
13. MIDI playback (sequencer → synth) — done (engine + UI)
14. ✓ **Master peak meter** (log-scale bar, atomic read+reset, decay)
15. ✓ **ASIO audio settings panel** (AudioDeviceSelectorComponent in DocumentWindow)
16. ✓ **JSON save/load** (save/load buttons, `.sdaw` JSON format, restore gains/BPM/clip/tracks)
17. ✓ **Piano roll improvements** (velocity lane + zoom + scroll + Clear All)
18. ✓ **Recording** (ASIO input → audio track)
19. ✓ **MIDI output routing** (MidiOutputRouter + combo box, persisted in `.sdaw`)
20. ✓ **Per-track gain smoothing** (SmoothedValue per track, eliminates zipper noise)
21. ✓ **Spacebar transport** (toggles sequencer play/stop from MainComponent)
22. ✓ **Drag-and-drop audio files onto a track row** (FileDragAndDropTarget on TrackRow)
23. ✓ **Track reorder via drag** (DragAndDropContainer on MainComponent, DragAndDropTarget on TrackRow)

**All core + selected stretch features complete. Remaining items:** App icon.

---

## Conventions (carry over from Markdown Viewer)

- No comments in code unless asked
- No emojis
- TypeScript/C++ strict where applicable
- All state mutations go through well-defined classes (no globals in audio thread)
- `std::atomic` for any parameter shared between GUI and audio threads
- `juce::SmoothedValue<float>` for any parameter that changes during playback
- `juce::Slider` text boxes: `setTextBoxIsEditable(false)` for value-only display
- Commit working state at the end of each week
- Window size is **960×600** — do not change without confirming with the user
