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
**VST3 insert per track — Complete.** Per-track **VST3** (load), **Bypass** (toggle, amber), **Edit** (open plugin editor window) buttons + plugin name label below the button row. `PluginHost` owns `AudioPluginFormatManager` (VST3 format) + `KnownPluginList`. `AudioTrack` owns `std::unique_ptr<juce::AudioPluginInstance>`; `renderInto` calls `plugin->processBlock(scratchBuffer, emptyMidi)` after the source renders and before gain/pan/sum. Plugin chooser is a modal `DocumentWindow` with a `ListBox` of scanned plugins + Rescan button; double-click loads async via `createPluginInstanceAsync`. Plugin editor opens in its own `DocumentWindow` via `createEditorAndMakeActive()`. `CMakeLists.txt` defines `JUCE_PLUGINHOST_VST3=1`. Also fixed the throwaway-`TrackRow` wart in `refreshLayout` (now uses static `TrackRow::getPreferredHeight()`).
**Docs — Complete.** `README.md` and `docs/daw-architecture.md` created. Title-bar encoding bug was already fixed in source (plain ASCII `"Simple DAW"`); removed stale TODO.
**A/B region loop (Option 2) — Complete.** `AudioTrackSource` gains `std::atomic<int> loopStart{0}` / `loopEnd{0}`. Default `loopEnd = numSamples` so the existing full-file loop keeps working when A/B is not set. `getNextAudioBlock` clamps the playhead to `[loopStart, loopEnd)` and wraps to `loopStart` (not 0) when `looping` is on. **A/B** button in `TrackRow` opens a collapsible second row with **Set A** / **Set B** buttons (capture current playhead), two horizontal sliders for fine adjustment, a **Clear** button, and a status label showing region length. `TrackRow` height is dynamic: 76 px collapsed, 126 px with A/B panel open. `MainComponent` wraps `tracksContainer` in a `juce::Viewport` (220 px tall) so a row with the A/B panel open can no longer push the MIDI keyboard off-screen.

### What's working

- `CMakeLists.txt` — JUCE 8 GUI app, `juce_generate_juce_header`, links `juce_audio_utils`/`juce_audio_devices`/`juce_audio_formats`/`juce_audio_processors`/`juce_dsp`/`juce_gui_extra` (must be explicit for `JuceHeader.h` aggregation). Defines `JUCE_MODAL_LOOPS_PERMITTED=1`, `JUCE_PLUGINHOST_VST3=1`. Uses `file(GLOB_RECURSE ... CONFIGURE_DEPENDS "src/*.cpp")`.
- `src/Main.cpp` — `MainComponent : juce::AudioAppComponent + MidiInputCallback + Timer`. `tracksContainer` is wrapped in a `juce::Viewport` (220 px tall) so a row with the A/B panel open (126 px) can no longer push the MIDI keyboard off-screen.
  - Owns `SynthAudioSource` (synth), `PluginHost pluginHost`, and `std::vector<std::unique_ptr<AudioTrack>> tracks;`
  - On-screen `MidiKeyboardComponent` (5 octaves, C2 to C7) wired to synth's `MidiKeyboardState`
  - `+ Add Track` button creates a new `AudioTrack` + `TrackRow` (passing `pluginHost`); `X` on a row removes it
  - `Scan VST3` button triggers `pluginHost.scanForPlugins()` off the message thread
  - `Master` label + `masterGainSlider` (0-2, default 0.8, `SmoothedValue` ramp 50 ms) at the top right
  - `Synth` label + `synthGainSlider` (0-2, default 0.6) at the bottom
  - `prepareToPlay` forwards to `pluginHost.setSampleRate` / `setBlockSize` so async plugin instantiation uses the real device rate
  - `getNextAudioBlock` mixes synth + all audio tracks (mute + gain + pan + plugin insert, with solo logic), then applies smoothed master gain
  - Status label refreshed by `Timer` every 500 ms: device / buffer / rate / MIDI ports / per-track state. `Timer` also calls `refreshTimeLabel()` on every row.
- `src/audio/AudioTrackSource.{h,cpp}` — `AudioSource` subclass. `playing`, `looping`, `loopStart`, `loopEnd` are `std::atomic`; `playPosition` is `std::atomic<int>`. `getNextAudioBlock` clamps the playhead to `[loopStart, loopEnd)` and wraps to `loopStart` when `looping` is on. `setLoopStart` / `setLoopEnd` / `clearLoopRegion` for the A/B UI. `loadFile` initialises `loopStart=0`, `loopEnd=numSamples` (full-file fallback when A/B not set).
- `src/midi/SineVoice.{h,cpp}` — 8-voice polyphonic sine; velocity → amplitude; `MidiMessage::getMidiNoteInHertz` → frequency
- `src/midi/DemoSound.{h,cpp}` — minimal `juce::SynthesiserSound` subclass
- `src/midi/SynthAudioSource.{h,cpp}` — owns `juce::Synthesiser` (8 `SineVoice`s + 1 `DemoSound`) and `juce::MidiKeyboardState`
- `src/plugin/PluginHost.{h,cpp}` — wraps `juce::AudioPluginFormatManager` (VST3 format registered) + `juce::KnownPluginList`. `scanForPlugins()` scans `C:\Program Files\Common Files\VST3` and `%APPDATA%\VST3` via `PluginDirectoryScanner`. `createInstanceAsync(desc, cb)` forwards to `formatManager.createPluginInstanceAsync`. Stores `sampleRate`/`blockSize` set by `MainComponent::prepareToPlay`.
- `src/plugin/PluginWindows.h` — `PluginChooserDialog` (modal `DocumentWindow` + `ListBoxModel` + `ChangeListener`; lists `KnownPluginList::getTypes()`, double-click loads, Rescan button) and `PluginEditorWindow` (`DocumentWindow` hosting `createEditorAndMakeActive()`, self-deletes on close). Uses `using juce::Component::addAndMakeVisible;` to unhide the reference overload hidden by `ResizableWindow`.
- `src/tracks/AudioTrack.{h,cpp}` — `class AudioTrack` owning `AudioTrackSource` + scratch buffer + `gain`/`pan`/`mute`/`solo` atomics + `std::unique_ptr<juce::AudioPluginInstance> plugin` + `std::atomic<bool> pluginBypass` + `juce::MidiBuffer pluginMidiBuffer` + `renderInto()`. `setPlugin` calls `prepareToPlay` + `setBusesLayout` (stereo in/out). `renderInto` runs `plugin->processBlock(scratchBuffer, pluginMidiBuffer)` after the source renders, before gain/pan/sum (skipped if bypassed). Solo logic: if any track is soloed, only soloed tracks are audible. Mute always wins.
- `src/tracks/TrackRow.{h,cpp}` — per-track `Component`, height dynamic (76 px collapsed, 126 px with the A/B panel open via `getCurrentPreferredHeight()`). Row 1: name label + plugin label. Row 2 (left → right): Load / Play / Stop / time label / Gain / Pan / Mute / Solo / **Loop** / **A/B** / **VST3** / **Bypass** / **Edit** / X. When A/B is toggled on, an extra row appears with **Set A** / **Set B** / **Clear** buttons, start/end sliders, value labels (`A: 1:23.45` / `B: 2:01.30`), and a region-length status label. `TrackRow` takes `PluginHost&` as a second constructor arg. `openPluginChooser()` opens a `PluginChooserDialog`; the async creation callback calls `track.setPlugin` then `refreshPluginLabel`/`updateButtons` on the message thread. `showPluginEditor()` news a `PluginEditorWindow`. Same `setTextBoxIsEditable(false)` on sliders. Same `TextButton + setClickingTogglesState(true)` on Mute/Solo/Loop/Bypass/A/B.
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
- **WASAPI buffer size is 480** on the Komplete Audio 1. Switch to Komplete Audio ASIO for ~2-3 ms latency.
- **Window size is 960×600** — keep this as the default; user confirmed it fits. Do not bump above 600 without checking.
- **`juce::Viewport` inserts an extra component layer** between the viewed component and the viewport's parent. A child that calls `getParentComponent()->...->resized()` to reach the top-level window will land on the viewport (or its internal view holder), not `MainComponent`. **Use an explicit callback (e.g. `onLayoutChanged`) instead of walking the parent chain** when a child needs to trigger a parent layout. `getTopLevelComponent()->resized()` also fails: it returns the `DocumentWindow`, and `DocumentWindow::resized()` calls `setContentOwned`-equivalent with the *same* bounds, which is a no-op in JUCE — so `MainComponent::resized()` never fires. The only reliable pattern is a direct callback to `MainComponent::refreshLayout()`.
- **`Component::setBounds` is a no-op when bounds are unchanged.** Do not rely on calling `resized()` on an ancestor to propagate layout; call the layout function directly.
- **`juce::OwnedArray` owns its elements.** `OwnedArray::clear()` deletes all objects by default. Do NOT manually `delete` elements and then call `clear()` — that is a double-free. Just stop/release the objects, then call `clear()` (or let the destructor handle it).

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

**A/B region loop (Option 2) — Complete.** Per-track A/B region with collapsible panel. Runtime test pending.

**Recommended next: Piano roll / sequencer (Phase 1 — data + engine).** Now that the mixer and inserts are solid, the next big "DAW moment" is note editing. Two phases:

### Piano roll / sequencer

**Phase 1 — data + engine:**
- `MidiNote { int pitch; double startBeat; double lengthBeats; uint8_t velocity; }`
- `MidiClip { int id; double startBeat; double lengthBeats; std::vector<MidiNote> notes; }`
- `MidiTrack : juce::AudioSource` — owns a clip, a beat clock, and a synth. In `getNextAudioBlock`, advance the beat clock by `numSamples/sampleRate * bpm/60`, emit `MidiMessage::noteOn` / `noteOff` into a `MidiBuffer`, render the buffer through a `juce::Synthesiser`.

**Phase 2 — UI:**
- `PianoRollComponent : juce::Component` — left piano keys (click to audition), grid with bars/beats, drag-to-draw notes, drag-to-resize, right-click delete.
- Snap-to-grid (1/4, 1/8, 1/16, triplet).
- Playhead that scrolls during playback.
- Velocity lane at the bottom (optional, stretch).

### Other stretch tasks (pick any)
- **ASIO audio settings panel** — switch from WASAPI to Komplete Audio ASIO for ~2-3 ms latency.
- **Master peak meter** — `getMagnitude(channel, 0, numSamples)` repainted from a `Timer`.
- **JSON save/load** — restore tracks + plugin state on relaunch.
- **`juce::MidiOutput`** — route on-screen keyboard notes to a selectable MIDI out.
- **Release build** benchmark.
- **App icon** via `juce_add_app_icon` or manual `.ico`.

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
├── README.md                   ✓ done
├── src/
│   ├── Main.cpp                ✓ Composes synth + multi-track audio + master gain + VST3 host + A/B loop viewport
│   ├── audio/
│   │   ├── AudioTrackSource.h  ✓ (playing/looping/playPosition/loopStart/loopEnd atomic, A/B wrap to loopStart)
│   │   └── AudioTrackSource.cpp ✓
│   ├── midi/
│   │   ├── SineVoice.h         ✓
│   │   ├── SineVoice.cpp       ✓
│   │   ├── DemoSound.h         ✓
│   │   ├── DemoSound.cpp       ✓
│   │   ├── SynthAudioSource.h  ✓
│   │   └── SynthAudioSource.cpp ✓
│   ├── tracks/
│   │   ├── AudioTrack.h        ✓ Mixer track (gain/pan/mute/solo + plugin insert + A/B pass-throughs + renderInto)
│   │   ├── AudioTrack.cpp      ✓
│   │   ├── TrackRow.h          ✓ Per-track row: name, Load, Play, Stop, time, Gain, Pan, Mute, Solo, Loop, A/B, VST3, Bypass, Edit, X + plugin label + collapsible A/B panel
│   │   └── TrackRow.cpp        ✓
│   ├── plugin/
│   │   ├── PluginHost.h        ✓ AudioPluginFormatManager + KnownPluginList wrapper
│   │   ├── PluginHost.cpp      ✓
│   │   └── PluginWindows.h     ✓ PluginChooserDialog + PluginEditorWindow (inline)
│   └── ui/                     ✗ Phase 2+ (PianoRoll)
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
11. Piano roll editor (custom `juce::Component`s)
12. MIDI playback (sequencer → synth)
13. Recording (ASIO input → audio track)

**A/B region loop is complete. Next: piano roll / sequencer (Phase 1).**

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
