# Learning Plan: Week 0, 2, and 3 (Detailed)

Companion to the full plan in `learning-plan-juce-dsp.md`. This file expands the three setup-and-foundation weeks into step-by-step tasks.

---

## Week 0 — Toolchain setup

**Goal:** A blank JUCE app builds and runs on your Windows 11 machine.

**Why first:** Nothing else matters until the toolchain works. JUCE builds are large; if CMake can't find MSVC or the SDK, debugging it later wastes hours.

### Steps

#### 1. Install Visual Studio 2022 Community

Download from `https://visualstudio.microsoft.com/downloads/`. During install, select these workloads:

- **Desktop development with C++** (required)
  - MSVC v143 toolset (x64/x86)
  - Windows 11 SDK (10.0.22621 or later)
  - C++ CMake tools for Windows

Verify in PowerShell:
```powershell
cl.exe
```
If not found, open a "Developer Command Prompt for VS 2022" (or run `vcvars64.bat`).

#### 2. Install CMake 3.22+

```powershell
winget install Kitware.CMake
cmake --version
```
Must report 3.22 or higher.

#### 3. Install Git (if missing)

```powershell
winget install Git.Git
git --version
```

#### 4. Clone JUCE 8 into the project

From your `simple-daw` workspace root:
```powershell
mkdir third_party
git clone --branch master https://github.com/juce-framework/JUCE.git third_party/JUCE
```

#### 5. Verify a JUCE app builds

Either:

- **Option A — CMake (recommended, modern path):** Use one of the JUCE examples as a template. Copy `third_party/JUCE/examples/CMake/AudioApp/CMakeLists.txt` and the matching `AudioApp.cpp` into a temporary folder, then:
  ```powershell
  cmake -B build -G "Visual Studio 17 2022" -A x64
  cmake --build build --config Release
  ./build/Release/AudioApp.exe
  ```
  A blank window should appear.

- **Option B — Projucer (legacy but still works):** Open `third_party/JUCE/extras/Projucer/Builds/VisualStudio2022/Projucer.sln`, build it, run it, generate a new GUI app, then open the generated `.sln` and build that.

#### 6. Sanity checks before moving on

- The JUCE window appears and closes cleanly
- No missing-DLL errors at startup (if you see `VCRUNTIME140.dll` missing, install the VS Redistributable)
- The build configuration is `x64`, not `x86`
- You can place a breakpoint in the app's `initialise()` and the debugger hits it

### Checkpoint

> **A blank JUCE window opens on your machine. Save this project — you'll reuse the structure as the DAW's seed.**

### Time estimate

2-4 hours, mostly installer wait time.

### Common failures and fixes

| Symptom | Cause | Fix |
|---------|-------|-----|
| `cl.exe` not recognized | VS not in PATH | Use "Developer Command Prompt" or run `vcvars64.bat` |
| `error MSB8036: Windows SDK not found` | SDK workload not installed | Re-run VS installer, add Windows 11 SDK |
| Linker errors about `kernel32.lib` | Wrong environment | Use Developer Command Prompt, not plain PowerShell |
| `JUCE headers not found` | Wrong CMake path | Confirm `third_party/JUCE` exists and is populated |
| App crashes on startup with missing DLL | Runtime not deployed | Install VS 2022 Redistributable (x64) |

---

## Week 2 — Digital audio fundamentals

**Goal:** Understand what a "sample" is, what buffer size means for latency, and how to generate audio in code.

**Why before JUCE audio I/O:** When you call `prepareToPlay(48000, 256)`, those numbers must mean something. This week builds that intuition.

### Topics

#### Sample rate

- Definition: how many audio samples per second
- Common rates: 44100 Hz (CD), 48000 Hz (video/pro audio), 96000 Hz (high-res)
- Nyquist theorem: max representable frequency = sample rate / 2

#### Bit depth

- Definition: how many bits represent one sample
- Common depths: 16-bit (CD), 24-bit (pro audio), 32-bit float (internal processing)
- 32-bit float is what JUCE uses internally; `[-1.0, 1.0]` is the valid range; values beyond clip to distortion

#### Frames, samples, channels

- One **sample** = one number for one channel at one instant
- One **frame** = one sample per channel at one instant (e.g., stereo frame = 2 samples)
- **Interleaved**: L R L R L R (one continuous buffer)
- **Non-interleaved** (JUCE's `AudioBuffer`): separate buffer per channel — `buffer[0]` is all left, `buffer[1]` is all right

#### Buffer size and latency

The core formula:
```
latency_ms = (bufferSize / sampleRate) * 1000
```

Examples:
- 64 samples @ 48 kHz = **1.33 ms** (low-latency, harder on CPU)
- 256 samples @ 48 kHz = **5.33 ms** (typical)
- 1024 samples @ 48 kHz = **21.3 ms** (safe, noticeable delay when monitoring)

For real-time monitoring through your interface, aim for under 10 ms. For mixing, 256 is the standard sweet spot.

#### Decibels

- `dB = 20 * log10(amplitude)`
- 0 dB = amplitude 1.0 (full scale, "0 dBFS")
- -6 dB ≈ amplitude 0.5
- -20 dB ≈ amplitude 0.1
- -inf dB = amplitude 0

#### Time vs frequency domain

- Time domain: the raw sample waveform
- Frequency domain: amplitude per frequency (via FFT)
- You only need to know the FFT exists this week. Defer to Week 5.

#### Aliasing (one-paragraph version)

Frequencies above Nyquist get mirrored back into the audible range, producing artifacts. Anti-aliasing = low-pass filter before sampling. Oversampling = running the math at 2x or 4x rate to push aliasing out of band.

### Resources

- **JUCE docs:** `juce::AudioBuffer<float>` overview page — `https://docs.juce.com/master/classjuce_1_1AudioBuffer.html`
- **Free book:** *The Theory and Technique of Electronic Music* — Miller Puckette — `https://msp.ucsd.edu/techniques.htm` (chapters 1-3)
- **Wikipedia:** "Sampling (signal processing)", "Nyquist frequency", "Decibel"
- **Practical primer:** "DAW basics: latency, buffer size, and sample rate" — any YouTube video by a reputable audio engineering channel (e.g., InTheMix, Make Pro Audio)

### Demo: generate a 440 Hz sine wave WAV

Two approaches. Pick one.

#### Option A — Hand-rolled WAV writer (teaches file format)

Write a 1-second mono 16-bit 440 Hz sine wave to `sine.wav`:
- Sample rate: 44100
- 44100 samples total
- For each sample `n`: `sin(2 * pi * 440 * n / 44100)`
- Scale to int16 range, write to file
- WAV header is 44 bytes (RIFF, fmt, data chunks)
- Reference: `http://soundfile.sapp.org/doc/WaveFormat/`

#### Option B — Use JUCE's `WavAudioFormat`

```cpp
juce::AudioBuffer<float> buffer(1, 44100);
auto* samples = buffer.getWritePointer(0);
for (int n = 0; n < 44100; ++n)
    samples[n] = std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * n / 44100.0);

juce::File outFile("sine.wav");
outFile.deleteFile();
auto stream = outFile.createOutputStream();
juce::WavAudioFormat wav;
auto writer = wav.createWriterFor(stream, juce::AudioFormatReaderOptions{}, 44100, 1, 16);
writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
```

Open the file in any audio player. You hear a 1-second A4 tone.

### Stretch

- Convert the loop to fill a `juce::AudioBuffer<float>` and add a second channel (stereo — duplicate the samples)
- Add a 1-second fade-in and fade-out by multiplying samples by a ramp
- Generate a chord (e.g., A4 + C#5 + E5) by summing three sines

### Checkpoint

> **You can explain "why 256 samples at 48 kHz = ~5.3 ms latency" without a calculator.**

### Time estimate

3-5 hours (mostly reading + experimentation).

### Common confusions to clear up

| Confusion | Clarification |
|-----------|---------------|
| "Higher sample rate = better quality" | Mostly marketing for music. The audible band is below 20 kHz regardless. Higher rates do help with plugin processing headroom and inter-plugin routing. |
| "Larger buffer = better quality" | No. Larger buffer = higher latency. The audio quality is the same. |
| "Bit depth affects dynamics" | 24-bit gives more headroom above the noise floor than 16-bit. For internal processing, use 32-bit float. |
| "Why does the output crackle at 64 samples?" | CPU can't keep up. Buffer underrun. Increase to 128 or 256. |

---

## Week 3 — JUCE audio I/O

**Goal:** Generate sound in the audio callback. Route it through your external interface. Control it with a GUI slider.

**Why this is the keystone week:** Once you understand `AudioDeviceManager` + `AudioAppComponent` + `getNextAudioBlock`, you understand 30% of every audio application ever built.

### Topics

#### `juce::AudioDeviceManager`

The class that owns:
- The selected audio device (ASIO device on Windows, etc.)
- The current sample rate and buffer size
- Audio and MIDI input/output routing

Key methods:
- `initialise(numInputChannels, numOutputChannels, ...)`
- `setAudioDeviceSetup(setup, true)` — change sample rate / buffer size at runtime
- `getCurrentAudioDevice()` — get the active device
- `getCurrentDeviceTypeObject()` — get the format selector (e.g., ASIO)

#### `juce::AudioIODeviceType` and Windows device types

Windows provides several:
- `createAudioIODeviceType_ASIO()` — **use this for your external interface**
- `createAudioIODeviceType_WASAPI()` — fallback; supports shared and exclusive mode
- `createAudioIODeviceType_DirectSound()` — legacy

ASIO is preferred for low-latency pro audio. WASAPI exclusive mode is second-best.

#### `juce::AudioAppComponent`

A convenience base class for audio apps. It:
- Owns an `AudioDeviceManager`
- Provides a `prepareToPlay`, `getNextAudioBlock`, `releaseResources` virtual methods
- Wires up the audio callback for you

Most simple audio apps derive from this.

#### The audio callback contract

Three virtual methods you override:

```cpp
void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
{
    // Allocate buffers, reset state
    // Called once when audio starts and whenever the device config changes
}

void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
{
    // Fill the buffer with audio
    // Runs on the audio thread — NEVER allocate, lock, or do I/O here
    bufferToFill.clearActiveBufferRegion();
    auto* left  = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
    auto* right = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);
    for (int n = 0; n < bufferToFill.numSamples; ++n)
    {
        left[n]  = generateSample();
        right[n] = generateSample();
    }
}

void releaseResources() override
{
    // Free what prepareToPlay allocated
}
```

**The single most important rule:** the audio thread is real-time. The OS will call `getNextAudioBlock` every ~5 ms. If your code takes longer than the buffer duration, you get a glitch. To stay safe:
- No `new` / `delete` in the callback
- No mutexes (use `std::atomic` or lock-free structures)
- No file I/O
- No logging
- No UI calls (no `repaint()`)

#### `juce::AudioSource` and `juce::AudioSourcePlayer`

Higher-level abstraction:
- `AudioSource` is anything that produces audio (`prepareToPlay`, `getNextAudioBlock`, `releaseResources`)
- `AudioSourcePlayer` is a bridge that connects an `AudioSource` to the `AudioIODevice`

For a single sound-generating object, this is cleaner than deriving from `AudioAppComponent`.

#### `juce::AudioPlayHead`

Provides position info to processors (current sample, time, BPM, time signature). For a DAW you'll implement this yourself to drive transport. For Week 3 you can ignore it.

#### Connecting a `juce::Slider` to the audio thread

GUI and audio threads are different. Don't pass a `Slider*` to the audio callback. Instead:
- Use `std::atomic<float>` for the parameter
- On the GUI thread, listen to `Slider::onValueChange` and write to the atomic
- On the audio thread, read the atomic
- For smoother changes, wrap in `juce::SmoothedValue<float>` (preview for Week 5)

```cpp
std::atomic<float> frequencyHz{440.0f};

// GUI thread
frequencySlider.onValueChange = [this]
{
    frequencyHz.store((float)frequencySlider.getValue());
};

// Audio thread (inside getNextAudioBlock)
const float freq = frequencyHz.load();
```

### Resources

- **JUCE tutorial:** *Create a Hello World audio app* — `https://docs.juce.com/master/tutorial_hello_world_audio_app.html`
- **JUCE tutorial:** *The AudioDeviceManager class* — `https://docs.juce.com/master/tutorial_audio_device_manager.html`
- **API reference:** `juce::AudioAppComponent` — `https://docs.juce.com/master/classjuce_1_1AudioAppComponent.html`
- **API reference:** `juce::AudioSource` — `https://docs.juce.com/master/classjuce_1_1AudioSource.html`
- **API reference:** `juce::AudioDeviceManager` — `https://docs.juce.com/master/classjuce_1_1AudioDeviceManager.html`
- **Source to read:** `JUCE/examples/CMake/AudioApp/` — minimal runnable example
- **Source to read:** `JUCE/examples/CMake/Audio/` — more involved examples

### Demo: live sine wave through your interface

Build an app that:
1. On startup, lists available audio devices and selects your external interface (or let the user pick)
2. Uses `AudioAppComponent` (or `AudioSourcePlayer` + `AudioSource`)
3. In `getNextAudioBlock`, generates a sine wave
4. Exposes a `juce::Slider` for frequency

A minimal `Main.cpp` skeleton:

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

class MainComponent : public juce::AudioAppComponent
{
public:
    MainComponent()
    {
        addAndMakeVisible(frequencySlider);
        frequencySlider.setRange(20.0, 5000.0, 1.0);
        frequencySlider.setValue(440.0);
        frequencySlider.onValueChange = [this]
        {
            sineSource.frequencyHz.store((float)frequencySlider.getValue());
        };

        setAudioChannels(0, 2);
    }

    ~MainComponent() override { shutdownAudio(); }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        sineSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        sineSource.getNextAudioBlock(bufferToFill);
    }

    void releaseResources() override
    {
        sineSource.releaseResources();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);
    }

    void resized() override
    {
        frequencySlider.setBounds(20, 20, 300, 40);
    }

private:
    SineAudioSource sineSource;
    juce::Slider frequencySlider;
};
```

Build, run, confirm the tone comes out of your interface. Drag the slider — frequency changes.

### Stretch

- Add a second slider for **amplitude** (with `juce::SmoothedValue<float>` to avoid clicks when changing it)
- Add a button to switch the waveform between sine, square, sawtooth, and triangle (the math for each is one line)
- Print the selected device name and current buffer size in the GUI (`getCurrentAudioDevice()->getName()`, `getCurrentAudioDevice()->getCurrentBufferSizeSamples()`)
- Force ASIO selection at startup (use `getCurrentDeviceTypeObject()` and iterate devices)
- Add a level meter using `juce::AudioDeviceManager::LevelMeter` and a `juce::Component::repaint()`

### Checkpoint

> **You hear a tone through your interface, and you can change its frequency with a GUI slider while it plays. This is the foundation of every audio app you'll ever build.**

### Time estimate

5-8 hours. Most of it is troubleshooting build issues and reading JUCE source to understand `AudioSourceChannelInfo`.

### Common failures and fixes

| Symptom | Cause | Fix |
|---------|-------|-----|
| App builds but no sound | Wrong output device selected | Open the audio settings panel, manually pick your interface |
| ASIO device not listed | ASIO SDK headers missing in your build | Use `JUCE_ASIO` CMake flag; JUCE ships ASIO support but may need explicit opt-in |
| Crackling / glitches | Buffer size too small for your CPU | Increase from 64 to 128 or 256 |
| High latency | Buffer too large, or wrong mode | Use ASIO, set buffer size to 128 or 256 in the device's control panel |
| Slider movement is glitchy | Reading slider value from audio thread (thread-safety issue) | Use `std::atomic<float>` as shown above |
| Click on startup / shutdown | Phase not zero, or sudden amplitude change | Initialize `phase = 0` in `prepareToPlay`; ramp gain with `SmoothedValue` |
| MIDI keyboard "sends" but no sound | This is Week 4 material, not Week 3 | Confirm you have a sine source — not a MIDI listener — in `getNextAudioBlock` |

### Reading order for this week

1. Skim the JUCE tutorial "Hello world audio app" (15 min)
2. Read the `AudioAppComponent` class reference top-to-bottom (20 min)
3. Read the `AudioSource` class reference — focus on `getNextAudioBlock` and the `AudioSourceChannelInfo` struct (20 min)
4. Open `JUCE/examples/CMake/AudioApp/Main.cpp` and read it line-by-line (30 min)
5. Write the demo above from scratch (don't just paste) — type each line, hit build often (2-3 hours)
6. Do one stretch task (1-2 hours)

### What you can build after this week

- A standalone tone generator
- A test-signal app for checking your interface's outputs
- The audio engine foundation for any synth, effect, or DAW

### What you cannot build yet (and that's fine)

- Anything that responds to MIDI input (Week 4)
- Anything with effects chains (Week 5)
- Anything that loads VST3 plugins (Week 6)
- Anything that loads or plays audio files (later)

---

## Where you stand after Week 3

You have the single most important skill for a DAW project: the audio callback is no longer mysterious. Every later feature — MIDI playback, plugin hosting, audio file streaming — slots into the same `getNextAudioBlock` shape. The DAW project (`simple-daw`) becomes "what data do I fill this buffer with, and where does it come from" rather than "how does audio even work."
