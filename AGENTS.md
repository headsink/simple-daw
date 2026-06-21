# Simple DAW — Project Memory

**Path:** `C:\Users\User\OneDrive\2026\Viibe Codng Stuff\simple-daw`
**Stack:** C++20 · JUCE 8 · CMake 3.22+ · MSVC v143 · Windows 11 SDK 10.0.26100 · ASIO via Komplete Audio Driver (Native Instruments Komplete Audio 1)
**Goal:** Multi-track DAW with audio + MIDI tracks, piano roll editor, VST3 plugin hosting.

---

## Current state (as of last session)

**Week 0 — Complete.** Toolchain scaffolded, JUCE 8 cloned, hello-world builds and runs.
**Week 3 — Complete.** Audio I/O works end-to-end: `AudioAppComponent` + `SineAudioSource` + `juce::Slider` driving `std::atomic<float>` frequency. Tone plays through the Komplete Audio 1 interface, slider changes pitch live.
**Week 4 — Complete.** MIDI synth works end-to-end: custom `SineVoice` + `juce::Synthesiser` + `MidiKeyboardState` + on-screen `MidiKeyboardComponent` (5 octaves) + auto-open all USB MIDI inputs (Komplete Audio 1 ports). User confirmed in-session that the build and smoke test passed. Committed to git.

### What's working

- `CMakeLists.txt` — JUCE 8 GUI app target, `juce_generate_juce_header(SimpleDaw)`, links `juce_audio_utils`, `juce_audio_devices`, `juce_audio_formats`, `juce_audio_processors`, `juce_dsp`, `juce_gui_extra` (must be listed explicitly so `JuceHeader.h` aggregates them)
- `src/Main.cpp` — `MainComponent : juce::AudioAppComponent + MidiInputCallback + Timer`:
  - `SynthAudioSource` owns `juce::Synthesiser` (8 `SineVoice`s + 1 `DemoSound`) and a `juce::MidiKeyboardState`
  - `SineVoice` — 8-voice polyphonic sine; velocity → amplitude; `MidiMessage::getMidiNoteInHertz` → frequency
  - `DemoSound` — minimal `juce::SynthesiserSound` subclass (base ctor is protected + has pure-virtuals)
  - On-screen `MidiKeyboardComponent` (5 octaves, C2 to C7) wired to the state
  - `openAllMidiInputs()` opens every device from `MidiInput::getAvailableDevices()`; routes note-on/off into the same `MidiKeyboardState` so on-screen keys light up
  - Status label refreshed by `Timer` every 500 ms: device / buffer / rate / open MIDI count + names
- `build-dev.bat` — initializes VsDevCmd, then CMake configure + build (Markdown Viewer pattern)
- `.gitignore` — excludes `build/`, `third_party/`, `.vs/`
- `docs/learning-plan-juce-dsp.md` — full 6-week plan
- `docs/learning-plan-week-0-2-3.md` — detailed expansion of Weeks 0, 2, 3

### Verified

- Build: `build-dev.bat Debug` produces `build\SimpleDaw_artefacts\Debug\Simple DAW.exe`
- Audio: tone plays through Komplete Audio 1 ASIO, slider changes pitch live
- MIDI synth: built and smoke-tested; on-screen keyboard + auto-opened MIDI inputs confirmed working (Komplete Audio 1 ports)
- Toolchain: MSVC 19.44.35228, CMake 4.3.3 (winget-installed at `C:\Program Files\CMake\`), Git 2.53.0, SDK 10.0.26100.0

### Known quirks

- **CMake not on PATH by default.** winget installed it to `C:\Program Files\CMake\bin\cmake.exe` but plain PowerShell sessions don't see it. `build-dev.bat` adds it explicitly.
- **`vcvars64.bat` path:** `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat` (not the literal `C:\Program Files (x86)\...` path from older VS versions)
- **Output exe has a space:** JUCE's CMake helper nests it at `build\SimpleDaw_artefacts\<Config>\Simple DAW.exe`, not `build\<Config>\SimpleDaw.exe`. The bat script looks in the right place.
- **OneDrive caveat (from Markdown Viewer):** if MSBuild ever complains about read-only files in `build/`, redirect output to `C:\Users\User\AppData\Local\Temp\opencode\simple-daw-target\` via `CMAKE_BUILD_DIR` or a `.cmake` user preset. Not needed yet.
- **`JuceHeader.h` aggregation pitfall:** `juce_generate_juce_header` only includes modules named in `target_link_libraries`. Linking only `juce::juce_gui_extra` makes the auto header miss the audio modules — you get cryptic "identifier not found" / "unrelated types" errors. Solution: list `juce_audio_utils` / `juce_audio_devices` / `juce_audio_formats` / `juce_audio_processors` / `juce_dsp` explicitly. If you change the link list, delete `build\SimpleDaw_artefacts\JuceLibraryCode\` to force regeneration.
- **`juce::SynthesiserSound` is not directly instantiable.** Constructor is protected and `appliesToNote` / `appliesToChannel` are pure virtual. Must subclass with a concrete `DemoSound` (or similar) that returns `true` for both. Don't trust the Week 3 plan doc on this — it says "use `juce::SynthesiserSound` directly" but the compiler rejects it.
- **`MidiKeyboardComponent` has no `setNumOctavesForKeyboard`.** Use `setAvailableRange(int lowestNote, int highestNote)` to set the visible range, `setKeyWidth(float px)` to set key size. Constructor is `MidiKeyboardComponent(MidiKeyboardState&, Orientation)`.
- **`MidiKeyboardComponent` colour IDs are limited.** `whiteNoteColourId`, `blackNoteColourId`, `keySeparatorLineColourId`, `mouseOverKeyOverlayColourId` exist. `mouseDownKeyFillColourId` does NOT exist on this class (it's on `KeyMappingEditorComponent`).
- **`MidiInput::openDevice` returns the input directly (not a pointer).** Use `auto input = openDevice(...); if (input) { input->start(); openedInputs.add(input.release()); }`. The returned object's lifetime is yours.
- **`MidiDeviceInfo` has no `isEnabled()`.** For "open all available", just iterate and call `openDevice`; it returns an empty object on failure which the `if (input)` check handles.
- **MIDI input callback method is `handleIncomingMidiMessage(MidiInput*, MidiMessage&)`** (not `midiInputCallback`). Both are mentioned in docs but only the former is the actual interface method.

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

**Recommended next: Audio track (load + play a WAV).** Reuses the audio engine from Week 3, completes the second MVP item, and unblocks multi-track + sequencer work that follows.

### Audio track plan (sketch)

1. Add a "Load WAV" button to the main window
2. In the click handler, use `juce::FileChooser` to pick a `.wav` file
3. Decode it via `juce::AudioFormatManager` + `juce::AudioFormatReader` → a `juce::AudioBuffer<float>` of samples
4. Add a new `AudioTrackAudioSource : juce::AudioSource` that holds the decoded buffer and a read head; renders samples into the output block based on `playPosition`
5. Add a transport: play / stop / seek (just a few buttons; no need to be a full `AudioPlayHead` yet)
6. In `MainComponent::getNextAudioBlock`: clear the buffer, then have the audio track render into it (replace the synth for now; we'll mix them later)
7. Build, run, drop a WAV in, hit play, hear it through the Komplete Audio 1

### Stretch (pick any)
- Keep both the synth and the WAV track running in parallel (mix)
- Loop the WAV between two markers
- Add volume + pan sliders for the WAV track
- Move `SineVoice` and `SynthAudioSource` into `src/audio/` and `src/midi/` subfolders (start the refactor — they're not part of `src/Main.cpp` once we have more components)

### Why this and not the sequencer
The piano roll / sequencer needs a `MidiClip` data model + a sequencer that turns it into `MidiBuffer` per `getNextAudioBlock`. The audio track is the simpler piece and uses the same engine we've already proven. Sequencing comes right after, with a clear dependency on the audio engine being correct.

### Code template (audio track source)

```cpp
class AudioTrackSource : public juce::AudioSource
{
public:
    void loadFile(const juce::File& file)
    {
        juce::AudioFormatManager mgr;
        mgr.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(mgr.createReaderFor(file));
        if (reader == nullptr) return;
        fileBuffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
        reader->read(&fileBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
        playPosition = 0;
    }

    void setPlaying(bool shouldPlay) { playing = shouldPlay; }
    void stop() { playPosition = 0; playing = false; }

    void prepareToPlay(int, double) override {}
    void releaseResources() override {}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        info.buffer->clear();
        if (!playing || fileBuffer.getNumSamples() == 0) return;
        const int numSamples = info.numSamples;
        const int remaining = fileBuffer.getNumSamples() - playPosition;
        const int toCopy = std::min(numSamples, remaining);
        for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
            info.buffer->copyFrom(ch, info.startSample, fileBuffer, ch % fileBuffer.getNumChannels(),
                                  playPosition, toCopy);
        playPosition += toCopy;
        if (playPosition >= fileBuffer.getNumSamples()) { playing = false; playPosition = 0; }
    }

private:
    juce::AudioBuffer<float> fileBuffer;
    int playPosition = 0;
    bool playing = false;
};
```

---

## Architectural decisions made

- **C++ over Rust** (user chose): VST3 SDK + JUCE + ASIO are first-class C++; Rust ecosystem is too thin for a serious DAW.
- **JUCE framework**: handles ASIO/WASAPI I/O, VST3 hosting, MIDI, file I/O, GUI primitives — all in one.
- **CMake over Projucer**: Projucer is being phased out. The `third_party/JUCE/examples/CMake/GuiApp/CMakeLists.txt` is the reference template.
- **Audio I/O backend**: ASIO primary (user has Komplete Audio 1 + Komplete Audio Driver), WASAPI as fallback. JUCE exposes both via `createAudioIODeviceType_ASIO()` / `_WASAPI()`.
- **Plugin format**: VST3 only for v1. CLAP support deferred to a later phase. JUCE's `AudioPluginFormatManager` + `KnownPluginList` handle scanning/hosting.
- **MIDI**: JUCE's `MidiBuffer` + `Synthesiser` for v1; sequencing/piano roll is custom `juce::Component` work.
- **Piano roll scope (decided)**: data model (`MidiNote` + `MidiClip`) is custom, UI is custom `juce::Component`s layered over a grid. JUCE provides the engine (sequencer feeds `MidiBuffer` to `Synthesiser` per `getNextAudioBlock`).
- **Synth voice style**: custom `juce::SynthesiserVoice` subclass for v1. This keeps the door open for adding filter/envelope stages in Week 5 without rewriting the dispatch.
- **MIDI device handling**: open all available `MidiInput` devices on startup; route everything through the same `MidiKeyboardState` so on-screen keys reflect external input. JUCE's audio setup panel still works for changing the selection.

---

## Project structure (target, current state marked)

```
simple-daw/
├── CMakeLists.txt              ✓ done (Week 0 + audio modules added in Week 3)
├── build-dev.bat               ✓ done
├── .gitignore                  ✓ done
├── README.md                   ✗ not yet
├── src/
│   ├── Main.cpp                ✓ Week 4 (synth + keyboard + MIDI input)
│   ├── audio/                  ✗ Next session (AudioTrackSource extracted here)
│   ├── midi/                   ✗ Sequencer + MidiClip + MidiNote (after audio track)
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
4. **Audio track (WAV load + playback)** (next)
5. Multi-track mixer
6. Piano roll editor (custom `juce::Component`s)
7. MIDI playback (sequencer → synth)
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
