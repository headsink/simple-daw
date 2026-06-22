# Simple DAW — Project Memory

**Path:** `C:\Users\User\OneDrive\2026\Viibe Codng Stuff\simple-daw`
**Stack:** C++20 · JUCE 8 · CMake 3.22+ · MSVC v143 · Windows 11 SDK 10.0.26100 · ASIO via Komplete Audio Driver (Native Instruments Komplete Audio 1)
**Goal:** Multi-track DAW with audio + MIDI tracks, piano roll editor, VST3 plugin hosting.

---

## Current state (as of last session)

**Week 0 — Complete.** Toolchain scaffolded, JUCE 8 cloned, hello-world builds and runs.
**Week 3 — Complete.** Audio I/O works end-to-end: `AudioAppComponent` + `SineAudioSource` + `juce::Slider` driving `std::atomic<float>` frequency. Tone plays through the Komplete Audio 1 interface, slider changes pitch live.
**Week 4 — Complete.** MIDI synth works end-to-end: custom `SineVoice` + `juce::Synthesiser` + `MidiKeyboardState` + on-screen `MidiKeyboardComponent` (5 octaves) + auto-open all USB MIDI inputs (Komplete Audio 1 ports).
**Audio track — Complete.** Load WAV / AIFF / FLAC / MP3 via `FileChooser`, decoded by `AudioFormatManager::registerBasicFormats()` into an `AudioBuffer<float>`. New `AudioTrackSource : juce::AudioSource` with playhead, plays through the Komplete Audio 1 via the existing ASIO engine.
**Refactor — Complete.** Split `src/Main.cpp` into `src/audio/AudioTrackSource.{h,cpp}` and `src/midi/{SineVoice,DemoSound,SynthAudioSource}.{h,cpp}`. `CMakeLists.txt` switched to `file(GLOB_RECURSE ... "src/*.cpp")` with `CONFIGURE_DEPENDS`.
**FileChooser fix — Complete.** `launchAsync` opened the dialog invisibly behind the main window (no callback ever fired). Switched to `browseForFileToOpen()` (synchronous, modal) with `JUCE_MODAL_LOOPS_PERMITTED=1` defined in `CMakeLists.txt`. Debug logging was removed. All committed to git.
**Multi-track mixer — Complete.** `std::vector<std::unique_ptr<AudioTrack>>` with per-track gain (atomic float, default 0.25), mute (atomic bool). Each `AudioTrack` owns an `AudioTrackSource` and a scratch buffer; `renderInto()` applies mute + gain + linear pan and sums into the master bus. `TrackRow` component provides per-track GUI: name label, Load / Play / Stop buttons, Gain slider (range 0-2, value display, non-editable text box), Mute button, Remove (X) button. `MainComponent` has a `+ Add Track` button. Synth stays as the always-on instrument at gain 0.6 alongside the audio track list. Solo is in the data model + renderInto logic but not yet exposed in the UI. Default mixer state: 1 empty track, 0.25 gain (quieter than unity to protect speakers). Not yet committed.

### What's working

- `CMakeLists.txt` — JUCE 8 GUI app target, `juce_generate_juce_header(SimpleDaw)`, links `juce_audio_utils`, `juce_audio_devices`, `juce_audio_formats`, `juce_audio_processors`, `juce_dsp`, `juce_gui_extra`. Defines `JUCE_MODAL_LOOPS_PERMITTED=1` to enable `FileChooser::browseForFileToOpen` in Debug. Uses `file(GLOB_RECURSE ... CONFIGURE_DEPENDS "src/*.cpp")` so new modules are auto-picked up.
- `src/Main.cpp` — `MainComponent : juce::AudioAppComponent + MidiInputCallback + Timer`:
  - Owns `SynthAudioSource` (synth, gain 0.6) and `std::vector<std::unique_ptr<AudioTrack>> tracks;`
  - On-screen `MidiKeyboardComponent` (5 octaves, C2 to C7) wired to the synth's `MidiKeyboardState`
  - `+ Add Track` button creates a new `AudioTrack` + `TrackRow`; `X` button on a row removes it
  - `getNextAudioBlock` mixes synth + all audio tracks (mute + gain + pan, with solo logic) into the same output
  - Status label refreshed by `Timer` every 500 ms: device / buffer / rate / MIDI ports / per-track state (`T1:P T2:. T3:M`)
- `src/audio/AudioTrackSource.{h,cpp}` — `AudioSource` subclass. `playing` is `std::atomic<bool>` (GUI/audio thread safety).
- `src/midi/SineVoice.{h,cpp}` — 8-voice polyphonic sine; velocity → amplitude; `MidiMessage::getMidiNoteInHertz` → frequency
- `src/midi/DemoSound.{h,cpp}` — minimal `juce::SynthesiserSound` subclass
- `src/midi/SynthAudioSource.{h,cpp}` — owns `juce::Synthesiser` (8 `SineVoice`s + 1 `DemoSound`) and `juce::MidiKeyboardState`
- `src/tracks/AudioTrack.{h,cpp}` — `struct AudioTrack` owning `AudioTrackSource` + scratch buffer + `gain`/`pan`/`mute`/`solo` atomics + `renderInto()`
- `src/tracks/TrackRow.{h,cpp}` — per-track `Component` (name label, Load/Play/Stop, Gain slider, Mute, Remove)
- `build-dev.bat` — initializes VsDevCmd, then CMake configure + build (Markdown Viewer pattern)
- `.gitignore` — excludes `build/`, `third_party/`, `.vs/`
- `docs/learning-plan-juce-dsp.md` — full 6-week plan
- `docs/learning-plan-week-0-2-3.md` — detailed expansion of Weeks 0, 2, 3

### Verified

- Build: `build-dev.bat Debug` produces `build\SimpleDaw_artefacts\Debug\Simple DAW.exe`
- Audio: sine tone + multi-track WAV playback through Komplete Audio 1 WASAPI (Speakers endpoint, 480-sample buffer @ 48 kHz); user confirmed both tracks play simultaneously after the Mute/TextBox fix
- MIDI: on-screen keyboard + auto-opened Komplete Audio 1 MIDI ports work
- Refactor: behavior unchanged, build clean, smoke test passes
- FileChooser fix: user confirmed modal native dialog opens on top of Simple DAW
- Multi-track mixer: user confirmed two different WAVs play at the same time; per-track state visible in status bar
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
- **`FileChooser::launchAsync` can open invisibly behind the main window** on Windows when the parent window isn't fully focused. Fix: use `browseForFileToOpen()` (modal) instead. Requires `JUCE_MODAL_LOOPS_PERMITTED=1` in Debug.
- **`JUCE_MODAL_LOOPS_PERMITTED` defaults to 0 in Debug builds** because the MSVC debugger doesn't like modal message loops. Already set in `CMakeLists.txt`.
- **`FileChooser::browseForFileToOpen` signature is `bool browseForFileToOpen(FilePreviewComponent* = nullptr)`** — no `flags` parameter. Calling `browseForFileToOpen(int)` will not compile.
- **`juce::Slider` with `TextBoxRight` is editable by default.** A user can click into the text box and type "0.0" which silently sets the parameter to 0. Worse, on some Windows setups, clicking the text box can swallow focus from a nearby button (we saw a Play button on track 2 not firing when the user clicked it). **Fix:** call `setTextBoxIsEditable(false)` on every `juce::Slider` whose value should only be changed by dragging. The text box still shows the value, but can't be focused.
- **`juce::ToggleButton` is a poor fit for short labels like "M".** The 36-px button with a 1-letter label was easy to misclick and visually ambiguous. **Fix:** use a `juce::TextButton` with `setClickingTogglesState(true)` and a clear label like "Mute". Size 50-60px wide.
- **`Component::setColour` takes 2 args:** `setColour(colourId, colour)`. A single-arg call won't compile. `backgroundColourId` lives on `ResizableWindow` / `Label`, not `Component` — for plain `Component` background fills, override `paint()` and use a constant.
- **Data race in audio thread if `playing` is a plain `bool`.** `AudioTrackSource::playing` is now `std::atomic<bool>`. Set on GUI thread, read on audio thread.
- **`AudioFormatManager::registerBasicFormats()`** bundles WAV/AIFF/FLAC/MP3 decoders; on Windows the WAV path uses the system Media Foundation fallback, the others use JUCE's bundled libs. No external codec install needed.
- **WASAPI buffer size is 480** on the Komplete Audio 1 (not the 256 we'd use with ASIO). For lower latency, switch to the Komplete Audio ASIO driver in the audio settings panel — but the mixer works either way.

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

**Recommended next: Option C — VST3 plugin insert.** The mixer is in place; loading a VST3 onto a track is the highest "wow factor" step that uses everything we've built so far. Get Vital or Surge XT working as a per-track insert and the DAW starts to feel like the real thing.

### Option C — VST3 insert plan

1. In `AudioTrack`, replace (or augment) the per-track audio source with a slot that can host an `AudioPluginInstance`:
   - Add `std::unique_ptr<juce::AudioPluginInstance> plugin;`
   - In `renderInto`, after rendering the source (or instead of, if no file is loaded), call `plugin->processBlock(buffer, midi)` on a per-track buffer
   - Show the plugin's editor as a child window attached to the `TrackRow`
2. Add a "Load VST3" button to `TrackRow`:
   - Use `juce::AudioPluginFormatManager` + `VST3PluginFormat`
   - Scan a default location (e.g., `C:\Program Files\Common Files\VST3\`) and present a `juce::PluginListComponent` or simple list
   - Instantiate the chosen plugin via `formatManager.createPluginInstance(description, ...)`
3. Stretch: bypass button, preset save/load via `getStateInformation()` / `setStateInformation()`, multiple insert slots per track.
4. Stretch: a free VST3 to start with — [Surge XT](https://surge-synthesizer.github.io/) or [Vital](https://vital.audio/). Both are ~100MB+ downloads, both expose audio through VST3.

### Why this before piano roll
- Piano roll needs a `MidiClip` data model + a sequencer that emits `MidiBuffer` events. That's a multi-day project on its own.
- VST3 hosting uses the existing audio engine, the existing mixer, and the existing transport. It plugs into what we have, doesn't require new infrastructure.
- The result is immediately audible and useful: load a WAV, route it through a free reverb / EQ / compressor, hear the effect.

### Option B — Piano roll / sequencer (deferred)

Larger feature. Plan in two phases:

**Phase 1 — data + engine:**
- `MidiNote { int pitch; double startBeat; double lengthBeats; uint8_t velocity; }`
- `MidiClip { int id; double startBeat; double lengthBeats; std::vector<MidiNote> notes; }`
- `MidiTrack : juce::AudioSource` — owns a clip, a beat clock, and a synth. In `getNextAudioBlock`, advance the beat clock by `numSamples/sampleRate * bpm/60`, emit `MidiMessage::noteOn` / `noteOff` into a `MidiBuffer`, render the buffer through a `juce::Synthesiser`.

**Phase 2 — UI:**
- `PianoRollComponent : juce::Component` — left-side piano keys (click to audition), grid with bars/beats, drag-to-draw notes, drag edges to resize, right-click to delete
- Snap-to-grid (1/4, 1/8, 1/16, triplet)
- Playhead that scrolls during playback
- Velocity lane at the bottom (optional, stretch)

**Why not now:** the piano roll is ~60% of a DAW's complexity in UI code. Better to ship VST3 + a working "load plugin on a track" demo first, then attack the sequencer as a focused week-long project.

### Stretch tasks (pick any)
- **Add Pan slider** to `TrackRow` (data model is there, just needs UI + a `setPan(float)` call)
- **Add Solo button** to `TrackRow` (data model + renderInto logic is there, just needs UI)
- **Master gain** `juce::Slider` with `juce::SmoothedValue<float>` in the main window
- **Time progress label** per track ("0:42 / 3:18") updated by the existing Timer
- **A/B loop markers** for the WAV track
- **Peak meter** on the master output
- **JSON save/load** so the session restores on relaunch

---

## Architectural decisions made

- **C++ over Rust** (user chose): VST3 SDK + JUCE + ASIO are first-class C++; Rust ecosystem is too thin for a serious DAW.
- **JUCE framework**: handles ASIO/WASAPI I/O, VST3 hosting, MIDI, file I/O, GUI primitives — all in one.
- **CMake over Projucer**: Projucer is being phased out. The `third_party/JUCE/examples/CMake/GuiApp/CMakeLists.txt` is the reference template.
- **Audio I/O backend**: ASIO primary (user has Komplete Audio 1 + Komplete Audio Driver), WASAPI as fallback. Currently using WASAPI (status bar shows "Speakers (2- Komplete Audio 1)"); switching to ASIO requires clicking the audio setup panel on next run and picking the Komplete Audio ASIO device.
- **Plugin format**: VST3 only for v1. CLAP support deferred. JUCE's `AudioPluginFormatManager` + `KnownPluginList` + `VST3PluginFormat` handle scanning/hosting.
- **MIDI**: JUCE's `MidiBuffer` + `Synthesiser` for v1; sequencing/piano roll is custom `juce::Component` work.
- **Piano roll scope (decided)**: data model (`MidiNote` + `MidiClip`) is custom, UI is custom `juce::Component`s layered over a grid. JUCE provides the engine.
- **Synth voice style**: custom `juce::SynthesiserVoice` subclass for v1.
- **MIDI device handling**: open all available `MidiInput` devices on startup; route everything through the same `MidiKeyboardState`.
- **Mixing architecture**: `std::vector<std::unique_ptr<AudioTrack>>`; each track owns its `AudioTrackSource` and a scratch buffer; `renderInto()` applies mute/solo/gain/pan and sums into the master bus. Synth renders into a separate scratch buffer in `MainComponent` and is added to the master before the audio tracks. Default track gain is 0.25 (user preference; protects small speakers).
- **GUI thread ↔ audio thread safety**: every parameter shared between threads is `std::atomic`. `AudioTrackSource::playing` is `std::atomic<bool>`.
- **Slider text box policy**: every `juce::Slider` that should only be dragged uses `setTextBoxIsEditable(false)`. Editable text boxes can swallow focus from nearby buttons on Windows.
- **Toggle buttons**: prefer `TextButton` + `setClickingTogglesState(true)` over `ToggleButton` for short label / large hit-area buttons.
- **File dialog pattern:** use `browseForFileToOpen()` (modal) with `JUCE_MODAL_LOOPS_PERMITTED=1`, not `launchAsync`.
- **Refactor timing:** `src/Main.cpp` is split into `src/audio/`, `src/midi/`, `src/tracks/` subfolders via `GLOB_RECURSE`. New modules (plugin/, ui/) are auto-picked up.

---

## Project structure (target, current state marked)

```
simple-daw/
├── CMakeLists.txt              ✓ done (Week 0 + audio modules + GLOB + JUCE_MODAL_LOOPS_PERMITTED)
├── build-dev.bat               ✓ done
├── .gitignore                  ✓ done
├── README.md                   ✗ not yet
├── src/
│   ├── Main.cpp                ✓ Composes synth + multi-track audio (~290 lines)
│   ├── audio/
│   │   ├── AudioTrackSource.h  ✓ extracted
│   │   └── AudioTrackSource.cpp ✓ extracted (playing is std::atomic<bool>)
│   ├── midi/
│   │   ├── SineVoice.h         ✓ extracted
│   │   ├── SineVoice.cpp       ✓ extracted
│   │   ├── DemoSound.h         ✓ extracted
│   │   ├── DemoSound.cpp       ✓ extracted
│   │   ├── SynthAudioSource.h  ✓ extracted
│   │   └── SynthAudioSource.cpp ✓ extracted
│   ├── tracks/
│   │   ├── AudioTrack.h        ✓ Mixer track (gain/pan/mute/solo + renderInto)
│   │   ├── AudioTrack.cpp      ✓ extracted
│   │   ├── TrackRow.h          ✓ Per-track UI row
│   │   └── TrackRow.cpp        ✓ extracted
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
6. ✓ **Multi-track mixer** (gain, mute, per-track UI, Add/Remove)
7. **VST3 insert** (next)
8. Piano roll editor (custom `juce::Component`s)
9. MIDI playback (sequencer → synth, drives one or more synth tracks in the mixer)
10. Recording (ASIO input → audio track)

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
