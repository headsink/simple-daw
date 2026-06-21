# Learning Plan: JUCE + Audio DSP Fundamentals

**Goal:** Build a working C++/JUCE foundation so the multi-track DAW project (simple-daw) is approachable, not magical. By the end, you can read JUCE source, write an audio callback without flinching, and understand why the DAW's architecture looks the way it does.

**Stack:** Windows 11 · C++20 · JUCE 8 · CMake 3.22+ · Visual Studio 2022 (MSVC v143) · ASIO (your external interface) · WASAPI (fallback)

**Duration:** ~6 weeks at ~1 hour/day. Adjust pacing freely.

**Style:** Theory first (short), then a tiny runnable demo, then a stretch task. Every week ends with a checkpoint demo that produces sound or visible output.

---

## Prerequisites (Day 0 — set up before Week 1)

These are blockers. Get them working in one sitting.

1. **Visual Studio 2022 Community** with workloads:
   - "Desktop development with C++"
   - MSVC v143 toolset
   - Windows 11 SDK (10.0.22621 or later)
2. **CMake 3.22+** — `cmake --version` in PowerShell
3. **Git** (for JUCE submodule) — `git --version`
4. **JUCE 8 source** — clone `https://github.com/juce-framework/JUCE` into `third_party/JUCE`
5. **Verify the toolchain builds a JUCE app:**
   - Open the official `JUCE/extras/Projucer/Builds/VisualStudio2022/` Projucer solution
   - Or run any of the `JUCE/examples` with CMake
   - Output: you can compile and run a JUCE GUI app

**Checkpoint:** A blank JUCE window opens on your machine. Save this project — you'll reuse the structure as the DAW's seed.

---

## Week 1 — C++ refresher (audio-flavored)

**Why:** JUCE source is modern C++ but unforgiving. Smart pointers, templates, lambdas, and the standard library will appear on page one.

**Topics:**
- RAII, `std::unique_ptr` / `std::shared_ptr` (JUCE uses these heavily)
- Move semantics (`std::move`, move constructors) — critical for audio buffers
- Templates — read a `juce::Array<T>` declaration
- Lambdas — used in DSP callbacks and GUI events
- `std::function` and `std::bind` (JUCE's `juce::Function` alternative)
- Value vs reference semantics — audio buffers are large, passed by reference
- Debug vs release builds — release matters for audio (assertions disabled)

**Resources:**
- *Effective Modern C++* — Scott Meyers (skim items 1-12, 25-30)
- JUCE source itself — pick a small header (e.g., `juce_MathsFunctions.h`) and read it
- cppreference.com for anything unfamiliar

**Demo:** Rewrite a small "ring buffer" in modern C++ using `std::unique_ptr` for the storage.

**Stretch:** Implement a simple audio delay effect (read N samples ago, mix with input) using a `std::vector<float>` ring buffer.

**Checkpoint:** You can read JUCE headers without reaching for the documentation every other line.

---

## Week 2 — Digital audio fundamentals

**Why:** Before touching JUCE's audio classes, you need to know what a sample is, why 44.1 kHz exists, and what "buffer size" actually means for latency.

**Topics:**
- Sample rate, bit depth, Nyquist
- Frames, samples, channels, interleaved vs non-interleaved
- Buffer size vs latency: `latency_ms = bufferSize / sampleRate * 1000`
- Common buffer sizes: 64 (low), 256 (typical), 1024 (safe)
- Floating-point audio range `[-1.0, 1.0]`; clipping behavior
- Decibels: `dB = 20 * log10(amplitude)`; `dBFS`
- Time domain vs frequency domain (preview — FFT comes later)
- Aliasing and basic anti-aliasing (one sentence is enough for now)

**Resources:**
- JUCE docs: `juce::AudioBuffer<float>` overview page
- *The Theory and Technique of Electronic Music* — Miller Puckette (free PDF, chapters 1-3)
- Wikipedia: "Sampling (signal processing)", "Nyquist frequency"

**Demo:** Write a 10-line program that generates a 1-second 440 Hz sine wave into a `std::vector<float>` and writes it to a WAV file (use JUCE's `WavAudioFormat` or a hand-rolled writer — both are fine).

**Stretch:** Convert the loop to fill a `juce::AudioBuffer<float>` instead of a raw vector. Note how the API is different.

**Checkpoint:** You can explain "why 256 samples at 48 kHz = ~5.3 ms latency" without a calculator.

---

## Week 3 — JUCE audio I/O

**Why:** This is the core. `AudioDeviceManager` is the heart of every JUCE audio app. Get comfortable with it and you're 30% done with the DAW.

**Topics:**
- `juce::AudioDeviceManager` — owns the device, sample rate, buffer size
- Device types: `createAudioIODeviceType_ASIO()`, `_WASAPI()` (Windows)
- `AudioAppComponent` — convenience base class for audio apps
- `prepareToPlay(sampleRate, blockSize)` — your chance to allocate buffers
- `getNextAudioBlock(bufferToFill)` — the real-time callback; **never allocate here**
- `releaseResources()` — cleanup
- ASIO vs WASAPI: latency, exclusive mode, device routing
- `AudioSource` / `AudioSourcePlayer` — higher-level wrapper

**Resources:**
- JUCE tutorial: **"Create a Hello World audio app"** (`Tutorial: Hello world audio app`)
- JUCE tutorial: **"Audio device manager"** (`Tutorial: The AudioDeviceManager class`)
- API: `juce::AudioAppComponent`, `juce::AudioDeviceManager`, `juce::AudioSource`
- Source: `JUCE/examples/Audio/` — `AppConfig.h` + `Main.cpp` patterns

**Demo:** Build an app that generates a sine wave in the audio callback. Route it through your external interface. Confirm ASIO is selected (JUCE will print it in the console).

**Stretch:** Add a slider (`juce::Slider`) that controls the sine frequency in real time. Use atomic-friendly state (lock-free or `std::atomic<float>`) so the audio thread never locks.

**Checkpoint:** You hear a tone through your interface, and you can change its frequency with a GUI slider while it plays. **This is the foundation of every audio app you'll ever build.**

---

## Week 4 — MIDI in JUCE

**Why:** MIDI is how a DAW gets musical input. The DAW's piano roll writes MIDI events; the synth consumes them.

**Topics:**
- MIDI message types: note on, note off, control change, pitch bend
- MIDI channels (1-16), note numbers (0-127, 60 = C4), velocity (1-127)
- `juce::MidiMessage` and `juce::MidiBuffer`
- `MidiInput::openDevice()` — list and connect to your MIDI device
- `MidiKeyboardState` — virtual on-screen keyboard
- `Synthesiser` + `SynthesiserVoice` — built-in polyphonic synth
- Sample-accurate timing: MIDI events carry a `sampleOffset` so they fire at the right point in the buffer
- Note-on/off flow: input → `MidiBuffer` → `Synthesiser::renderNextBlock()` → audio

**Resources:**
- JUCE tutorial: **"MIDI input"** (`Tutorial: Midi input`)
- JUCE tutorial: **"Build a MIDI synthesiser"** (`Tutorial: Build a MIDI synthesiser`)
- API: `MidiBuffer`, `MidiMessage`, `Synthesiser`, `MidiKeyboardState`
- Source: `JUCE/examples/CMake/AudioPlugin` for a self-contained synth

**Demo:** An on-screen keyboard (`juce::MidiKeyboardComponent`) plays a built-in synth. Plug in a USB MIDI controller — verify it triggers the same synth.

**Stretch:** Add two oscillators (sine + saw) with detune. Implement ADSR envelope yourself in a custom `SynthesiserVoice` subclass.

**Checkpoint:** Pressing a key on screen or a physical controller produces a sustained note from your interface's output.

---

## Week 5 — DSP building blocks

**Why:** Once audio is flowing, you need basic effects: gain, pan, EQ, delay, reverb. Learn them as graph nodes you can chain.

**Topics:**
- `juce::dsp::Gain`, `juce::dsp::Oscillator`, `juce::dsp::Filter`
- `juce::dsp::ProcessorChain` — chain processors in a single signal path
- `juce::dsp::StateVariableFilter` — one-pole filter family
- Smoothed values (`juce::SmoothedValue<float>`) — avoid zipper noise on parameter changes
- Sample-accurate parameter automation
- Delay lines: simple feedback delay, comb filter
- Biquad filters: low-pass, high-pass, band-pass (the `IIR::Coefficients` class)
- Anti-aliasing and oversampling (intro only)

**Resources:**
- JUCE tutorial: **"Add distortion/audio effects to a synth"** (`Tutorial: Add distortion/audio effects to a synth`)
- JUCE module docs: `juce_dsp` — read the class list in `modules/juce_dsp/`
- *Audio Effects: Theory, Implementation and Application* — Joshua D. Reiss (free online)
- Julius O. Smith III's online books: intro to digital filters

**Demo:** Build a 3-knob effect: gain → filter (cutoff) → delay (time + feedback). Route a synth through it.

**Stretch:** Implement a simple state-variable filter from scratch and compare its output to `juce::dsp::StateVariableFilter`.

**Checkpoint:** You can chain three DSP blocks and hear the result change as you turn knobs.

---

## Week 6 — Plugin hosting (VST3)

**Why:** A "DAW" that can't load plugins is a toy. JUCE's `AudioPluginFormatManager` handles VST3 scanning, instantiation, and the editor window.

**Topics:**
- VST3 format basics: a self-describing module with audio I/O and a UI
- `AudioPluginFormatManager` — register formats (`VST3PluginFormat`)
- `KnownPluginList` — scan directories, persist found plugins to XML
- `AudioPluginInstance` — the loaded plugin
- `PluginDescription` — metadata: name, manufacturer, ID
- `getStateInformation()` / `setStateInformation()` — save/recall presets
- Editor windows: `plugin->createEditor()` returns a `Component*`
- The `AudioProcessorGraph` — a node-based audio graph
- Parameter automation via `AudioProcessorParameter`

**Resources:**
- JUCE tutorial: **"Hosting a VST plugin"** (`Tutorial: Plugin hosting`)
- JUCE tutorial: **"AudioProcessorGraph"** — for the node graph
- API: `AudioPluginFormatManager`, `KnownPluginList`, `AudioPluginInstance`
- Source: `JUCE/examples/Hosting/` — full reference host
- Steinberg VST3 SDK docs (skim, don't deep-read): `https://www.steinberg.net/developers/`

**Demo:** Build a minimal host: a "Load Plugin" button, an effects slot, and a master gain. Load a free VST3 (e.g., [Surge XT](https://surge-synthesizer.github.io/) or [Vital](https://vital.audio/)) and play a synth through it.

**Stretch:** Add a second slot in series (e.g., EQ → Reverb). Use `AudioProcessorGraph` instead of a hand-rolled chain.

**Checkpoint:** You load a real third-party VST3, twiddle its knobs through the editor window, and hear it.

---

## End-of-plan capstone: a 1-track mini DAW

After Week 6, build a tiny but real DAW that proves the architecture for `simple-daw`:

- One audio track playing a WAV file
- One MIDI track with a 4-bar MIDI clip
- One VST3 insert slot on each track
- Play, stop, seek transport
- Save/load the session as a single JSON file

This is **not** the full multi-track DAW. It's the architecture validated. Once this works, scaling to N tracks is mostly UI work and a `std::vector<Track>`.

---

## Reference card (keep open while coding)

| Need | JUCE class |
|------|-----------|
| Choose audio interface | `juce::AudioDeviceManager` |
| Audio callback | `juce::AudioAppComponent::getNextAudioBlock` |
| Sample buffer | `juce::AudioBuffer<float>` |
| MIDI messages | `juce::MidiMessage` / `juce::MidiBuffer` |
| Virtual keyboard | `juce::MidiKeyboardComponent` + `MidiKeyboardState` |
| Built-in synth | `juce::Synthesiser` + `SynthesiserVoice` |
| MIDI I/O | `juce::MidiInput` / `juce::MidiOutput` |
| DSP blocks | `juce::dsp::ProcessorChain` |
| Smoothed knob | `juce::SmoothedValue<float>` |
| Load VST3 | `juce::AudioPluginFormatManager` + `KnownPluginList` |
| Plugin editor UI | `plugin->createEditor()` |
| Save plugin state | `plugin->getStateInformation()` |
| Audio graph | `juce::AudioProcessorGraph` |
| File dialog | `juce::FileChooser` |
| Properties (settings) | `juce::PropertiesFile` |

---

## Topic dependency graph

```
Week 1 (C++) ──┐
               ├─► Week 3 (Audio I/O) ──► Week 5 (DSP) ──► Week 6 (Plugin hosting) ──► Capstone
Week 2 (Audio) ┘        │
                       └─► Week 4 (MIDI) ─────────────────────────────────────────────┘
```

If you're short on time, **Week 3 and Week 4 are the highest-priority weeks** — they unlock both audio and MIDI playback, which is 80% of a usable DAW. Week 1, 2, 5, 6 are deepenable later.

---

## Cross-cutting skills (do these in parallel, any week)

- **Reading JUCE source** — `Ctrl+Click` in your IDE jumps to the framework code. Skim `juce_AudioBuffer.h` and `juce_Synthesiser.h` — they're well-commented.
- **The Projucer → CMake migration** — Projucer is being phased out. Use CMake with `juce_add_gui_app()` / `juce_add_plugin()`. The example CMakeLists in `JUCE/examples/CMake` is your template.
- **Reading Steinberg VST3 docs** — VST3 is a C++ API. Most of it is irrelevant for hosts (you only need `IAudioProcessor` and `IEditController`); the rest is for plugin authors.
- **Debugging audio glitches** — start by increasing the buffer size to rule out underruns. Use the Visual Studio audio debugger sparingly (it pauses the audio thread).
- **Studying the JUCE examples** — `JUCE/examples/CMake/Audio/`, `Hosting/`, and `DSP/` are the highest-value repos to read.
