# Simple DAW - Architecture

This document describes how Simple DAW is structured today and where it is
headed. It is the canonical reference for "which class owns what" and
"which thread touches which data".

## 1. High-level overview

Simple DAW is a single-process JUCE 8 GUI app. There are two threads of
interest:

| Thread | Role |
|------|------|
| **Message (GUI) thread** | Owns all `juce::Component`s, sliders, buttons, the `Timer`, file dialogs, plugin editors. |
| **Audio thread** | Runs inside `AudioAppComponent::getNextAudioBlock` at the device callback rate (every `bufferSize / sampleRate` seconds). Must not allocate, lock, or touch non-atomic GUI state. |

The two threads communicate exclusively through `std::atomic<T>` members
and `juce::SmoothedValue<float>` (which is lock-free and audio-safe).

```
+----------------------------- GUI thread -----------------------------+
|                                                                     |
|  MainComponent (AudioAppComponent + MidiInputCallback + Timer +     |
|                 DragAndDropContainer)                              |
|  +- TransportBar (top row + status)                                 |
|  |    +- statusLabel, addTrack, scanVST3, settings, save, load,     |
|  |       rec, midiOut combo, masterGain, peak meter                 |
|  +- sequencer row  (Seq Play/Stop/Loop/Piano Roll, BPM, beat, Seq) |
|  +- TracksViewport (owns tracksContainer + viewport)                |
|  |    +- TrackRow 0   [Load/Play/Stop/Time/Gain/Pan/Mute/Solo/Loop/X]|
|  |    +- TrackRow 1                                                  |
|  |    +- ...                                                         |
|  +- MidiKeyboardComponent (5 octaves, C2..C7)                       |
|  +- synth row  (Synth gain)                                        |
|  +- SessionIO  (save/load/recording helper)                         |
|  +- openedInputs : OwnedArray<MidiInput>                            |
|                                                                     |
+-------------------------+------------------------+-------------------+
                          |                        |
              atomic<float> synthGain    atomic<float> masterGainTarget
                          |                        |
                          v                        v
+----------------------- Audio thread ------------------------------+
|                                                                  |
|  getNextAudioBlock(bufferToFill)                                 |
|    1. scratchBuffer <- SynthAudioSource.getNextAudioBlock        |
|       * Synthesiser renders 8 SineVoices driven by               |
|         MidiKeyboardState.processNextMidiBuffer                  |
|       * apply synthGain                                          |
|    2. dest <- scratchBuffer                                      |
|    3. for each AudioTrack t:                                     |
|         t.renderInto(dest, ...)                                  |
|           * scratch <- AudioTrackSource.getNextAudioBlock        |
|             (wraps at end if looping, stops if not)              |
|           * apply gain * panGain per channel                     |
|           * addFrom into dest                                    |
|    4. masterGainSmoothed.setTargetValue(masterGainTarget)        |
|       apply masterGainSmoothed.getNextValue() to dest            |
|                                                                  |
+------------------------------------------------------------------+
```

## 2. Module map

### `src/Main.cpp` - `MainComponent`

The root component. Inherits:

- `juce::AudioAppComponent` - audio I/O lifecycle.
- `juce::MidiInputCallback` - receives MIDI from hardware inputs.
- `juce::Timer` - 500 ms tick that refreshes the status label and every
  `TrackRow::refreshTimeLabel()`.
- `juce::DragAndDropContainer` - mixin that exposes
  `findParentDragContainerFor` to `TrackRow` for track reorder.

Owns the audio engine + state:

- `SynthAudioSource synthSource` - the built-in sine synth.
- `MidiTrack midiTrack` - the sequencer track (synth + clip).
- `Recorder recorder` - input capture.
- `MidiOutputRouter midiOutputRouter` - selectable MIDI out.
- `PluginHost pluginHost` - VST3 scan/host.
- `juce::OwnedArray<juce::MidiInput> openedInputs` - all USB MIDI inputs
  opened on startup.
- `juce::AudioBuffer<float> scratchBuffer` + `midiTrackScratch` - scratch
  for the synth + sequencer renders.
- `std::atomic<float> synthGain{0.6f}`, `midiGain{0.5f}`,
  `masterGainTarget{0.8f}`, `masterPeak{0.0f}`,
  `juce::SmoothedValue<float> masterGainSmoothed{0.8f}`.

Owns the sub-components that make up the UI (delegated work):

- `TransportBar transportBar` - top row + status label.
- `TracksViewport tracksViewport` - the tracks viewport + container.
- `SessionIO sessionIO` - save/load/recording helper.

Plus the remaining inline UI: sequencer row (Seq Play/Stop/Loop/Piano
Roll/BPM/beat/Seq gain), MIDI keyboard, synth row, and the open
subwindow pointers (`pianoRollWindow`, `settingsWindow`).

Key methods:

- `prepareToPlay` - forwards to `synthSource`, `midiTrack`, and
  `tracksViewport.prepareAllToPlay`; resets the smoothed master gain.
- `getNextAudioBlock` - the entire mix. See the diagram above. Track
  rendering is delegated to `tracksViewport.tryRenderAll`.
- `refreshLayout` - vertical box layout of the sub-components and inline
  rows. Track-row layout lives in `TracksViewport::refreshLayout`.
- `handleIncomingMidiMessage` - logs the message and forwards note
  on/off to `synthSource.keyboardState`.
- `timerCallback` - status update + per-row time refresh.
- `keyPressed` - spacebar toggles sequencer play/stop.

`MainWindow` is a `juce::DocumentWindow` with the native title bar. The
title text is plain ASCII (`"Simple DAW"`) to avoid the em-dash encoding
garble (`aC`) seen in earlier builds when the source contained a UTF-8
em-dash and the Win32 title bar interpreted it as ANSI.

`SimpleDawApplication` is the `juce::JUCEApplication` glue.

### `src/ui/TransportBar` - top row + status

A `juce::Component` that owns the top strip of the main window: the
status label, **+ Add Track** / **Scan VST3** / **Settings** buttons,
**Save** / **Load** / **Rec** buttons, the **MIDI Out** combo, the
**Master** gain slider, and the peak meter. Communicates with the rest
of the app through a set of `std::function` callbacks given at
construction time:

- `onAddTrack`, `onScanPlugins`, `onSettings`
- `onSave`, `onLoad`, `onRecordToggle`
- `onMidiOutputChanged(name)`, `onMasterGainChanged(value)`

Public mutators: `setStatusText`, `setMasterGain`, `setRecToggleState`,
`setScanButtonEnabled`, `refreshMidiOutputCombo`,
`setMidiOutputSelection`. Public getters: `getMasterGain`,
`getMidiOutputSelection`. Exposes the `noneOutputEntry` constant for
`SessionIO` to compare against.

### `src/tracks/TracksViewport` - track list + viewport

A `juce::Component` that owns the `juce::Viewport` and the inner
`juce::Component` container, plus the `tracks` / `trackRows` vectors
and the `tracksLock` spin-lock. Owns `addTrack` /
`createTrackFromFileAsync` / `removeTrack` / `reorderTrack` /
`clearAll` / `refreshLayout` / `prepareAllToPlay` /
`releaseAllResources` / `tryRenderAll` / `anyTrackSoloed`. Exposes
`getTracks()` and `getTrackRows()` for read-only access by
`MainComponent` (status display) and `SessionIO` (save/load).

### `src/session/SessionIO` - save / load / recording

A plain helper class (not a `Component`). Owns the JSON `.sdaw`
serialisation and the recording start/stop. Reads slider values and
writes them back via `std::function` getters/setters given at
construction time (so it does not need to know about the slider
widgets directly). Methods: `saveSession`, `loadSession`,
`toggleRecording`.

### `src/audio/AudioTrackSource` - file-backed `AudioSource`

Holds a fully-decoded `juce::AudioBuffer<float>` in memory and streams it
out one block at a time.

| Member | Type | Thread | Notes |
|------|------|------|------|
| `fileBuffer` | `AudioBuffer<float>` | GUI (load) + audio (read) | Reads are read-only after `loadFile` returns. |
| `playPosition` | `atomic<int>` | both | Updated by audio, read by GUI for the time label. |
| `playing` | `atomic<bool>` | both | Toggled by `TrackRow` buttons. |
| `looping` | `atomic<bool>` | both | When `true`, wraps `playPosition` to 0 at end of buffer; when `false`, stops and zeroes `playPosition`. |
| `currentSampleRate` | `double` | both | Set by `prepareToPlay` and by `loadFile`. |
| `loadedFileName` | `String` | GUI only | Display string with channel / sample-count / rate info. |

`loadFile` constructs a local `AudioFormatManager`, registers basic
formats (WAV/AIFF/FLAC/MP3), creates a reader, allocates `fileBuffer`,
and reads the whole file in. This is the only place that allocates on
behalf of a track; it runs on the GUI thread inside `TrackRow::openFile`.

`getNextAudioBlock` copies `min(numSamples, remaining)` samples from
`fileBuffer` into the output buffer, channel-by-channel with modulo for
mono files. If the playhead reaches the end:

- looping: wrap to 0, continue.
- not looping: stop, zero the playhead, return.

### `src/midi/SineVoice` + `DemoSound` + `SynthAudioSource`

- `SineVoice : juce::SynthesiserVoice` - sine oscillator, velocity -> amplitude
  (velocity * 0.15). No envelope, no tail-off; `stopNote` zeroes `level`
  and clears the current note. 8 voices are added to the `Synthesiser`.
- `DemoSound : juce::SynthesiserSound` - `appliesToNote` and
  `appliesToChannel` both return `true`. A single instance is added to
  the `Synthesiser`.
- `SynthAudioSource : juce::AudioSource` - owns the `Synthesiser` and a
  public `MidiKeyboardState`. `getNextAudioBlock` clears the buffer,
  pulls MIDI from the keyboard state into a local `MidiBuffer`, and
  calls `synth.renderNextBlock`. The MIDI buffer is created per block -
  this is fine because `juce::MidiBuffer` is cheap and the synth drains
  it immediately.

### `src/tracks/AudioTrack` - mixer strip

Plain class (not a `Component`). Owns:

- `std::unique_ptr<AudioTrackSource> source` - the file player.
- `juce::AudioBuffer<float> scratchBuffer` - per-track scratch used by
  `renderInto`, sized in `prepareToPlay`.
- `std::atomic<float> gain{1.0f}`, `pan{0.0f}`,
  `std::atomic<bool> mute{false}`, `solo{false}`.

`renderInto(dest, startSample, numSamples, anyOtherTrackSoloed)` is called
once per block per track from `MainComponent::getNextAudioBlock`:

1. If muted, return.
2. If any other track is soloed and this one is not, return.
3. If no file is loaded, return.
4. Render the source into `scratchBuffer`.
5. For each destination channel, compute the linear pan gain
   (`L = p<=0 ? 1 : 1-p`, `R = p>=0 ? 1 : 1+p`) and `addFrom` the
   scratch into `dest` with `gain * panGain`.

Pan law: **linear constant-power-ish**. Not a true constant-power law
(-3 dB center); it is a linear crossfade. Replace with
`L = cos((p+1)*pi/4)`, `R = sin((p+1)*pi/4)` if constant-power is wanted.

### `src/tracks/TrackRow` - per-track UI

`juce::Component`, 76 px tall. Layout (left to right on the button row):

```
[Load] [Play/Pause] [Stop] [time] [Gain====] [Pan==] [Mute] [Solo] [Loop] ... [X]
```

The name label spans the first 18 px row. All toggles are
`TextButton + setClickingTogglesState(true)`. Colour overrides:

- Mute on: `0xffaa3030` (red)
- Solo on: `0xff3060a0` (blue)
- Loop on: `0xff308030` (green)
- Remove (always): `0xff803030` (dim red)

`openFile` uses a modal `juce::FileChooser` (permitted by
`JUCE_MODAL_LOOPS_PERMITTED=1`). The earlier `launchAsync` variant opened
invisibly behind the main window on Windows; the modal form is reliable.

`refreshTimeLabel` is called by `MainComponent::timerCallback` every
500 ms. It reads `getPlayPosition()` and `getNumSamples()` from the
source and formats as `m:ss / m:ss`.

## 3. Thread-safety model

| Data | GUI thread | Audio thread |
|------|------|------|
| `AudioTrackSource::playing`, `looping`, `playPosition` | write (buttons) / read (time label) | read + write |
| `AudioTrack::gain`, `pan`, `mute`, `solo` | write (sliders/buttons) | read |
| `MainComponent::synthGain`, `masterGainTarget` | write (sliders) | read |
| `MainComponent::masterGainSmoothed` | - | read + write (audio-only) |
| `AudioTrackSource::fileBuffer` | write only inside `loadFile` (GUI) | read only after `loadFile` returns |
| `SynthAudioSource::keyboardState` | write (mouse, MIDI in) | drained by `processNextMidiBuffer` |

Everything that crosses the line is `std::atomic` (or, for the master
gain, `juce::SmoothedValue<float>` which is lock-free). The
`MidiKeyboardState` is internally synchronised by JUCE.

## 4. Build & dependency model

`CMakeLists.txt`:

- `juce_add_gui_app(SimpleDaw PRODUCT_NAME "Simple DAW" COMPANY_NAME "Viibe" VERSION 0.1.0)`
- `juce_generate_juce_header(SimpleDaw)` - generates `JuceHeader.h` that
  aggregates the modules listed in `target_link_libraries`. **Pitfall:**
  modules must be listed explicitly, even ones pulled in transitively
  (e.g. `juce_gui_extra`), or they are missing from the aggregator. If
  you add a module and `JuceHeader.h` does not see it, delete
  `build/SimpleDaw_artefacts/JuceLibraryCode/` to force a regen.
- `file(GLOB_RECURSE ... CONFIGURE_DEPENDS "src/*.cpp")` - new files
  under `src/` are picked up automatically. New folders (e.g. the
  planned `src/plugin/`, `src/ui/`) require no CMake edit.
- Defines: `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`,
  `JUCE_VST3_CAN_REPLACE_VST2=0`, `JUCE_MODAL_LOOPS_PERMITTED=1`.
- Links: `juce_audio_utils`, `juce_audio_devices`, `juce_audio_formats`,
  `juce_audio_processors` (already linked for the upcoming VST3 host),
  `juce_dsp`, `juce_gui_extra` + the recommended config / LTO / warning
  flag modules.

`build-dev.bat` initialises `VsDevCmd.bat` then runs CMake configure +
build. Output: `build/SimpleDaw_artefacts/<Config>/Simple DAW.exe` (note
the space in the file name - JUCE uses `PRODUCT_NAME`).

## 5. Audio signal flow (one block)

```
MainComponent::getNextAudioBlock(bufferToFill)
|
|-- scratchBuffer.clear()
|-- SynthAudioSource.getNextAudioBlock(scratch)
|     |-- MidiKeyboardState -> MidiBuffer
|     |-- Synthesiser.renderNextBlock (8 SineVoices)
|     \-- (return)  -> apply synthGain -> addFrom into bufferToFill
|
|-- for each AudioTrack t:
|     \-- t.renderInto(bufferToFill, ...)
|           |-- if mute / not-soloed-when-others-soloed / unloaded: return
|           |-- scratch.clear()
|           |-- AudioTrackSource.getNextAudioBlock(scratch)
|           |     \-- copy from fileBuffer, wrap or stop at end
|           \-- for each channel: addFrom with gain * panGain
|
|-- masterGainSmoothed.setTargetValue(masterGainTarget)
|-- apply masterGainSmoothed.getNextValue() to bufferToFill
\-- (return to JUCE)
```

## 6. Roadmap

Milestones are tracked in `AGENTS.md`. Current status:

- [x] Week 0 - toolchain + hello world
- [x] Week 3 - audio I/O (sine via `Slider` -> `atomic<float>`)
- [x] Week 4 - MIDI input + on-screen keyboard
- [x] Audio track loading (WAV/AIFF/FLAC/MP3)
- [x] Refactor: split `src/Main.cpp` into `audio/`, `midi/`, `tracks/`
- [x] Multi-track mixer
- [x] Synth gain slider
- [x] Mixer polish: Pan, Solo, Time, Master (SmoothedValue), Loop
- [ ] **VST3 insert per track** (next)
- [ ] A/B region loop (after VST3)
- [ ] Piano roll / sequencer (Phase 1 data + engine, Phase 2 UI)
- [ ] Recording (ASIO input -> audio track)

### VST3 insert - planned shape

1. `AudioTrack` gains `std::unique_ptr<juce::AudioPluginInstance> plugin`.
2. `TrackRow` gains a "Load VST3" button. Scanning uses
   `juce::AudioPluginFormatManager` (with `VST3PluginFormat` registered)
   and `juce::KnownPluginList` over `C:\Program Files\Common Files\VST3\`.
   Present a `juce::PluginListComponent` or a simple list dialog.
   Instantiate via `formatManager.createPluginInstance(...)`.
3. In `AudioTrack::renderInto`, after the source renders into
   `scratchBuffer`, call `plugin->processBlock(scratchBuffer, midi)`
   before the gain/pan/sum.
4. Show the plugin editor
   (`plugin->createEditorIfNeeded()`) as a child window attached to the
   `TrackRow`.
5. Stretch: bypass button, preset save/load via
   `getStateInformation()` / `setStateInformation()`, multiple insert
   slots per track.

`juce_audio_processors` is already linked, so no CMake change is
required. The throwaway-`TrackRow` wart in `refreshLayout` must be
fixed first because `TrackRow` will become expensive to construct once
it owns a plugin editor.

### A/B region loop - planned shape

Deferred until after VST3 + piano roll. Adds:

- `std::atomic<int> loopStart`, `loopEnd` on `AudioTrackSource`.
- "Set A at current playhead" / "Set B at current playhead" buttons in
  `TrackRow`.
- Two slider fields for fine adjustment.
- `TrackRow` layout redesign (the button row is already busy).

### Piano roll - planned shape

- **Phase 1 - data + engine.** `MidiNote`, `MidiClip`, `MidiTrack :
  AudioSource` with a beat clock that emits `noteOn` / `noteOff` into a
  `MidiBuffer` and renders through a `juce::Synthesiser`.
- **Phase 2 - UI.** `PianoRollComponent : juce::Component` with left
  piano keys, grid with bars/beats, drag-to-draw, drag-to-resize,
  right-click delete. Snap-to-grid (1/4, 1/8, 1/16, triplet). Playhead
  that scrolls during playback. Velocity lane (stretch).

### Other stretch tasks

- Switch to ASIO (Komplete Audio 1) via an audio settings panel -
  latency drops from ~10 ms to ~2-3 ms.
- Master peak meter (`getMagnitude(channel, 0, numSamples)` repainted
  from a `Timer`).
- JSON session save/load (restore tracks + plugin state on relaunch).
- `juce::MidiOutput` and route on-screen keyboard notes to a selectable
  MIDI out.
- App icon via `juce_add_app_icon` or a manual `.ico`.
- Release build benchmark.

## 7. Known quirks and gotchas

- **CMake is not on PATH by default** after a winget install. `build-dev.bat`
  adds `C:\Program Files\CMake\bin` to PATH.
- **`JuceHeader.h` aggregation pitfall:** modules must be named in
  `target_link_libraries` explicitly. Delete
  `build/SimpleDaw_artefacts/JuceLibraryCode/` to force a regen.
- **`juce::SynthesiserSound` is abstract.** Subclass it (`DemoSound`).
- **`MidiKeyboardComponent` has no `setNumOctavesForKeyboard`.** Use
  `setAvailableRange(int, int)` and `setKeyWidth(float)`.
- **`MidiKeyboardComponent` colour IDs are limited.**
  `mouseDownKeyFillColourId` does not exist on this class.
- **`MidiInput::openDevice` returns by value.** Use
  `if (input) { input->start(); openedInputs.add(input.release()); }`.
- **`MidiDeviceInfo` has no `isEnabled()`.** Just iterate; failure
  returns an empty list.
- **MIDI callback method is `handleIncomingMidiMessage`** (not
  `midiInputCallback`).
- **`FileChooser::launchAsync` opens invisibly behind the main window on
  Windows.** Use `browseForFileToOpen()` (modal) with
  `JUCE_MODAL_LOOPS_PERMITTED=1`.
- **`JUCE_MODAL_LOOPS_PERMITTED` defaults to 0 in Debug.** Already set
  to 1 in `CMakeLists.txt`.
- **`juce::Slider` with `TextBoxRight` is editable by default.** Always
  call `setTextBoxIsEditable(false)` on drag-only sliders; otherwise a
  click in the text box can swallow focus from a nearby button.
- **`juce::ToggleButton` is a poor fit for short labels.** Use
  `TextButton` + `setClickingTogglesState(true)`.
- **Data race if `playing` is a plain `bool`.** `AudioTrackSource`'s
  `playing` / `looping` / `playPosition` and `AudioTrack`'s
  `gain` / `pan` / `mute` / `solo` are all `std::atomic`.
- **`AudioFormatManager::registerBasicFormats()`** bundles
  WAV/AIFF/FLAC/MP3 decoders.
- **WASAPI buffer size is 480** on the Komplete Audio 1. ASIO drops
  this to ~2-3 ms.
- **OneDrive caveat:** if MSBuild complains about read-only files under
  `src-tauri/target/`, redirect cargo's build output to a temp dir. Not
  currently triggered for the JUCE build because JUCE writes only into
  `build/` (gitignored).
- **Window size is 960x600** - user-confirmed default. Do not increase
  without checking.
