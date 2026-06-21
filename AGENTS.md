# Simple DAW — Project Memory

**Path:** `C:\Users\User\OneDrive\2026\Viibe Codng Stuff\simple-daw`
**Stack:** C++20 · JUCE 8 · CMake 3.22+ · MSVC v143 · Windows 11 SDK 10.0.26100 · ASIO (user's external interface)
**Goal:** Multi-track DAW with audio + MIDI tracks, piano roll editor, VST3 plugin hosting.

---

## Current state (as of last session)

**Week 0 — Complete.** Toolchain scaffolded, JUCE 8 cloned, hello-world app builds and runs.

### What's working

- `CMakeLists.txt` — JUCE 8 GUI app target, `juce_generate_juce_header(SimpleDaw)`, links `juce::juce_gui_extra` (transitively pulls in audio modules)
- `src/Main.cpp` — minimal `juce::JUCEApplication` with a `MainComponent` that draws "Simple DAW — Week 0 hello world"
- `build-dev.bat` — initializes VsDevCmd, then runs CMake configure + build (Markdown Viewer pattern)
- `.gitignore` — excludes `build/`, `third_party/`, `.vs/`
- `docs/learning-plan-juce-dsp.md` — full 6-week plan
- `docs/learning-plan-week-0-2-3.md` — detailed expansion of Weeks 0, 2, 3

### Verified

- Build: `build-dev.bat Debug` produces `build\SimpleDaw_artefacts\Debug\Simple DAW.exe`
- Launch: executable starts, runs without crash, can be killed cleanly
- Toolchain: MSVC 19.44.35228, CMake 4.3.3 (winget-installed at `C:\Program Files\CMake\`), Git 2.53.0, SDK 10.0.26100.0

### Known path quirks

- **CMake not on PATH by default.** winget installed it to `C:\Program Files\CMake\bin\cmake.exe` but plain PowerShell sessions don't see it. `build-dev.bat` adds it explicitly.
- **`vcvars64.bat` path:** `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat` (not the literal `C:\Program Files (x86)\...` path from older VS versions)
- **Output exe has a space:** JUCE's CMake helper nests it at `build\SimpleDaw_artefacts\<Config>\Simple DAW.exe`, not `build\<Config>\SimpleDaw.exe`. The bat script looks in the right place.
- **OneDrive caveat (from Markdown Viewer):** if MSBuild ever complains about read-only files in `build/`, redirect output to `C:\Users\User\AppData\Local\Temp\opencode\simple-daw-target\` via `CMAKE_BUILD_DIR` or a `.cmake` user preset. Not needed yet.

---

## Toolchain locations (re-verify on next session)

| Tool | Path |
|------|------|
| VsDevCmd | `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat` |
| vcvars64 | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat` |
| cl.exe | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe` |
| CMake | `C:\Program Files\CMake\bin\cmake.exe` |
| JUCE | `third_party/JUCE/` (depth-1 clone, master branch) |

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

**Recommended next: Week 3 (Audio I/O).** It's the keystone — once the audio callback works, every later feature slots into the same `getNextAudioBlock`.

### Week 3 plan (from `docs/learning-plan-week-0-2-3.md`)

1. Read `juce::AudioAppComponent` API reference (~20 min)
2. Read `juce::AudioSource` API reference (~20 min)
3. Read `third_party/JUCE/examples/CMake/GuiApp/Main.cpp` and `MainComponent.cpp` for the current project shape
4. Rewrite `src/Main.cpp` to derive `MainComponent` from `juce::AudioAppComponent` (not `juce::Component`):
   - Add `prepareToPlay(int samplesPerBlockExpected, double sampleRate) override`
   - Add `getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override` — fill with a 440 Hz sine wave
   - Add `releaseResources() override`
   - In constructor: call `setAudioChannels(0, 2)` to request stereo output
   - In destructor: call `shutdownAudio()`
5. Add a `juce::Slider` (20 Hz to 5 kHz) for frequency, connected via `std::atomic<float>` (audio thread must not lock)
6. Build, run, confirm:
   - The app's audio setup panel appears (or auto-select ASIO)
   - A tone plays through the user's external interface
   - Dragging the slider changes the pitch live without glitches
7. **Stretch:** print the selected device name + buffer size on screen, force ASIO selection at startup, add a second slider for amplitude with `juce::SmoothedValue<float>`

### Code template (from the Week 3 plan doc)

```cpp
class SineAudioSource : public juce::AudioSource
{
public:
    void prepareToPlay(int, double sampleRate) override
    {
        currentSampleRate = sampleRate;
        phase = 0.0;
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        const float freq = frequencyHz.load();
        const double phaseIncrement = freq / currentSampleRate;

        for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
        {
            auto* samples = info.buffer->getWritePointer(ch, info.startSample);
            for (int n = 0; n < info.numSamples; ++n)
            {
                samples[n] = 0.2f * std::sin(phase * juce::MathConstants<double>::twoPi);
                phase += phaseIncrement;
                if (phase >= 1.0) phase -= 1.0;
            }
        }
    }

    void releaseResources() override {}

    std::atomic<float> frequencyHz{440.0f};

private:
    double currentSampleRate = 44100.0;
    double phase = 0.0;
};
```

---

## Architectural decisions made

- **C++ over Rust** (user chose): VST3 SDK + JUCE + ASIO are first-class C++; Rust ecosystem is too thin for a serious DAW.
- **JUCE framework**: handles ASIO/WASAPI I/O, VST3 hosting, MIDI, file I/O, GUI primitives — all in one.
- **CMake over Projucer**: Projucer is being phased out. The `third_party/JUCE/examples/CMake/GuiApp/CMakeLists.txt` is the reference template.
- **Audio I/O backend**: ASIO primary (user has external interface), WASAPI as fallback. JUCE exposes both via `createAudioIODeviceType_ASIO()` / `_WASAPI()`.
- **Plugin format**: VST3 only for v1. CLAP support deferred to a later phase. JUCE's `AudioPluginFormatManager` + `KnownPluginList` handle scanning/hosting.
- **MIDI**: JUCE's `MidiBuffer` + `Synthesiser` for v1; sequencing/piano roll is custom `juce::Component` work.
- **Piano roll scope (decided)**: data model (`MidiNote` + `MidiClip`) is custom, UI is custom `juce::Component`s layered over a grid. JUCE provides the engine (sequencer feeds `MidiBuffer` to `Synthesiser` per `getNextAudioBlock`).

---

## Project structure (target, current state marked)

```
simple-daw/
├── CMakeLists.txt              ✓ done
├── build-dev.bat               ✓ done
├── .gitignore                  ✓ done
├── README.md                   ✗ not yet (consider for next session)
├── src/
│   ├── Main.cpp                ✓ Week 0 hello-world
│   ├── audio/                  ✗ Week 3+ (AudioEngine, Transport)
│   ├── midi/                   ✗ Week 4+ (MidiNote, MidiClip, Sequencer)
│   ├── tracks/                 ✗ Phase 2+ (AudioTrack, MidiTrack)
│   ├── plugin/                 ✗ Week 6+ (VST3 host wrapper)
│   └── ui/                     ✗ Phase 2+ (PianoRoll, Mixer, TrackView)
├── third_party/
│   └── JUCE/                   ✓ cloned
├── docs/
│   ├── learning-plan-juce-dsp.md          ✓ done
│   ├── learning-plan-week-0-2-3.md       ✓ done
│   └── daw-architecture.md                ✗ not yet (consider for next session)
└── build/                      gitignored
```

---

## MVP order (from earlier planning)

1. ✓ Week 0 toolchain
2. **Week 3** — Audio I/O (next)
3. Week 4 — MIDI input
4. Audio track (WAV load + playback)
5. Multi-track mixer
6. Piano roll editor (custom `juce::Component`s)
7. MIDI playback (sequencer → synth)
8. VST3 insert
9. Recording (ASIO input → audio track)

---

## Open questions for next session

- Does the user have ASIO drivers installed for their audio interface? (If not: install ASIO4ALL as fallback.) Check at audio device selection time.
- Does the user want a Release build now that the smoke test passed? Release matters for audio (no debug overhead, assertions off).
- Should we commit Week 0 to git before moving on? (No git repo exists yet — `simple-daw/` is not a git repo per the env metadata.)
- README + architecture doc — write these before or after Week 3?

---

## Conventions (carry over from Markdown Viewer)

- No comments in code unless asked
- No emojis
- TypeScript/C++ strict where applicable
- All state mutations go through well-defined classes (no globals in audio thread)
- `std::atomic` for any parameter shared between GUI and audio threads
- `juce::SmoothedValue<float>` for any parameter that changes during playback
