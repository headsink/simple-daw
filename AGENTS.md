# Simple DAW — Project Memory

**Path:** `C:\Users\User\OneDrive\2026\Viibe Codng Stuff\simple-daw`
**Stack:** C++20 · JUCE 8 · CMake 3.22+ · MSVC v143 · Windows 11 SDK 10.0.26100 · ASIO via Komplete Audio Driver (Native Instruments Komplete Audio 1)
**Goal:** Multi-track DAW with audio + MIDI tracks, piano roll editor, VST3 plugin hosting.

---

## Current state (as of last session)

**Week 0 — Complete.** Toolchain scaffolded, JUCE 8 cloned, hello-world builds and runs.
**Week 3 — Complete.** Audio I/O works end-to-end: `AudioAppComponent` + `SineAudioSource` + `juce::Slider` driving `std::atomic<float>` frequency. Tone plays through the Komplete Audio 1 interface, slider changes pitch live.
**Week 4 — Complete.** MIDI synth works end-to-end: custom `SineVoice` + `juce::Synthesiser` + `MidiKeyboardState` + on-screen `MidiKeyboardComponent` (5 octaves) + auto-open all USB MIDI inputs (Komplete Audio 1 ports).
**Audio track — Complete.** Load WAV / AIFF / FLAC / MP3 via `FileChooser`, decoded by `AudioFormatManager::registerBasicFormats()` into an `AudioBuffer<float>`. New `AudioTrackSource : juce::AudioSource` with playhead, plays through the Komplete Audio 1 via the existing ASIO engine. Transport: Load WAV / Play (toggles to Pause) / Stop. **Synth and WAV mix in parallel** in `getNextAudioBlock` (synth gain 0.6, track gain 1.0). All committed to git.

### What's working

- `CMakeLists.txt` — JUCE 8 GUI app target, `juce_generate_juce_header(SimpleDaw)`, links `juce_audio_utils`, `juce_audio_devices`, `juce_audio_formats`, `juce_audio_processors`, `juce_dsp`, `juce_gui_extra` (must be listed explicitly so `JuceHeader.h` aggregates them)
- `src/Main.cpp` — `MainComponent : juce::AudioAppComponent + MidiInputCallback + Timer`:
  - `SineVoice : SynthesiserVoice` — 8-voice polyphonic sine; velocity → amplitude; `MidiMessage::getMidiNoteInHertz` → frequency
  - `DemoSound : SynthesiserSound` — minimal subclass (base ctor is protected + has pure-virtuals)
  - `SynthAudioSource : AudioSource` — owns `juce::Synthesiser` and `juce::MidiKeyboardState`; renders via `keyboardState.processNextMidiBuffer` → `synth.renderNextBlock`
  - `AudioTrackSource : AudioSource` — holds decoded `AudioBuffer<float>` + playhead; `loadFile(File)`, `togglePlay()`, `stop()`, `seekToStart()`; auto-stops at end of buffer
  - `openAllMidiInputs()` — opens every device from `MidiInput::getAvailableDevices()`, routes note-on/off into the shared `MidiKeyboardState`
  - GUI: 5-octave `MidiKeyboardComponent` (C2–C7), `Load WAV` / `Play` / `Stop` transport buttons, `trackLabel` showing file metadata, `statusLabel` with device / buffer / rate / MIDI ports (Timer-refreshed every 500 ms)
  - `getNextAudioBlock` mixes synth + audio track into the same output via two scratch buffers
- `build-dev.bat` — initializes VsDevCmd, then CMake configure + build (Markdown Viewer pattern)
- `.gitignore` — excludes `build/`, `third_party/`, `.vs/`
- `docs/learning-plan-juce-dsp.md` — full 6-week plan
- `docs/learning-plan-week-0-2-3.md` — detailed expansion of Weeks 0, 2, 3

### Verified

- Build: `build-dev.bat Debug` produces `build\SimpleDaw_artefacts\Debug\Simple DAW.exe`
- Audio: sine tone + WAV playback through Komplete Audio 1 ASIO; slider changes pitch live; synth and WAV mix in parallel
- MIDI: on-screen keyboard + auto-opened Komplete Audio 1 MIDI ports work
- Toolchain: MSVC 19.44.35228, CMake 4.3.3 (winget-installed at `C:\Program Files\CMake\`), Git 2.53.0, SDK 10.0.26100.0

### Known quirks

- **CMake not on PATH by default.** winget installed it to `C:\Program Files\CMake\bin\cmake.exe` but plain PowerShell sessions don't see it. `build-dev.bat` adds it explicitly.
- **`vcvars64.bat` path:** `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat` (not the literal `C:\Program Files (x86)\...` path from older VS versions)
- **Output exe has a space:** JUCE's CMake helper nests it at `build\SimpleDaw_artefacts\<Config>\Simple DAW.exe`, not `build\<Config>\SimpleDaw.exe`. The bat script looks in the right place.
- **OneDrive caveat (from Markdown Viewer):** if MSBuild ever complains about read-only files in `build/`, redirect output to `C:\Users\User\AppData\Local\Temp\opencode\simple-daw-target\` via `CMAKE_BUILD_DIR` or a `.cmake` user preset. Not needed yet.
- **`JuceHeader.h` aggregation pitfall:** `juce_generate_juce_header` only includes modules named in `target_link_libraries`. Linking only `juce::juce_gui_extra` makes the auto header miss the audio modules — you get cryptic "identifier not found" / "unrelated types" errors. Solution: list `juce_audio_utils` / `juce_audio_devices` / `juce_audio_formats` / `juce_audio_processors` / `juce_dsp` explicitly. If you change the link list, delete `build\SimpleDaw_artefacts\JuceLibraryCode\` to force regeneration.
- **`juce::SynthesiserSound` is not directly instantiable.** Constructor is protected and `appliesToNote` / `appliesToChannel` are pure virtual. Must subclass with a concrete `DemoSound` (or similar) that returns `true` for both.
- **`MidiKeyboardComponent` has no `setNumOctavesForKeyboard`.** Use `setAvailableRange(int lowestNote, int highestNote)` to set the visible range, `setKeyWidth(float px)` to set key size. Constructor is `MidiKeyboardComponent(MidiKeyboardState&, Orientation)`.
- **`MidiKeyboardComponent` colour IDs are limited.** `whiteNoteColourId`, `blackNoteColourId`, `keySeparatorLineColourId`, `mouseOverKeyOverlayColourId` exist. `mouseDownKeyFillColourId` does NOT exist on this class (it's on `KeyMappingEditorComponent`).
- **`MidiInput::openDevice` returns the input directly (not a pointer).** Use `auto input = openDevice(...); if (input) { input->start(); openedInputs.add(input.release()); }`. The returned object's lifetime is yours.
- **`MidiDeviceInfo` has no `isEnabled()`.** For "open all available", just iterate and call `openDevice`; it returns an empty object on failure which the `if (input)` check handles.
- **MIDI input callback method is `handleIncomingMidiMessage(MidiInput*, MidiMessage&)`** (not `midiInputCallback`). Both are mentioned in docs but only the former is the actual interface method.
- **`FileChooser::launchAsync` `int flags` shadows `juce::Component::flags`** if you name the local variable `flags` — rename to `chooserFlags` or similar. Warning only, not an error.
- **`AudioFormatManager::registerBasicFormats()`** bundles WAV/AIFF/FLAC/MP3 decoders; on Windows the WAV path uses the system Media Foundation fallback, the others use JUCE's bundled libs. No external codec install needed.

---

## Toolchain locations (re-verify on next session)

| Tool | Path |
|------|------|
| VsDevCmd | `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat` |
| vcvars64 | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat` |
| cl.exe | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe` |
| CMake | `C:\Program Files\CMake\bin\cmake.exe` |
| JUCE | `third_party/JUCE/` (depth-1 clone, master branch) |
| Audio device | Komplete Audio 1 (Native Instruments) via Komplete Audio Driver (ASIO) |

---

## Build commands

```powershell
cd "C:\Users\User\OneDrive\2026\Viibe Codng Stuff\simple-daw"
.\build-dev.bat           # Debug build (default)
.\build-dev.bat Release   # Release build
```

Output: `build\SimpleDaw_artefacts\<Config>\Simple DAW.exe`

To launch: `& "build\SimpleDaw_artefacts\Debug\Simple DAW.exe"` from PowerShell, or double-click in Explorer.

To see MIDI / audio logs while running, launch from PowerShell so the stdout is visible.

---

## Next session — pick up here

**Recommended next: Refactor into `src/audio/` + `src/midi/`, then Multi-track mixer.** Two-part plan:

### Part A — Refactor (30 min, mechanical)

`src/Main.cpp` is approaching 400 lines. Split it now, before adding more:

- `src/audio/AudioTrackSource.h` + `.cpp` — the WAV source, moved as-is
- `src/midi/SineVoice.h` + `.cpp` — the synth voice
- `src/midi/DemoSound.h` + `.cpp` — the placeholder sound
- `src/midi/SynthAudioSource.h` + `.cpp` — owns `Synthesiser` + `MidiKeyboardState`
- `src/Main.cpp` — composes them, no class bodies

The `CMakeLists.txt` `target_sources(...)` list changes from `src/Main.cpp` to a glob:
```cmake
file(GLOB_RECURSE SIMPLE_DAW_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
target_sources(SimpleDaw PRIVATE ${SIMPLE_DAW_SOURCES})
```

This is the 10-minute refactor that pays off in the mixer step.

### Part B — Multi-track mixer (1-2 hours, the real work)

Goal: `std::vector<AudioTrackSource>` with per-track volume/pan/mute/solo, summing into a master bus.

1. Introduce `struct Track` (or `class AudioTrack`) that owns its `AudioTrackSource` plus `gain` (atomic float), `pan` (atomic float), `mute` (atomic bool), `solo` (atomic bool).
2. Add per-track controls to the GUI: a "Add Track" button + a row per track with Load/Play/Stop/Volume/Pan/Mute/Solo.
3. In `getNextAudioBlock`: iterate tracks, render each into a per-track scratch buffer, apply gain + pan + mute + solo logic, sum into the master.
4. The synth stays as a "MIDI master" or "always-on instrument track" for now (defer full multi-MIDI-track until the sequencer lands).
5. Stretch: a master gain `Slider` + simple peak meter on the master output.

### Why mixer before sequencer
- The sequencer needs a place to put notes; in a real DAW that's a MIDI track in the mixer. Building the mixer first means the sequencer slots in as another track type rather than a special case.
- Mixing 1→2→3 tracks is a 30-line change once the `Track` struct exists; 3→N is mechanical.
- The audio + MIDI tracks we have today are sufficient to prove the architecture; we don't need a clip model yet.

### Code template (Track struct)

```cpp
struct AudioTrack
{
    std::unique_ptr<AudioTrackSource> source;
    std::atomic<float> gain{1.0f};
    std::atomic<float> pan{0.0f};
    std::atomic<bool> mute{false};
    std::atomic<bool> solo{false};

    void renderInto(juce::AudioBuffer<float>& dest, int startSample, int numSamples)
    {
        if (mute.load()) return;
        const float g = gain.load();
        for (int ch = 0; ch < dest.getNumChannels(); ++ch)
        {
            const int srcCh = ch % source->getBuffer().getNumChannels();
            const float p = pan.load();
            const float leftGain  = (ch == 0) ? g * (p <= 0.0f ? 1.0f : 1.0f - p) : 0.0f;
            const float rightGain = (ch == 1) ? g * (p >= 0.0f ? 1.0f : 1.0f + p) : 0.0f;
            // ...copy + apply gain...
        }
    }
};
```

(The exact pan law — equal-power vs linear — is a taste choice; default to linear for v1.)

### Stretch tasks (pick any)
- Loop the WAV between two markers (A/B loop)
- Time progress label ("0:42 / 3:18")
- Add `juce::MidiOutput` and route note-on/off from the on-screen keyboard to a selectable MIDI out
- Save/load the project state to JSON (track count, gains, file paths) so a session restores on relaunch
- A simple peak meter on the master output (just `getMagnitude(channel, 0, numSamples)` repainted from a `Timer`)

---

## Architectural decisions made

- **C++ over Rust** (user chose): VST3 SDK + JUCE + ASIO are first-class C++; Rust ecosystem is too thin for a serious DAW.
- **JUCE framework**: handles ASIO/WASAPI I/O, VST3 hosting, MIDI, file I/O, GUI primitives — all in one.
- **CMake over Projucer**: Projucer is being phased out. The `third_party/JUCE/examples/CMake/GuiApp/CMakeLists.txt` is the reference template.
- **Audio I/O backend**: ASIO primary (user has Komplete Audio 1 + Komplete Audio Driver), WASAPI as fallback.
- **Plugin format**: VST3 only for v1. CLAP support deferred to a later phase.
- **MIDI**: JUCE's `MidiBuffer` + `Synthesiser` for v1; sequencing/piano roll is custom `juce::Component` work.
- **Piano roll scope (decided)**: data model (`MidiNote` + `MidiClip`) is custom, UI is custom `juce::Component`s layered over a grid. JUCE provides the engine.
- **Synth voice style**: custom `juce::SynthesiserVoice` subclass for v1 — extensible for filter/envelope in later passes.
- **MIDI device handling**: open all available `MidiInput` devices on startup; route everything through the same `MidiKeyboardState`.
- **Mixing architecture**: at the moment, two `AudioSource`s render into scratch buffers in `getNextAudioBlock`, then sum into the output. This generalizes to N tracks by replacing the two hard-coded sources with a `std::vector<Track>`.
- **Refactor timing**: split `src/Main.cpp` into `src/audio/` and `src/midi/` subfolders before the mixer step — `Main.cpp` is already approaching 400 lines.

---

## Project structure (target, current state marked)

```
simple-daw/
├── CMakeLists.txt              ✓ done (Week 0 + audio modules + GLOB)
├── build-dev.bat               ✓ done
├── .gitignore                  ✓ done
├── README.md                   ✗ not yet
├── src/
│   ├── Main.cpp                ✓ Composes audio + midi (Week 4 + audio track)
│   ├── audio/                  ✗ Next session (AudioTrackSource extracted here)
│   ├── midi/                   ✗ Next session (SineVoice, DemoSound, SynthAudioSource)
│   ├── tracks/                 ✗ Phase 2+ (AudioTrack, MidiTrack)
│   ├── plugin/                 ✗ Week 6+ (VST3 host wrapper)
│   └── ui/                     ✗ Phase 2+ (PianoRoll, Mixer, TrackView)
├── third_party/
│   └── JUCE/                   ✓ cloned
├── docs/
│   ├── learning-plan-juce-dsp.md          ✓ done
│   ├── learning-plan-week-0-2-3.md       ✓ done
│   └── daw-architecture.md                ✗ not yet
└── build/                      gitignored
```

---

## MVP order (from earlier planning)

1. ✓ Week 0 toolchain
2. ✓ Week 3 — Audio I/O
3. ✓ Week 4 — MIDI input
4. ✓ **Audio track (WAV load + playback)**
5. **Refactor + Multi-track mixer** (next)
6. Piano roll editor (custom `juce::Component`s)
7. MIDI playback (sequencer → synth, drives one or more synth tracks in the mixer)
8. VST3 insert
9. Recording (ASIO input → audio track)

---

## Conventions (carry over from Markdown Viewer)

- No comments in code unless asked
- No emojis
- TypeScript/C++ strict where applicable
- All state mutations go through well-defined classes (no globals in audio thread)
- `std::atomic` for any parameter shared between GUI and audio threads
- `juce::SmoothedValue<float>` for any parameter that changes during playback
- Commit working state at the end of each week
