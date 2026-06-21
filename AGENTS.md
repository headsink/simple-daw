# Simple DAW — Project Memory

**Path:** `C:\Users\User\OneDrive\2026\Viibe Codng Stuff\simple-daw`
**Stack:** C++20 · JUCE 8 · CMake 3.22+ · MSVC v143 · Windows 11 SDK 10.0.26100 · ASIO via Komplete Audio Driver (Native Instruments Komplete Audio 1)
**Goal:** Multi-track DAW with audio + MIDI tracks, piano roll editor, VST3 plugin hosting.

---

## Current state (as of last session)

**Week 0 — Complete.** Toolchain scaffolded, JUCE 8 cloned, hello-world builds and runs.
**Week 3 — Complete.** Audio I/O works end-to-end: `AudioAppComponent` + `SineAudioSource` + `juce::Slider` driving `std::atomic<float>` frequency. User confirmed tone plays through the Komplete Audio 1 interface and the slider changes pitch live without glitches. Committed to git.

### What's working

- `CMakeLists.txt` — JUCE 8 GUI app target, `juce_generate_juce_header(SimpleDaw)`, links `juce::juce_audio_utils`, `juce::juce_audio_devices`, `juce::juce_audio_formats`, `juce::juce_audio_processors`, `juce::juce_dsp`, `juce::juce_gui_extra` (audio modules must be listed explicitly so `JuceHeader.h` aggregates them)
- `src/Main.cpp` — `AudioAppComponent` with `SineAudioSource` (440 Hz sine into stereo), frequency `juce::Slider` 20 Hz–5 kHz, status `juce::Label` showing device/buffer/rate after `prepareToPlay`
- `build-dev.bat` — initializes VsDevCmd, then runs CMake configure + build (Markdown Viewer pattern)
- `.gitignore` — excludes `build/`, `third_party/`, `.vs/`
- `docs/learning-plan-juce-dsp.md` — full 6-week plan
- `docs/learning-plan-week-0-2-3.md` — detailed expansion of Weeks 0, 2, 3

### Verified

- Build: `build-dev.bat Debug` produces `build\SimpleDaw_artefacts\Debug\Simple DAW.exe`
- Audio: tone plays through Komplete Audio 1 ASIO, slider changes pitch live
- Toolchain: MSVC 19.44.35228, CMake 4.3.3 (winget-installed at `C:\Program Files\CMake\`), Git 2.53.0, SDK 10.0.26100.0

### Known quirks

- **CMake not on PATH by default.** winget installed it to `C:\Program Files\CMake\bin\cmake.exe` but plain PowerShell sessions don't see it. `build-dev.bat` adds it explicitly.
- **`vcvars64.bat` path:** `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat` (not the literal `C:\Program Files (x86)\...` path from older VS versions)
- **Output exe has a space:** JUCE's CMake helper nests it at `build\SimpleDaw_artefacts\<Config>\Simple DAW.exe`, not `build\<Config>\SimpleDaw.exe`. The bat script looks in the right place.
- **OneDrive caveat (from Markdown Viewer):** if MSBuild ever complains about read-only files in `build/`, redirect output to `C:\Users\User\AppData\Local\Temp\opencode\simple-daw-target\` via `CMAKE_BUILD_DIR` or a `.cmake` user preset. Not needed yet.
- **`JuceHeader.h` aggregation pitfall:** `juce_generate_juce_header` only includes modules named in `target_link_libraries`. Linking only `juce::juce_gui_extra` makes the auto header miss the audio modules — you get cryptic "identifier not found" / "unrelated types" errors. Solution: list `juce_audio_utils` / `juce_audio_devices` / `juce_audio_formats` / `juce_audio_processors` / `juce_dsp` explicitly. If you change the link list, delete `build\SimpleDaw_artefacts\JuceLibraryCode\` to force regeneration.

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

---

## Next session — pick up here

**Recommended next: Week 4 (MIDI).** User's Komplete Audio 1 has MIDI ports — we can use the on-screen `MidiKeyboardComponent` immediately, and a USB controller when one is plugged in.

### Week 4 plan

1. Read `juce::MidiKeyboardState` and `juce::MidiKeyboardComponent` API references (~20 min)
2. Read `juce::Synthesiser` and `juce::SynthesiserVoice` API references (~30 min) — focus on `renderNextBlock`
3. Skim `third_party/JUCE/examples/CMake/AudioPlugin/PluginEditor.cpp` for a `MidiKeyboardComponent` + `Synthesiser` reference
4. Refactor `src/Main.cpp`:
   - Keep the existing `MainComponent` shell but make space for the keyboard and the synth
   - Add a `SynthAudioSource : public juce::AudioSource` that owns a `juce::Synthesiser` and a `juce::MidiKeyboardState`
   - In its `getNextAudioBlock`: call `keyboardState.processNextMidiBuffer(midi, ...)` to inject keyboard events, then `synth.renderNextBlock(buffer, midi, ...)`
   - Add a `juce::MidiKeyboardComponent` wired to the state (8 octaves wide, visible on screen)
   - Replace the `SineAudioSource` (or keep it as a test tone on a parallel path) — the synth is the new sound source
5. (Optional) Open a USB MIDI device: in `initialise`, iterate `MidiInput::getAvailableDevices()`, log them, optionally auto-open the first one and call `startTimer` to keep the input callback running. JUCE's audio setup panel also exposes MIDI device selection.
6. Build, run, confirm:
   - The on-screen keyboard renders and is clickable
   - Clicking keys produces sound (oscillator + simple ADSR envelope in a custom `SynthesiserVoice`)
   - A USB MIDI controller plugged into the Komplete Audio 1 also produces sound
7. **Stretch:** second oscillator with detune, simple filter in the voice, multi-voice polyphony check, velocity-sensitive amplitude

### Code template (MIDI synth source)

```cpp
class SineVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override { return dynamic_cast<juce::SynthesiserSound*>(sound) != nullptr; }
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        currentAngle = 0.0;
        level = velocity * 0.15;
        double cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        double cyclesPerSample = cyclesPerSecond / getSampleRate();
        angleDelta = cyclesPerSample * juce::MathConstants<double>::twoPi;
    }
    void stopNote(float, bool allowTailOff) override
    {
        if (allowTailOff) { level = 0; clearCurrentNote(); }
        else { level = 0; clearCurrentNote(); }
    }
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (level <= 0.0) return;
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float currentSample = (float)(level * std::sin(currentAngle));
            for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
                outputBuffer.addSample(ch, startSample + sample, currentSample);
            currentAngle += angleDelta;
        }
    }
private:
    double currentAngle = 0.0, angleDelta = 0.0, level = 0.0;
};

class SynthAudioSource : public juce::AudioSource
{
public:
    SynthAudioSource() { for (int i = 0; i < 8; ++i) synth.addVoice(new SineVoice()); synth.clearSounds(); synth.addSound(new juce::SynthesiserSound()); }
    void prepareToPlay(int, double sampleRate) override { synth.setCurrentPlaybackSampleRate(sampleRate); }
    void releaseResources() override {}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        info.buffer->clear();
        juce::MidiBuffer midi;
        keyboardState.processNextMidiBuffer(midi, info.startSample, info.numSamples, true);
        synth.renderNextBlock(*info.buffer, midi, info.startSample, info.numSamples);
    }
    juce::MidiKeyboardState keyboardState;
private:
    juce::Synthesiser synth;
};
```

(Use `juce::SynthesiserSound` directly — it has a default empty sound class, or you can derive your own to gate `canPlaySound` cleanly.)

### Stretch tasks (pick any)
- Auto-open the first USB MIDI input on startup
- Add a waveform selector (sine/saw/square) — one line in `SineVoice::startNote`
- Add a `juce::Slider` for master amplitude with `juce::SmoothedValue<float>`
- Show MIDI activity in the status label ("MIDI: 3 devices, Komplete MIDI in active")

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

---

## Project structure (target, current state marked)

```
simple-daw/
├── CMakeLists.txt              ✓ done (Week 0 + audio modules added in Week 3)
├── build-dev.bat               ✓ done
├── .gitignore                  ✓ done
├── README.md                   ✗ not yet
├── src/
│   ├── Main.cpp                ✓ Week 3 (AudioAppComponent + SineAudioSource + Slider)
│   ├── audio/                  ✗ Week 5+ (refactor: move source out of Main.cpp)
│   ├── midi/                   ✗ Week 4+ (MidiNote, MidiClip, Sequencer)
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
2. ✓ **Week 3** — Audio I/O
3. **Week 4** — MIDI input (next)
4. Audio track (WAV load + playback)
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
