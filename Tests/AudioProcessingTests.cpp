// Headless sample-rate / buffer-size sweep for RefraktProcessor — no DAW,
// no editor, just direct processBlock calls. Exercises the STFT engine
// (prepareToPlay's FFT sizing via pickFftOrder, the FIFO/overlap-add path
// in processBlock) across every sample rate pickFftOrder branches on and a
// wide spread of host buffer sizes, checking for NaN/Inf output and correct
// reported latency at each combination.
#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <cmath>
#include <random>
#include <chrono>
#include <thread>
#include <vector>

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { \
    ++checks; \
    if (!(cond)) { ++failures; std::printf ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::abs((double)(a) - (double)(b)) < (eps))

static bool allFinite (const juce::AudioBuffer<float>& buf)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        const float* d = buf.getReadPointer (ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
            if (! std::isfinite (d[i]))
                return false;
    }
    return true;
}

static void runSweep (double sampleRate, int blockSize, int expectedFftSize)
{
    RefraktProcessor proc;
    proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    proc.prepareToPlay (sampleRate, blockSize);

    CHECK (proc.getLatencySamples() == expectedFftSize);

    std::mt19937 rng (12345);
    std::uniform_real_distribution<float> dist (-0.5f, 0.5f);
    juce::MidiBuffer midi;

    // Run enough blocks to cover several full FFT windows so the FIFO /
    // overlap-add path exercises real steady-state, not just the cold start.
    const int totalSamples = expectedFftSize * 8;
    int samplesRun = 0;
    bool sawFiniteFailure = false;
    while (samplesRun < totalSamples)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < blockSize; ++i)
                d[i] = dist (rng);
        }
        proc.processBlock (buffer, midi);
        if (! allFinite (buffer))
            sawFiniteFailure = true;
        samplesRun += blockSize;
    }
    CHECK (! sawFiniteFailure);
}

static void runSilenceCheck (double sampleRate, int blockSize, int expectedFftSize)
{
    // Feeding pure silence must stay pure silence out (no self-oscillation,
    // no denormal blow-up) once the FFT pipeline has filled.
    RefraktProcessor proc;
    proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    proc.prepareToPlay (sampleRate, blockSize);
    juce::MidiBuffer midi;

    const int totalSamples = expectedFftSize * 8;
    int samplesRun = 0;
    float maxAbsAfterFill = 0.0f;
    while (samplesRun < totalSamples)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        proc.processBlock (buffer, midi);
        if (samplesRun > expectedFftSize * 2) // past the initial fill/latency period
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    maxAbsAfterFill = juce::jmax (maxAbsAfterFill, std::abs (buffer.getReadPointer (ch)[i]));
        samplesRun += blockSize;
    }
    CHECK (maxAbsAfterFill < 1.0e-6f);
}

static void runMonoInputCheck (double sampleRate, int blockSize, int expectedFftSize)
{
    // Mono-in/stereo-out is explicitly declared supported by
    // isBusesLayoutSupported (see PluginProcessor.cpp) — negotiate the same
    // bus layout a real host would, not just a raw setPlayConfigDetails,
    // so this actually exercises the isBusesLayoutSupported acceptance path.
    RefraktProcessor proc;

    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::mono());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    CHECK (proc.checkBusesLayoutSupported (layout));
    CHECK (proc.setBusesLayout (layout));
    CHECK (proc.getTotalNumInputChannels() == 1);
    CHECK (proc.getTotalNumOutputChannels() == 2);

    proc.setPlayConfigDetails (1, 2, sampleRate, blockSize);
    proc.prepareToPlay (sampleRate, blockSize);

    std::mt19937 rng (777);
    std::uniform_real_distribution<float> dist (-0.5f, 0.5f);
    juce::MidiBuffer midi;

    const int totalSamples = expectedFftSize * 8;
    int samplesRun = 0;
    bool sawFiniteFailure = false;
    bool sawNonSilentOutput = false;
    bool sawChannelMismatch = false;
    while (samplesRun < totalSamples)
    {
        // Host allocates a buffer sized for the larger side (2 channels here)
        // even though only channel 0 carries real input — channel 1's
        // pre-existing content must never leak into the output since
        // processBlock only ever reads inR via getReadPointer(0) when
        // numInCh==1 (see PluginProcessor.cpp). Poison channel 1 with
        // garbage to prove that.
        juce::AudioBuffer<float> buffer (2, blockSize);
        auto* ch0 = buffer.getWritePointer (0);
        for (int i = 0; i < blockSize; ++i) ch0[i] = dist (rng);
        auto* ch1 = buffer.getWritePointer (1);
        for (int i = 0; i < blockSize; ++i) ch1[i] = 999.0f; // never a valid sample value if it leaked through

        proc.processBlock (buffer, midi);

        if (! allFinite (buffer))
            sawFiniteFailure = true;

        const auto* outL = buffer.getReadPointer (0);
        const auto* outR = buffer.getReadPointer (1);
        for (int i = 0; i < blockSize; ++i)
        {
            if (std::abs (outL[i]) > 1.0e-6f) sawNonSilentOutput = true;
            // No side/pan content should exist for a mono source at default
            // (flat, Static mode, Tilt=0) settings — mid=inL exactly, side
            // stays 0, so outL must equal outR exactly, not approximately.
            if (std::abs (outL[i] - outR[i]) > 1.0e-5f) sawChannelMismatch = true;
        }
        samplesRun += blockSize;
    }
    CHECK (! sawFiniteFailure);
    CHECK (sawNonSilentOutput); // proves the mono content actually reached the output, not silently dropped
    CHECK (! sawChannelMismatch);
}

// --- DAW project save/reload round-trip -----------------------------------
// getStateInformation/setStateInformation is what a DAW calls on project
// save/reload — never exercised end-to-end before, despite Custom-shape
// nodes/zones being stashed outside APVTS specifically to survive it (see
// the comment on getStateInformation in PluginProcessor.cpp).

static void setParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float rawValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (rawValue));
}

static float getParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    return *apvts.getRawParameterValue (id);
}

static void runStateRoundTripTest()
{
    RefraktProcessor a;
    setParam (a.apvts, "tilt_amount", 0.73f);
    setParam (a.apvts, "curve_shape", -0.4f);
    setParam (a.apvts, "pivot_freq", 1234.0f);
    setParam (a.apvts, "pivot_boost", 0.6f);
    setParam (a.apvts, "lfo_rate", 4.2f);
    setParam (a.apvts, "lfo_intensity", 0.85f);
    setParam (a.apvts, "wave_cycles", 9.0f);
    setParam (a.apvts, "output_gain", -6.0f);
    setParam (a.apvts, "mix", 0.65f);
    setParam (a.apvts, "balance", 0.3f);
    setParam (a.apvts, "inverted", 1.0f);
    setParam (a.apvts, "safe_bass_on", 1.0f);
    setParam (a.apvts, "safe_bass_freq", 88.0f);
    setParam (a.apvts, "mod_mode", 3.0f);   // Tilt+Wave
    setParam (a.apvts, "lfo_shape", 5.0f);  // Custom
    setParam (a.apvts, "tempo_sync", 1.0f);
    setParam (a.apvts, "sync_division", 1.0f);

    const std::vector<RefraktProcessor::CustomNode> nodes = {
        { 0.1f, 0.5f, 3.0f }, { 0.5f, -0.7f, 6.0f }, { 0.9f, 0.2f, 1.5f }
    };
    const std::vector<RefraktProcessor::ProtectZone> zones = {
        { 0.2f, 0.3f }, { 0.6f, 0.65f }
    };
    a.setCustomShapeState (nodes, zones);

    juce::MemoryBlock saved;
    a.getStateInformation (saved);

    RefraktProcessor b;
    // Pre-populate B with different state to prove restore actually
    // overwrites it, rather than merging with or leaving it untouched.
    b.setCustomShapeState ({ { 0.99f, 0.99f, 9.0f } }, { { 0.01f, 0.02f } });
    setParam (b.apvts, "tilt_amount", -0.99f);

    b.setStateInformation (saved.getData(), (int) saved.getSize());

    CHECK_NEAR (getParam (b.apvts, "tilt_amount"), 0.73f, 1e-4f);
    CHECK_NEAR (getParam (b.apvts, "curve_shape"), -0.4f, 1e-4f);
    CHECK_NEAR (getParam (b.apvts, "pivot_freq"), 1234.0f, 0.5f); // skewed range, coarser quantisation
    CHECK_NEAR (getParam (b.apvts, "pivot_boost"), 0.6f, 1e-3f);
    CHECK_NEAR (getParam (b.apvts, "lfo_rate"), 4.2f, 1e-3f);
    CHECK_NEAR (getParam (b.apvts, "lfo_intensity"), 0.85f, 1e-3f);
    CHECK_NEAR (getParam (b.apvts, "wave_cycles"), 9.0f, 1e-4f);
    CHECK_NEAR (getParam (b.apvts, "output_gain"), -6.0f, 1e-3f);
    CHECK_NEAR (getParam (b.apvts, "mix"), 0.65f, 1e-3f);
    CHECK_NEAR (getParam (b.apvts, "balance"), 0.3f, 1e-3f);
    CHECK (getParam (b.apvts, "inverted") > 0.5f);
    CHECK (getParam (b.apvts, "safe_bass_on") > 0.5f);
    CHECK_NEAR (getParam (b.apvts, "safe_bass_freq"), 88.0f, 1e-3f);
    CHECK_NEAR (getParam (b.apvts, "mod_mode"), 3.0f, 1e-4f);
    CHECK_NEAR (getParam (b.apvts, "lfo_shape"), 5.0f, 1e-4f);
    CHECK (getParam (b.apvts, "tempo_sync") > 0.5f);
    CHECK_NEAR (getParam (b.apvts, "sync_division"), 1.0f, 1e-4f);

    std::vector<RefraktProcessor::CustomNode> nodesOut;
    std::vector<RefraktProcessor::ProtectZone> zonesOut;
    b.getCustomShapeStateForDisplay (nodesOut, zonesOut);
    CHECK (nodesOut.size() == nodes.size());
    for (size_t i = 0; i < nodes.size() && i < nodesOut.size(); ++i)
    {
        CHECK_NEAR (nodesOut[i].freq, nodes[i].freq, 1e-4f);
        CHECK_NEAR (nodesOut[i].pan, nodes[i].pan, 1e-4f);
        CHECK_NEAR (nodesOut[i].q, nodes[i].q, 1e-4f);
    }
    CHECK (zonesOut.size() == zones.size());
    for (size_t i = 0; i < zones.size() && i < zonesOut.size(); ++i)
    {
        CHECK_NEAR (zonesOut[i].lo, zones[i].lo, 1e-4f);
        CHECK_NEAR (zonesOut[i].hi, zones[i].hi, 1e-4f);
    }
}

static void runEmptyCustomShapeClearsPriorState()
{
    RefraktProcessor a; // never touched — no nodes/zones set
    juce::MemoryBlock saved;
    a.getStateInformation (saved);

    RefraktProcessor b;
    b.setCustomShapeState ({ { 0.5f, 0.5f, 3.0f } }, { { 0.1f, 0.2f } }); // pre-existing junk
    b.setStateInformation (saved.getData(), (int) saved.getSize());

    std::vector<RefraktProcessor::CustomNode> nodesOut;
    std::vector<RefraktProcessor::ProtectZone> zonesOut;
    b.getCustomShapeStateForDisplay (nodesOut, zonesOut);
    CHECK (nodesOut.empty());
    CHECK (zonesOut.empty());
}

static void runMaxCapacityRoundTrip()
{
    RefraktProcessor a;
    std::vector<RefraktProcessor::CustomNode> nodes;
    for (int i = 0; i < RefraktProcessor::kMaxCustomNodes; ++i)
        nodes.push_back ({ (float) i / (float) RefraktProcessor::kMaxCustomNodes,
                            (i % 2 == 0 ? 0.5f : -0.5f), 2.0f + (float) i });
    std::vector<RefraktProcessor::ProtectZone> zones;
    for (int i = 0; i < RefraktProcessor::kMaxProtectZones; ++i)
        zones.push_back ({ 0.1f * (float) i, 0.1f * (float) i + 0.05f });
    a.setCustomShapeState (nodes, zones);

    juce::MemoryBlock saved;
    a.getStateInformation (saved);

    RefraktProcessor b;
    b.setStateInformation (saved.getData(), (int) saved.getSize());

    std::vector<RefraktProcessor::CustomNode> nodesOut;
    std::vector<RefraktProcessor::ProtectZone> zonesOut;
    b.getCustomShapeStateForDisplay (nodesOut, zonesOut);
    CHECK (nodesOut.size() == (size_t) RefraktProcessor::kMaxCustomNodes);
    CHECK (zonesOut.size() == (size_t) RefraktProcessor::kMaxProtectZones);
}

static void runCorruptStateDoesNotCrash()
{
    RefraktProcessor b;
    b.setCustomShapeState ({ { 0.5f, 0.5f, 3.0f } }, {});
    setParam (b.apvts, "tilt_amount", 0.42f);

    std::vector<char> garbage (64);
    for (size_t i = 0; i < garbage.size(); ++i)
        garbage[i] = (char) (i * 37 + 3);
    b.setStateInformation (garbage.data(), (int) garbage.size()); // must not crash

    // Unrecognised data is safely ignored (see the null/tag-mismatch guard
    // at the top of setStateInformation) — prior state must survive untouched.
    CHECK_NEAR (getParam (b.apvts, "tilt_amount"), 0.42f, 1e-4f);
    std::vector<RefraktProcessor::CustomNode> nodesOut;
    std::vector<RefraktProcessor::ProtectZone> zonesOut;
    b.getCustomShapeStateForDisplay (nodesOut, zonesOut);
    CHECK (nodesOut.size() == 1);
}

// --- Automation zipper-noise check -----------------------------------
// output_gain/mix are read once per BLOCK and applied per-sample with no
// smoothing prior to this test — a host automating either fast enough to
// change between blocks produced a hard step at the block boundary
// (see PluginProcessor.h's comment on gainSmoothed/mixSmoothed for the fix).
// This proves the ramp actually holds under a worst-case instantaneous jump.

static void runGainAutomationZipperCheck()
{
    // Twin processors fed bit-identical input, one held at a fixed 0dB
    // reference and one that jumps to +24dB partway through — dividing the
    // jumped processor's output by the reference's, sample for sample,
    // recovers the EFFECTIVE gain envelope actually applied at each sample,
    // independent of the sine's own instantaneous derivative (a raw
    // waveform-delta metric conflates "gain changed" with "signal moved,"
    // since a louder signal naturally has a bigger per-sample swing too).
    const double sampleRate = 44100.0;
    const int blockSize = 64; // small block = frequent boundaries = worst case
    const int fftSize = 2048;

    RefraktProcessor baseline, jumped;
    for (auto* p : { &baseline, &jumped })
    {
        p->setPlayConfigDetails (2, 2, sampleRate, blockSize);
        p->prepareToPlay (sampleRate, blockSize);
        setParam (p->apvts, "output_gain", 0.0f);
        setParam (p->apvts, "mix", 1.0f);
    }

    juce::MidiBuffer midi;
    const double freq = 1000.0;
    const double phaseInc = juce::MathConstants<double>::twoPi * freq / sampleRate;

    auto makeInput = [&] (double startPhase)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            double p = startPhase;
            for (int i = 0; i < blockSize; ++i) { d[i] = 0.2f * (float) std::sin (p); p += phaseInc; }
        }
        return buffer;
    };

    double runPhase = 0.0;
    auto advance = [&] () { runPhase = std::fmod (runPhase + phaseInc * blockSize, juce::MathConstants<double>::twoPi); };

    // Reach steady state (past the FFT's initial fill/latency window) on
    // both processors identically before doing anything else.
    const int fillBlocks = (fftSize * 4) / blockSize;
    for (int b = 0; b < fillBlocks; ++b)
    {
        auto bufA = makeInput (runPhase);
        auto bufB = makeInput (runPhase);
        baseline.processBlock (bufA, midi);
        jumped.processBlock (bufB, midi);
        advance();
    }

    // Worst-case instantaneous jump: 0dB -> +24dB (~15.85x), set right
    // before the next block — exactly what a host compressing a fast
    // automation ramp into one block boundary would produce. Baseline
    // never changes, so it stays the reference for "no gain change" output.
    setParam (jumped.apvts, "output_gain", 24.0f);

    // Seeded with a known-valid ratio (both processors identical pre-jump,
    // so 1.0 is exact) and haveRatio already true — otherwise the very
    // FIRST post-jump sample, which is exactly where an instant snap would
    // land, never gets compared against anything and the test can't see it.
    float prevRatio = 1.0f;
    bool haveRatio = true;
    float maxRatioStep = 0.0f;
    float finalRatio = 1.0f;
    for (int b = 0; b < 40; ++b) // several blocks: covers the whole 20ms ramp at this block size
    {
        auto bufA = makeInput (runPhase);
        auto bufB = makeInput (runPhase);
        baseline.processBlock (bufA, midi);
        jumped.processBlock (bufB, midi);
        advance();

        const auto* a = bufA.getReadPointer (0);
        const auto* bOut = bufB.getReadPointer (0);
        for (int i = 0; i < blockSize; ++i)
        {
            if (std::abs (a[i]) > 0.02f) // skip near-zero-crossing samples, division gets noisy there
            {
                const float ratio = bOut[i] / a[i];
                if (haveRatio)
                    maxRatioStep = juce::jmax (maxRatioStep, std::abs (ratio - prevRatio));
                prevRatio = ratio;
                finalRatio = ratio;
                haveRatio = true;
            }
        }
    }

    // A hard block-boundary step (pre-fix) shows up here as the ratio
    // jumping from ~1x straight to ~15.85x between two adjacent qualifying
    // samples (step size ~14.85). The 20ms ramp instead spreads that same
    // change over ~882 samples (~0.017 per step) — nowhere close to 1.0.
    CHECK (maxRatioStep < 1.0f);
    CHECK (finalRatio > 10.0f); // and the ramp did complete: settled near the ~15.85x target
}

// --- CPU / performance profiling --------------------------------------
// Wall-clock cost of processBlock, expressed as a fraction of real-time
// (e.g. 0.03 = 3% of one CPU core to run one instance in real time).
// Printed, not asserted — CI runners vary wildly in raw speed and
// scheduling noise, so a hard pass/fail threshold on absolute time would
// just be flaky. The one thing that IS asserted: N instances running
// concurrently on separate threads shouldn't take dramatically longer per
// thread than a single instance alone — that would mean a hidden shared
// resource serializing instances that should be fully independent (each
// RefraktProcessor owns all its own state, no statics).

static double benchmarkOneInstance (double sampleRate, int blockSize, double audioDurationSeconds)
{
    RefraktProcessor proc;
    proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    proc.prepareToPlay (sampleRate, blockSize);

    std::mt19937 rng (42);
    std::uniform_real_distribution<float> dist (-0.5f, 0.5f);
    juce::MidiBuffer midi;

    const int totalSamples = (int) (audioDurationSeconds * sampleRate);
    juce::AudioBuffer<float> buffer (2, blockSize);

    const auto start = std::chrono::steady_clock::now();
    int samplesRun = 0;
    while (samplesRun < totalSamples)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < blockSize; ++i) d[i] = dist (rng);
        }
        proc.processBlock (buffer, midi);
        samplesRun += blockSize;
    }
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double> (end - start).count();
}

static void runCpuProfilingReport()
{
    struct Case { double sampleRate; int blockSize; };
    const Case cases[] = {
        { 44100.0, 512 },
        { 48000.0, 128 }, // smaller block = more per-block overhead relative to work
        { 96000.0, 512 }, // double-size FFT
    };

    std::printf ("\n--- CPU profiling (this machine, single-threaded) ---\n");
    for (const auto& c : cases)
    {
        const double audioSeconds = 5.0;
        const double wallSeconds = benchmarkOneInstance (c.sampleRate, c.blockSize, audioSeconds);
        const double realTimeRatio = wallSeconds / audioSeconds;
        const double instancesPerCore = realTimeRatio > 0.0 ? 1.0 / realTimeRatio : 0.0;
        std::printf ("  %.0fHz, block %d: %.2f%% of one core in real time (~%.0f instances/core headroom)\n",
                     c.sampleRate, c.blockSize, realTimeRatio * 100.0, instancesPerCore);
    }

    const int numThreads = 4;
    const double audioSeconds = 3.0;
    const double baselineWall = benchmarkOneInstance (44100.0, 512, audioSeconds);

    std::vector<double> threadWallTimes ((size_t) numThreads);
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t)
        threads.emplace_back ([&, t] () { threadWallTimes[(size_t) t] = benchmarkOneInstance (44100.0, 512, audioSeconds); });
    for (auto& th : threads) th.join();

    double maxParallelWall = 0.0;
    for (double w : threadWallTimes) maxParallelWall = juce::jmax (maxParallelWall, w);
    std::printf ("  Concurrent %d-instance check: baseline %.3fs, max-per-thread %.3fs\n",
                 numThreads, baselineWall, maxParallelWall);
    std::printf ("--- end CPU profiling ---\n\n");

    CHECK (maxParallelWall < baselineWall * 3.0);
}

// --- Solo real-audio isolation check -----------------------------------
// getSoloMask (CurveMathTests.cpp) already covers the pure math; this
// verifies it actually reaches the real audio path end-to-end. Twin
// processors, identical mono broadband noise (mono so the side channel is
// exactly zero and solo's effect isn't diluted by untouched side content —
// solo only ever touches the repositionable mid path, consistent with
// every other feature in this plugin) — one left alone, one with a narrow
// zone soloed. Soloing must visibly cut total output energy versus the
// unsoloed baseline on the identical input.
static void runSoloAudioIsolationCheck()
{
    const double sampleRate = 44100.0;
    const int blockSize = 512;
    const int fftSize = 2048;

    RefraktProcessor baseline, soloed;
    for (auto* p : { &baseline, &soloed })
    {
        p->setPlayConfigDetails (2, 2, sampleRate, blockSize);
        p->prepareToPlay (sampleRate, blockSize);
        setParam (p->apvts, "mix", 1.0f);
        setParam (p->apvts, "output_gain", 0.0f);
    }
    soloed.setCustomShapeState ({}, { { 0.55f, 0.65f } });
    soloed.setSoloTarget (0);

    std::mt19937 rng (99);
    std::uniform_real_distribution<float> dist (-0.4f, 0.4f);
    juce::MidiBuffer midi;

    double sumSqBaseline = 0.0, sumSqSoloed = 0.0;
    const int totalSamples = fftSize * 12;
    int samplesRun = 0;
    while (samplesRun < totalSamples)
    {
        juce::AudioBuffer<float> bufA (2, blockSize), bufB (2, blockSize);
        for (int i = 0; i < blockSize; ++i)
        {
            const float s = dist (rng); // identical L/R -> side channel is exactly zero
            bufA.setSample (0, i, s); bufA.setSample (1, i, s);
            bufB.setSample (0, i, s); bufB.setSample (1, i, s);
        }
        baseline.processBlock (bufA, midi);
        soloed.processBlock (bufB, midi);

        if (samplesRun > fftSize * 2) // past the FFT's initial fill/latency window
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const float a = bufA.getReadPointer (0)[i];
                const float b = bufB.getReadPointer (0)[i];
                sumSqBaseline += (double) a * a;
                sumSqSoloed += (double) b * b;
            }
        }
        samplesRun += blockSize;
    }

    CHECK (sumSqBaseline > 0.0); // sanity: baseline actually produced audible output
    CHECK (sumSqSoloed < sumSqBaseline * 0.5); // soloing a narrow zone must dramatically cut total energy
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    struct Case { double sampleRate; int expectedFft; };
    const Case cases[] = {
        { 44100.0, 2048 }, { 48000.0, 2048 },
        { 88200.0, 4096 }, { 96000.0, 4096 },
        { 176400.0, 8192 }, { 192000.0, 8192 },
    };
    const int blockSizes[] = { 1, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192 };

    for (const auto& c : cases)
        for (int bs : blockSizes)
            runSweep (c.sampleRate, bs, c.expectedFft);

    // Silence check at a representative subset (full sweep would be redundant).
    for (const auto& c : cases)
        runSilenceCheck (c.sampleRate, 512, c.expectedFft);

    // Mono-in/stereo-out across the same sample-rate spread and a
    // representative subset of block sizes.
    for (const auto& c : cases)
        for (int bs : { 64, 512, 4096 })
            runMonoInputCheck (c.sampleRate, bs, c.expectedFft);

    runStateRoundTripTest();
    runEmptyCustomShapeClearsPriorState();
    runMaxCapacityRoundTrip();
    runCorruptStateDoesNotCrash();

    runGainAutomationZipperCheck();

    runCpuProfilingReport();

    runSoloAudioIsolationCheck();

    std::printf ("%d/%d checks passed\n", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
