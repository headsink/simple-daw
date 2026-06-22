# Simple DAW — Project Memory

**Path:** `C:\Users\User\OneDrive\2026\Viibe Codng Stuff\simple-daw`
**Stack:** C++20 · JUCE 8 · CMake 3.22+ · MSVC v143 · Windows 11 SDK 10.0.26100 · ASIO via Komplete Audio Driver (Native Instruments Komplete Audio 1)
**Goal:** Multi-track DAW with audio + MIDI tracks, piano roll editor, VST3 plugin hosting.

---

## Current state (as of last session)

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

### What's working

- `CMakeLists.txt` — JUCE 8 GUI app, `juce_generate_juce_header`, links `juce_audio_utils`/`juce_audio_devices`/`juce_audio_formats`/`juce_audio_processors`/`juce_dsp`/`juce_gui_extra` (must be explicit for `JuceHeader.h` aggregation). Defines `JUCE_MODAL_LOOPS_PERMITTED=1`. Uses `file(GLOB_RECURSE ... CONFIGURE_DEPENDS "src/*.cpp")`.
- `src/Main.cpp` — `MainComponent : juce::AudioAppComponent + MidiInputCallback + Timer`:
  - Owns `SynthAudioSource` (synth) and `std::vector<std::unique_ptr<AudioTrack>> tracks;`
  - On-screen `MidiKeyboardComponent` (5 octaves, C2 to C7) wired to synth's `MidiKeyboardState`
  - `+ Add Track` button creates a new `AudioTrack` + `TrackRow`; `X` on a row removes it
  - `Master` label + `masterGainSlider` (0-2, default 0.8, `SmoothedValue` ramp 50 ms) at the top right
  - `Synth` label + `synthGainSlider` (0-2, default 0.6) at the bottom
  - `getNextAudioBlock` mixes synth + all audio tracks (mute + gain + pan, with solo logic), then applies smoothed master gain
  - Status label refreshed by `Timer` every 500 ms: device / buffer / rate / MIDI ports / per-track state. `Timer` also calls `refreshTimeLabel()` on every row.
- `src/audio/AudioTrackSource.{h,cpp}` — `AudioSource` subclass. `playing` and `looping` are `std::atomic<bool>`. `playPosition` is `std::atomic<int>`. New: `getPlayPosition()`, `getSampleRate()`, `setLooping(bool)`, `isLooping()`. When looping and playhead reaches end, wraps to 0.
- `src/midi/SineVoice.{h,cpp}` — 8-voice polyphonic sine; velocity → amplitude; `MidiMessage::getMidiNoteInHertz` → frequency
- `src/midi/DemoSound.{h,cpp}` — minimal `juce::SynthesiserSound` subclass
- `src/midi/SynthAudioSource.{h,cpp}` — owns `juce::Synthesiser` (8 `SineVoice`s + 1 `DemoSound`) and `juce::MidiKeyboardState`
- `src/tracks/AudioTrack.{h,cpp}` — `struct AudioTrack` owning `AudioTrackSource` + scratch buffer + `gain`/`pan`/`mute`/`solo` atomics + `renderInto()`. Solo logic: if any track is soloed, only soloed tracks are audible. Mute always wins.
- `src/tracks/TrackRow.{h,cpp}` — per-track `Component`, height 76px. Row contents (left → right): name label on top; second row has Load / Play / Stop / time label / Gain slider / Pan slider / Mute / Solo / **Loop** / X. Same `setTextBoxIsEditable(false)` on all sliders. Same `TextButton + setClickingTogglesState(true)` pattern on Mute/Solo/Loop.
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
- **`AudioFormatManager::registerBasicFormats()`** bundles WAV/AIFF/FLAC/MP3 decoders.
- **WASAPI buffer size is 480** on the Komplete Audio 1. Switch to Komplete Audio ASIO for ~2-3 ms latency.
- **Window size is 960×600** — keep this as the default; user confirmed it fits. Do not bump above 600 without checking.

---

## Toolchain locations

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

**Recommended next: Option C — VST3 plugin insert.** All mixer polish (Pan, Solo, Mute, Loop, Time, Master) is done. VST3 hosting is the next "DAW moment" and uses everything we have so far.

### Option C — VST3 insert plan

1. In `AudioTrack`, add `std::unique_ptr<juce::AudioPluginInstance> plugin;`
2. Add a "Load VST3" button to `TrackRow`:
   - `juce::AudioPluginFormatManager` with `VST3PluginFormat` registered
   - `juce::KnownPluginList` scans `C:\Program Files\Common Files\VST3\`
   - Present a `juce::PluginListComponent` (or a simple list dialog) for the user to pick
   - Instantiate via `formatManager.createPluginInstance(description, ...)`
3. In `AudioTrack::renderInto`:
   - After the source renders into `scratchBuffer`, call `plugin->processBlock(scratchBuffer, midi)` before the gain/pan/sum
4. Show the plugin's editor (`plugin->createEditorIfNeeded()`) as a child window attached to the `TrackRow`
5. Stretch: bypass button, preset save/load via `getStateInformation()` / `setStateInformation()`, multiple insert slots per track

### A/B region loop (Option 2) — deferred until after VST3

User decision: do full-file Loop (Option 1) now, do A/B region loop (Option 2 — sliders for loop start/end) **after VST3 and piano roll** land. The data model for A/B will need `std::atomic<int> loopStart`, `loopEnd` on `AudioTrackSource`, plus "Set A at current playhead" / "Set B at current playhead" buttons and two slider fields in `TrackRow`. Layout will need a redesign because the row is already busy.

### Option B — Piano roll / sequencer (deferred)

**Phase 1 — data + engine:**
- `MidiNote { int pitch; double startBeat; double lengthBeats; uint8_t velocity; }`
- `MidiClip { int id; double startBeat; double lengthBeats; std::vector<MidiNote> notes; }`
- `MidiTrack : juce::AudioSource` — owns a clip, a beat clock, and a synth. In `getNextAudioBlock`, advance the beat clock by `numSamples/sampleRate * bpm/60`, emit `MidiMessage::noteOn` / `noteOff` into a `MidiBuffer`, render the buffer through a `juce::Synthesiser`.

**Phase 2 — UI:**
- `PianoRollComponent : juce::Component` — left piano keys (click to audition), grid with bars/beats, drag-to-draw notes, drag-to-resize, right-click delete
- Snap-to-grid (1/4, 1/8, 1/16, triplet)
- Playhead that scrolls during playback
- Velocity lane at the bottom (optional, stretch)

### Other stretch tasks (pick any)
- **Switch to ASIO** (Komplete Audio 1) in the audio settings panel — drops latency from 10 ms to ~2-3 ms
- **A/B region loop** (after VST3 + piano roll)
- **Time progress label** done
- **Master gain** done (with `SmoothedValue`)
- **Peak meter** on the master output (just `getMagnitude(channel, 0, numSamples)` repainted from a `Timer`)
- **JSON save/load** so the session restores on relaunch
- **Add `juce::MidiOutput`** and route note-on/off from the on-screen keyboard to a selectable MIDI out
- **Fix the title bar encoding** (the `â□` in the window title is a UTF-8 / em-dash rendering issue — replace with ASCII `-` in the `MainWindow` constructor or set the title font to one with proper Unicode support)
- **Release build** benchmark
- **App icon** via `juce_add_app_icon` or manual `.ico`
- **README.md** + **docs/daw-architecture.md**

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
- **Loop model (v1)**: full-file loop. `AudioTrackSource::looping` atomic bool; if true, wrap `playPosition` to 0 at end of buffer. A/B region loop (Option 2) is deferred until after VST3 + piano roll.
- **Gain defaults**: tracks 0.25, synth 0.6, master 0.8 (user preference; protects small speakers).
- **GUI thread ↔ audio thread safety**: every parameter shared between threads is `std::atomic`. `AudioTrackSource::playing`/`looping`/`playPosition`, `AudioTrack::gain`/`pan`/`mute`/`solo`, `MainComponent::synthGain`/`masterGainTarget` are all atomic.
- **Parameter smoothing**: `juce::SmoothedValue<float>` for the master gain (50 ms ramp, prevents clicks on big volume changes). Per-track gain is *not* smoothed (uses raw atomic read); if zipper noise appears, add `SmoothedValue` per track.
- **Slider text box policy**: every `juce::Slider` that should only be dragged uses `setTextBoxIsEditable(false)`.
- **Toggle buttons**: prefer `TextButton` + `setClickingTogglesState(true)` over `ToggleButton`.
- **File dialog pattern:** `browseForFileToOpen()` (modal) with `JUCE_MODAL_LOOPS_PERMITTED=1`.
- **Refactor timing:** `src/Main.cpp` split into `src/audio/`, `src/midi/`, `src/tracks/` via `GLOB_RECURSE`.
- **Window size:** 960×600 is the user-confirmed default. Do not increase.

---

## Project structure (target, current state marked)

```
simple-daw/
├── CMakeLists.txt              ✓ done
├── build-dev.bat               ✓ done
├── .gitignore                  ✓ done
├── README.md                   ✗ not yet
├── src/
│   ├── Main.cpp                ✓ Composes synth + multi-track audio + master gain
│   ├── audio/
│   │   ├── AudioTrackSource.h  ✓ (playing/looping/playPosition atomic, looping wraps at end)
│   │   └── AudioTrackSource.cpp ✓
│   ├── midi/
│   │   ├── SineVoice.h         ✓
│   │   ├── SineVoice.cpp       ✓
│   │   ├── DemoSound.h         ✓
│   │   ├── DemoSound.cpp       ✓
│   │   ├── SynthAudioSource.h  ✓
│   │   └── SynthAudioSource.cpp ✓
│   ├── tracks/
│   │   ├── AudioTrack.h        ✓ Mixer track (gain/pan/mute/solo + renderInto)
│   │   ├── AudioTrack.cpp      ✓
│   │   ├── TrackRow.h          ✓ Per-track row: name, Load, Play, Stop, time, Gain, Pan, Mute, Solo, Loop, X
│   │   └── TrackRow.cpp        ✓
│   ├── plugin/                 ✗ Next session (VST3 host wrapper)
│   └── ui/                     ✗ Phase 2+ (PianoRoll)
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
4. ✓ Audio track (WAV load + playback)
5. ✓ Refactor (split into `src/audio/` + `src/midi/`)
6. ✓ Multi-track mixer
7. ✓ Synth gain slider
8. ✓ **Mixer polish** (Pan, Solo, Time, Master, Loop)
9. **VST3 insert** (next)
10. Piano roll editor (custom `juce::Component`s)
11. MIDI playback (sequencer → synth)
12. Recording (ASIO input → audio track)

After VST3, do **A/B region loop** (Option 2) as a small polish pass.

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
