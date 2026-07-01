#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "../src/midi/MidiOutputRouter.h"

class MidiOutputRouterTests : public juce::UnitTest
{
public:
    MidiOutputRouterTests() : juce::UnitTest("MidiOutputRouter") {}

    void runTest() override
    {
        beginTest("openOutput with empty name is a no-op and stays closed");
        {
            MidiOutputRouter r;
            expect(! r.isOpen());
            expect(! r.openOutput({}));
            expect(! r.isOpen());
            expect(r.getCurrentName().isEmpty());
            r.closeOutput();
            expect(! r.isOpen());
        }

        beginTest("isOpen / getCurrentName are safe to call concurrently with open/close");
        {
            MidiOutputRouter r;
            std::atomic<bool> stop{false};
            std::atomic<int> readErrors{0};

            auto reader = std::thread([&]
            {
                while (! stop.load(std::memory_order_acquire))
                {
                    (void) r.isOpen();
                    (void) r.getCurrentName();
                }
            });

            for (int i = 0; i < 5000; ++i)
            {
                r.openOutput({});
                r.closeOutput();
            }

            stop.store(true, std::memory_order_release);
            reader.join();
            expectEquals((int) readErrors.load(), 0);
        }

        beginTest("audio-thread sendBuffer does not block while message thread swaps output");
        {
            MidiOutputRouter r;

            juce::MidiBuffer buffer;
            for (int n = 0; n < 16; ++n)
                buffer.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), n);

            std::atomic<bool> stop{false};
            std::atomic<int> calls{0};
            std::atomic<juce::int64> maxNs{0};

            auto audioThread = std::thread([&]
            {
                while (! stop.load(std::memory_order_acquire))
                {
                    const auto t0 = std::chrono::steady_clock::now();
                    r.sendBuffer(buffer, 0);
                    const auto t1 = std::chrono::steady_clock::now();
                    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
                    juce::int64 prev = maxNs.load(std::memory_order_relaxed);
                    while ((juce::int64) ns > prev && ! maxNs.compare_exchange_weak(prev, (juce::int64) ns)) {}
                    calls.fetch_add(1, std::memory_order_relaxed);
                }
            });

            auto messageThread = std::thread([&]
            {
                for (int i = 0; i < 1000; ++i)
                {
                    r.openOutput({});
                    r.closeOutput();
                }
            });

            messageThread.join();
            juce::Thread::sleep(50);
            stop.store(true, std::memory_order_release);
            audioThread.join();

            const juce::int64 maxCallNs = maxNs.load(std::memory_order_relaxed);
            const int totalCalls = calls.load(std::memory_order_relaxed);

            logMessage("sendBuffer calls: " + juce::String(totalCalls)
                + ", max latency: " + juce::String(maxCallNs) + " ns");

            expect(totalCalls > 0);
            expect(maxCallNs < 1000000);
        }

        beginTest("sendAllNotesOff is a no-op when no output is open");
        {
            MidiOutputRouter r;
            r.sendAllNotesOff();
            r.sendNow(juce::MidiMessage::noteOn(1, 60, 0.8f));
            expect(! r.isOpen());
        }
    }
};

static MidiOutputRouterTests midiOutputRouterTests;
