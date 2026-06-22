# Simple DAW

A small, multi-track Digital Audio Workstation built in **C++20** on top of
[JUCE 8](https://github.com/juce-framework/JUCE). It is a personal learning
project targeting Windows 11 with MSVC, but the code itself is portable.

> Status: pre-alpha. Audio playback, MIDI synthesis and a basic mixer work
> end-to-end. VST3 hosting, piano roll and recording are on the roadmap
> (see [docs/daw-architecture.md](docs/daw-architecture.md)).

## Features

- **Multi-track audio playback.** Load `WAV / AIFF / FLAC / MP3` files into
  any number of tracks and mix them in real time. Per-track controls:
  - Load, Play/Pause, Stop
  - Gain (0.0 - 2.0, default 0.25)
  - Pan (-1.0 - +1.0, linear)
  - Mute, Solo, Loop (full-file loop, wraps at end of buffer)
  - Time progress label (`m:ss / m:ss`), refreshed every 500 ms
  - Remove
- **Built-in MIDI synth.** 8-voice polyphonic sine-wave synthesiser driven
  by an on-screen 5-octave `MidiKeyboardComponent` (C2 - C7) and any USB
  MIDI input that is connected when the app starts.
- **Master bus.** Master gain (0.0 - 2.0, default 0.8) with a
  `juce::SmoothedValue<float>` 50 ms ramp so large volume changes do not
  click.
- **Live status bar.** Shows the active audio device, buffer size, sample
  rate, open MIDI ports and a per-track state code (`P` playing, `M` muted,
  `.` idle).
- **Thread-safe mixer.** Every parameter shared between the GUI and the
  audio thread is `std::atomic`. The master gain uses a smoothed value.

## Stack

| Layer | Choice |
|------|------|
| Language | C++20 (MSVC v143, `/std:c++20`, no extensions) |
| Framework | JUCE 8 (depth-1 clone under `third_party/JUCE/`) |
| Build | CMake 3.22+ (`juce_add_gui_app`) |
| Audio I/O | JUCE `AudioAppComponent`, WASAPI default / ASIO ready |
| MIDI | `juce::MidiKeyboardState` + `juce::Synthesiser` |
| Formats | `AudioFormatManager::registerBasicFormats()` (WAV/AIFF/FLAC/MP3) |
| Platform | Windows 11 SDK 10.0.26100 |

## Repository layout

```
simple-daw/
├── CMakeLists.txt          JUCE GUI app + module list + GLOB_RECURSE sources
├── build-dev.bat           Initializes VsDevCmd, then CMake configure + build
├── AGENTS.md               Project memory / next-session plan (edit this!)
├── README.md               This file
├── docs/
│   ├── daw-architecture.md          High-level architecture and data flow
│   ├── learning-plan-week-0-2-3.md  Week-by-week learning log
│   └── learning-plan-juce-dsp.md    DSP learning plan
├── src/
│   ├── Main.cpp            MainComponent + MainWindow + JUCEApplication
│   ├── audio/
│   │   ├── AudioTrackSource.{h,cpp}   AudioSource that streams an AudioBuffer
│   ├── midi/
│   │   ├── SynthAudioSource.{h,cpp}   Owns Synthesiser + MidiKeyboardState
│   │   ├── SineVoice.{h,cpp}          Polyphonic sine voice
│   │   └── DemoSound.{h,cpp}          Trivial SynthesiserSound (always applies)
│   └── tracks/
│       ├── AudioTrack.{h,cpp}         Mixer strip (gain/pan/mute/solo + renderInto)
│       └── TrackRow.{h,cpp}           Per-track UI component (76 px tall)
├── third_party/JUCE/      gitignored, depth-1 clone
└── build/                 gitignored
```

## Build

### Prerequisites

- Visual Studio 2022 (MSVC v143, C++20) with the Windows 11 SDK
- CMake 3.22+ (winget: `C:\Program Files\CMake\bin\cmake.exe`)
- Git
- JUCE 8 cloned at `third_party/JUCE/` (depth 1, `master` branch):
  ```powershell
  git clone --depth 1 https://github.com/juce-framework/JUCE.git third_party/JUCE
  ```

### Build commands

From PowerShell:

```powershell
.\build-dev.bat            # Debug build (default)
.\build-dev.bat Release    # Release build
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

The app opens the system default output (typically Komplete Audio 1 on the
developer's machine via WASAPI, 480-sample buffer at 48 kHz). To switch to
ASIO for lower latency, use the JUCE audio settings panel (not yet wired
into the UI; see roadmap in `docs/daw-architecture.md`).

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
- Window size is locked at **960 x 600**.
- Commits at the end of each milestone.

## Roadmap

A full breakdown lives in
[docs/daw-architecture.md](docs/daw-architecture.md). Summary:

1. ~~Audio I/O~~ done
2. ~~MIDI synth + on-screen keyboard~~ done
3. ~~Audio track loading~~ done
4. ~~Multi-track mixer with pan / solo / mute / loop / time / master~~ done
5. **VST3 insert per track** (next)
6. A/B region loop (after VST3)
7. Piano roll + sequencer
8. Recording (ASIO input -> audio track)

## License

Personal learning project, no license granted yet.
