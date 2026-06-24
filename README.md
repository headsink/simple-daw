# Simple DAW

A small, multi-track Digital Audio Workstation built in **C++20** on top of
[JUCE 8](https://github.com/juce-framework/JUCE). It is a personal learning
project targeting Windows 11 with MSVC, but the code itself is portable.

> Status: feature-complete for the MVP scope. Multi-track audio, VST3 inserts,
> piano roll, recording, A/B loop, save/load and MIDI output routing all work
> end-to-end (see [docs/daw-architecture.md](docs/daw-architecture.md)).

## Features

### Audio
- **Multi-track playback.** Load `WAV / AIFF / FLAC / MP3` files into any
  number of tracks and mix them in real time. File load is async (off the
  message thread) with a `(loading...)` placeholder.
- **Per-track controls:** Load, Play, Stop, Gain (smoothed 50 ms ramp),
  Pan, Mute, Solo, **A/B region loop** (Set A / Set B / Clear + sliders),
  per-track log-scale peak meter, Remove.
- **Drag-and-drop** audio files onto a track row. **Drag to reorder** rows.
- **VST3 insert per track** with async plugin loading, Bypass, and an
  external editor window. Scan with **Scan VST3** (runs on a background
  thread; dialog shows "Scanning... (N found)").
- **Master bus** with smoothed gain (0.0–2.0, default 0.8) and a log-scale
  peak meter (green < −24 dB, yellow < −12 dB, red ≥ −12 dB).
- **Recording.** Captures the ASIO input into a growing buffer; on stop,
  writes a timestamped WAV to `Documents\Simple DAW Recordings\` and
  spawns a new audio track that auto-loads it.

### MIDI
- **On-screen 5-octave keyboard** (C2–C7) plus auto-open of every USB
  MIDI input on startup.
- **Built-in 8-voice sine synth** with velocity-sensitive amplitude.
- **Selectable MIDI output** in the top row. The on-screen keyboard and
  the sequencer both route to the chosen device; switching sends
  all-notes-off on all 16 channels first.
- **Piano roll** (in a separate window): draw / move / resize / delete
  notes, drag-rectangle select, snap to 1/4 / 1/8 / 1/16 / 1/16T,
  velocity lane (drag to edit), zoom (− / +), horizontal scroll,
  **Ctrl+Z / Y** undo-redo, **Ctrl+C / V / X** clipboard, **Del** delete,
  **Ctrl+A** select all, arrow-key nudge.
- **Sequencer** (in the main window): Seq Play / Stop / Loop, BPM
  slider (40–240, default 120), beat position label, spacebar toggles
  play/stop. Pre-populated with a C-major demo clip.

### Session
- **Save / Load** the full session to a `.sdaw` JSON file: master /
  synth / midi gains, BPM, MIDI clip (length + notes), every audio
  track (file path, gain, pan, mute, solo, loop, A/B region), MIDI
  output device, and VST3 plugin identifier + state + bypass.
- **Audio settings panel** (Settings button): switch to ASIO
  (Komplete Audio 1), pick buffer size and sample rate, changes apply
  immediately.
- **JSON persistence** via `juce::DynamicObject` + `juce::JSON::writeToStream`.

### UX
- **Window size 960×600**, fits the developer's screen.
- **Live status bar** shows the audio device, buffer size, sample rate,
  open MIDI ports and a per-track state code (`P` playing, `M` muted,
  `.` idle). Refreshed every 500 ms.
- **Thread-safe mixer.** Every parameter shared between the GUI and
  audio thread is `std::atomic`; per-track gain and master gain use
  `juce::SmoothedValue<float>` (50 ms ramp) so slider drags and volume
  changes do not click or zipper.

### Tests
- **`MidiClip` unit tests** in `tests/MidiClipTests.cpp` (12 subtests, 61
  expectations). Build with `.\run-tests.bat`. The test binary is a
  headless console app that links only `juce::juce_core` — no audio,
  MIDI, or display needed. Runs in <1 s, exits non-zero on any
  failure. Covers data mutations (add/remove/clear/replace), undo/redo
  round-trip, the 200-snapshot cap, the `tryLock` snapshot pattern
  used by the audio thread, and the `beginEdit` manual-push API.

## Stack

| Layer | Choice |
|------|------|
| Language | C++20 (MSVC v143, `/std:c++20`, no extensions) |
| Framework | JUCE 8 (depth-1 clone under `third_party/JUCE/`) |
| Build | CMake 3.22+ (`juce_add_gui_app`) |
| Audio I/O | JUCE `AudioAppComponent`, WASAPI default / Komplete Audio 1 ASIO |
| MIDI | `juce::MidiKeyboardState` + `juce::Synthesiser` + `MidiOutput` |
| Formats | `AudioFormatManager::registerBasicFormats()` (WAV / AIFF / FLAC / MP3) |
| Plugins | VST3 via `juce::AudioPluginFormatManager` + `KnownPluginList` |
| Platform | Windows 11 SDK 10.0.26100 |

## Repository layout

```
simple-daw/
├── CMakeLists.txt            JUCE GUI app + module list + GLOB_RECURSE sources
├── build-dev.bat             Initializes VsDevCmd, then CMake configure + build
├── AGENTS.md                 Project memory / next-session plan (edit this!)
├── README.md                 This file
├── docs/
│   ├── daw-architecture.md            High-level architecture and data flow
│   ├── juce-pinning.md                How to restore / upgrade the pinned JUCE
│   ├── learning-plan-week-0-2-3.md    Week-by-week learning log
│   └── learning-plan-juce-dsp.md      DSP learning plan
├── src/
│   ├── Main.cpp              MainComponent + MainWindow + JUCEApplication
│   ├── audio/
│   │   ├── AudioTrackSource.{h,cpp}   AudioSource that streams an AudioBuffer
│   │   └── Recorder.{h,cpp}           AudioIODeviceCallback -> WAV writer
│   ├── midi/
│   │   ├── SynthAudioSource.{h,cpp}   Owns Synthesiser + MidiKeyboardState
│   │   ├── SineVoice.{h,cpp}          Polyphonic sine voice
│   │   ├── DemoSound.{h,cpp}          Trivial SynthesiserSound (always applies)
│   │   ├── MidiNote.h                 Note data (pitch, startBeat, length, vel)
│   │   ├── MidiClip.{h,cpp}           Vector<MidiNote> + undo/redo (cap 200)
│   │   ├── MidiTrack.{h,cpp}          Sequencer engine (beat clock + synth)
│   │   └── MidiOutputRouter.h         Selectable MidiOutput wrapper
│   ├── plugin/
│   │   ├── PluginHost.{h,cpp}         AudioPluginFormatManager + KnownPluginList
│   │   └── PluginWindows.h            PluginChooserDialog + PluginEditorWindow
│   ├── tracks/
│   │   ├── AudioTrack.{h,cpp}         Mixer strip (gain/pan/mute/solo + plugin)
│   │   ├── TrackRow.{h,cpp}           Per-track UI row (drag source + drop target)
│   │   └── TracksViewport.{h,cpp}     Viewport + tracks/trackRows + add/remove/reorder
│   ├── ui/
│   │   ├── PianoRollComponent.{h,cpp} Piano roll (draw/move/select/clipboard/undo)
│   │   ├── PeakMeterComponent.h       Log-scale master peak meter
│   │   ├── TrackMeterComponent.h      Log-scale per-track peak meter
│   │   ├── SettingsWindow.h           AudioDeviceSelectorComponent in DocumentWindow
│   │   └── TransportBar.{h,cpp}       Top row + status label (callback-driven)
│   └── session/
│       └── SessionIO.{h,cpp}          .sdaw save/load + recording helper
├── third_party/
│   ├── JUCE/                 gitignored, depth-1 clone (see JUCE.commit)
│   └── JUCE.commit           Pinned JUCE SHA (1 line of git output)
└── build/                   gitignored
```

## Build

### Prerequisites

- Visual Studio 2022 (MSVC v143, C++20) with the Windows 11 SDK
- CMake 3.22+ (winget: `C:\Program Files\CMake\bin\cmake.exe`)
- Git
- JUCE 8 cloned at `third_party/JUCE/` and checked out to the commit
  pinned in `third_party/JUCE.commit` (currently `3ba67d4`).
  See [docs/juce-pinning.md](docs/juce-pinning.md) for restore / upgrade
  steps:
  ```powershell
  git clone https://github.com/juce-framework/JUCE.git third_party/JUCE
  cd third_party/JUCE
  git fetch --unshallow
  git checkout 3ba67d4585e9d1fbcdb26a877c7978608b1f802e
  cd ..\..
  ```

### Build commands

From PowerShell:

```powershell
.\build-dev.bat            # Debug build (default, ~28 MB exe)
.\build-dev.bat Release    # Release build (~7 MB exe)
.\run-tests.bat            # Build + run MidiClip unit tests
```

`build-dev.bat` initializes `VsDevCmd.bat`, then runs CMake configure + build.
Output is at:

```
build\SimpleDaw_artefacts\<Config>\Simple DAW.exe
```

Launch from PowerShell so stdout (MIDI / audio logs) is visible:

```powershell
& "build\SimpleDaw_artefacts\Debug\Simple DAW.exe"
```

## Audio device

The app opens the system default output (typically Komplete Audio 1 via
WASAPI, 480-sample buffer at 48 kHz). Click **Settings** in the top row
to switch to ASIO for ~2–3 ms latency and change buffer size / sample
rate.

## Conventions

- C++20 strict, no compiler extensions.
- `std::atomic` for every parameter shared between the GUI and audio
  threads; `juce::SmoothedValue<float>` for any parameter that changes
  during playback.
- `juce::Slider` text boxes are non-editable
  (`setTextBoxIsEditable(false)`) on drag-only sliders.
- Toggle buttons use `juce::TextButton` + `setClickingTogglesState(true)`,
  not `juce::ToggleButton`.
- No comments in code unless requested.
- Window size is locked at **960 × 600**.
- Commits at the end of each milestone.

## Roadmap

A full breakdown lives in
[docs/daw-architecture.md](docs/daw-architecture.md). The core MVP scope
(multi-track audio, VST3 inserts, A/B loop, piano roll + sequencer,
recording, save/load, MIDI output routing) is complete. Remaining
polish items:

- App icon via `juce_add_app_icon` (drop a `.ico` in `resources/`)

## License

Personal learning project, no license granted yet.
