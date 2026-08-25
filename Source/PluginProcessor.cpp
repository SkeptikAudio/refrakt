#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "CurveMath.h"
#include <cmath>

using namespace RefraktCurveMath;

RefraktProcessor::RefraktProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    , apvts (*this, &undoManager, "STATE", createParameterLayout())
{
}

RefraktProcessor::~RefraktProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout
RefraktProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tilt_amount", "Tilt", -1.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("curve_shape", "Shape", -1.0f, 1.0f, 0.0f));
    // Where the tilt curve's zero-crossing sits. Log-scaled (skewed) like
    // Rate, since frequency perception is logarithmic — a linear Hz range
    // would bunch all the musically useful low/mid pivot points into a
    // sliver of the control's travel. Default 663Hz = sqrt(20*22000), the
    // exact geometric-mean centre the curve used before this parameter
    // existed, so nothing changes for any existing preset unless it's
    // touched. Range clamped to 60Hz-8kHz so the pivot can never collapse
    // one whole side of the tilt to zero span. Only meaningful in Tilt and
    // Tilt+Wave modes (mirrors the GUI gating).
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("pivot_freq", "Pivot", juce::NormalisableRange<float> (60.0f, 8000.0f, 0.0f, 0.3f), 663.0f));
    // warpX's midrange boost is strongest right at the pivot crossing (see
    // CurveMath.h) — dramatic, but can read as too extreme right around the
    // pivot, so it defaults OFF. 0 = flat/plain log-linear response
    // (k=1, no warp), 1 = full pivot-emphasis boost (k=0.12).
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("pivot_boost", "Boost", 0.0f, 1.0f, 0.0f));
    // 0.1Hz floor (not 0.3) so genuinely glacial Atmosphere-style motion is reachable.
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lfo_rate", "LFO Rate", juce::NormalisableRange<float> (0.1f, 10.0f, 0.0f, 0.3f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lfo_intensity", "LFO Intensity", 0.0f, 1.0f, 1.0f));
    // 1-16 to match the GUI's real range, and hard-integer stepped —
    // fractional cycles change the actual ripple pattern, not just the label.
    params.push_back (std::make_unique<juce::AudioParameterInt>   ("wave_cycles", "Wave Cycles", 1, 16, 2));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_gain", "Output Gain", -24.0f, 24.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix", 0.0f, 1.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("balance", "Tilt/Wave Balance", -1.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("inverted", "Invert", false));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("bypass", "Bypass", false));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("safe_bass_on", "Safe Bass", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("safe_bass_freq", "Safe Bass Freq", 40.0f, 400.0f, 150.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("mod_mode", "Modulation Mode", juce::StringArray { "Static", "Tilt", "Wave", "Tilt+Wave" }, 0));
    // "Custom" drives the node-based curve (see customNodes/setCustomShapeState
    // in the header) — an arbitrary list of (freq, pan, Q) nodes, which is
    // variable-length state that doesn't fit a flat APVTS parameter, so it
    // lives outside APVTS entirely and is only ever authored via preset
    // selection. Falls back to Sine if no nodes are set (see computeBinGains).
    // Spike removed earlier for being redundant with Pulse; Pulse itself
    // then removed too ("adds too little" per user feedback). Custom
    // shifted down to index 5 as a result.
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("lfo_shape", "LFO Shape", juce::StringArray { "Sine", "Triangle", "Saw", "Square", "Random", "Custom" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("tempo_sync", "Tempo Sync", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("sync_division", "Sync Division", juce::StringArray { "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bar" }, 2));
    return { params.begin(), params.end() };
}

int RefraktProcessor::pickFftOrder (double sampleRate)
{
    // Keep bin width (Hz) and latency (seconds) roughly constant as sample
    // rate rises, by doubling the window every time the rate doubles.
    if (sampleRate > 150000.0) return 13; // 8192 @ 176.4/192k
    if (sampleRate > 75000.0)  return 12; // 4096 @ 88.2/96k
    return 11;                            // 2048 @ 44.1/48k
}

void RefraktProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate;
    fftOrder = pickFftOrder (sampleRate);
    fftSize  = 1 << fftOrder;
    hopSize  = fftSize / 4; // 75% overlap — see header comment: required for squared-Hann COLA
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    window.resize ((size_t) fftSize);
    // Periodic Hann (divide by fftSize, NOT fftSize-1 — that's the symmetric
    // variant, meant for spectral analysis, not reconstruction), at 75%
    // overlap (hopSize = fftSize/4): with the window applied twice
    // (analysis AND synthesis), window-squared summed across overlapping
    // frames is a constant 1.5 at this hop fraction (verified numerically,
    // not assumed) — so the window is pre-scaled by 1/sqrt(1.5) here,
    // making window-squared sum to exactly 1.0 and guaranteeing a true
    // unity-gain passthrough when every bin gain is 1.
    const float colaCompensation = 1.0f / std::sqrt (1.5f);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = colaCompensation * 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi
                                                        * (float) i / (float) fftSize));

    inFifoM.assign ((size_t) fftSize, 0.0f);
    outFifoLFifo.assign ((size_t) hopSize, 0.0f);
    outFifoRFifo.assign ((size_t) hopSize, 0.0f);
    fftData.assign ((size_t) (2 * fftSize), 0.0f);
    fftDataL.assign ((size_t) (2 * fftSize), 0.0f);
    fftDataR.assign ((size_t) (2 * fftSize), 0.0f);
    overlapL.assign ((size_t) fftSize, 0.0f);
    overlapR.assign ((size_t) fftSize, 0.0f);

    sDelayLine.assign ((size_t) fftSize, 0.0f);
    dryDelayL.assign ((size_t) fftSize, 0.0f);
    dryDelayR.assign ((size_t) fftSize, 0.0f);
    delayWritePos = 0;

    const int numBins = fftSize / 2 + 1;
    // Pre-sized once here so the audio thread never allocates: computeBinGains
    // just fills these in place every hop.
    binGainL.assign ((size_t) numBins, 1.0f);
    binGainR.assign ((size_t) numBins, 1.0f);

    // Bin -> log-frequency position is fixed for the lifetime of this
    // sample rate / FFT size — computed once instead of once per bin,
    // every single hop.
    binFrac.assign ((size_t) numBins, 0.0);
    for (int bin = 0; bin < numBins; ++bin)
    {
        const double freqHz = (double) bin * sampleRate / (double) fftSize;
        binFrac[(size_t) bin] = hzToFrac (freqHz);
    }

    fifoIndex = 0;
    hopCounter = 0;
    samplePosition = 0;
    lfoPhaseAccum = 0.0;
    cycleScrollAccum = 0.0;

    // 20ms ramp — long enough to kill block-boundary zipper on fast
    // automation, short enough to stay inaudible as a fade on a deliberate
    // knob move. Snap to whatever's currently set, not 0, so prepareToPlay
    // (e.g. a sample-rate change mid-session) never causes its own fade-in.
    gainSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    const float initialGainDb = *apvts.getRawParameterValue ("output_gain");
    const float initialMix = *apvts.getRawParameterValue ("mix");
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (initialGainDb));
    mixSmoothed.setCurrentAndTargetValue (initialMix);

    // Total round-trip latency of an overlap-add STFT is exactly one
    // analysis window. Reported via the standard host API so every DAW
    // applies plugin-delay-compensation automatically — nothing for the
    // user to configure.
    setLatencySamples (fftSize);
}

void RefraktProcessor::releaseResources() {}

bool RefraktProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    // Stereo-in/stereo-out is the main case; mono-in/stereo-out is explicitly
    // supported too (panning a mono source is a legitimate, common use —
    // it can only add width, and Safe Bass / zones already guard the mono-fold risk).
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono()) return false;
    return true;
}

void RefraktProcessor::computeBinGains (juce::int64 frameStartSample)
{
    const int numBins = fftSize / 2 + 1;
    const double sr = currentSampleRate;

    const float tilt      = *apvts.getRawParameterValue ("tilt_amount");
    const float shapeKnob = *apvts.getRawParameterValue ("curve_shape");
    const float pivotHz   = *apvts.getRawParameterValue ("pivot_freq");
    const float rateFree  = *apvts.getRawParameterValue ("lfo_rate");
    const float intensity = *apvts.getRawParameterValue ("lfo_intensity");
    const int   cycles    = (int) *apvts.getRawParameterValue ("wave_cycles");
    const float balance   = *apvts.getRawParameterValue ("balance");
    const bool  inverted  = *apvts.getRawParameterValue ("inverted") > 0.5f;
    const int   mode      = (int) *apvts.getRawParameterValue ("mod_mode"); // 0 static 1 tilt 2 wave 3 both
    const int   shapeIdx  = (int) *apvts.getRawParameterValue ("lfo_shape");
    const bool  syncOn    = *apvts.getRawParameterValue ("tempo_sync") > 0.5f;
    const int   divIdx    = (int) *apvts.getRawParameterValue ("sync_division");
    const bool  safeBassOn   = *apvts.getRawParameterValue ("safe_bass_on") > 0.5f;
    const float safeBassHz   = *apvts.getRawParameterValue ("safe_bass_freq");
    const double safeBassFrac = hzToFrac ((double) safeBassHz);
    const double warpK = boostToK ((double) *apvts.getRawParameterValue ("pivot_boost"));

    // Tiny fixed-size copy, once per hop — see header comment on customShapeLock.
    std::array<CustomNode, kMaxCustomNodes> nodesLocal {};
    std::array<ProtectZone, kMaxProtectZones> zonesLocal {};
    int numNodesLocal = 0, numZonesLocal = 0;
    {
        const juce::SpinLock::ScopedLockType sl (customShapeLock);
        nodesLocal = customNodes;
        numNodesLocal = numCustomNodes;
        zonesLocal = protectZones;
        numZonesLocal = numProtectZones;
    }

    // Pivot is a live, automatable parameter, so — unlike binFrac — its
    // frac position can't be precomputed once at prepareToPlay time. Cheap
    // either way: one hzToFrac call per hop, not per bin.
    const double pivotFrac = hzToFrac ((double) pivotHz);

    // Host tempo (act as 120 BPM whenever a host provides none — the common,
    // safe default rather than silently breaking free-run behaviour).
    double bpm = 120.0;
    if (auto* ph = getPlayHead()) // already const-qualified in JUCE, no cast needed
    {
        if (auto pos = ph->getPosition())
            if (auto bpmOpt = pos->getBpm())
                bpm = *bpmOpt;
    }
    static const double divMult[6] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
    const double rateHz = syncOn ? (bpm / 60.0) * divMult[juce::jlimit (0, 5, divIdx)] : (double) rateFree;

    // Phase accumulator, not phase=rate*elapsedTime — see header comment on
    // lfoPhaseAccum. Advance using THIS hop's rate before reading it, so a
    // Rate change takes effect smoothly from here onward rather than
    // retroactively recomputing where phase "should" be.
    const double hopDurationSeconds = (double) hopSize / sr;
    lfoPhaseAccum = std::fmod (lfoPhaseAccum + juce::MathConstants<double>::twoPi * rateHz * hopDurationSeconds,
                                juce::MathConstants<double>::twoPi);
    cycleScrollAccum = std::fmod (cycleScrollAccum + rateHz * hopDurationSeconds, 2.0);
    const double lfoPhase = lfoPhaseAccum;
    uiLfoPhase.store (lfoPhaseAccum, std::memory_order_relaxed);

    // Still tied to absolute sample position (not the phase accumulator
    // above) deliberately — Random shape's determinism policy requires it
    // to key off real elapsed sample time so a bounce reproduces exactly.
    const double elapsedSeconds = (double) frameStartSample / sr;

    const double tiltW = (1.0 - balance) / 2.0;
    const double waveW = (1.0 + balance) / 2.0;

    // Custom shape (index 5) is only "live" when there's actually node data
    // to drive it — an empty node list falls back to Sine rather than
    // silently producing a flat, motionless curve.
    const bool isCustom = (shapeIdx == 5) && (numNodesLocal > 0);
    const int effectiveShape = (shapeIdx >= 5) ? 0 : shapeIdx;

    for (int bin = 0; bin < numBins; ++bin)
    {
        const double freqHz = (double) bin * sr / (double) fftSize;
        const double frac = binFrac[(size_t) bin]; // precomputed in prepareToPlay — no per-hop log10
        const double x = warpX (pivotX (frac, pivotFrac), warpK); // pivot-dependent, so computed fresh each hop
        const double tiltShape = tiltShapeAt (tilt, shapeKnob, x);
        // Nodes contribute a static per-bin pan bias in every mode (matches
        // the mockup's buildStaticCurve, which folds nodeInfluence into the
        // base curve unconditionally) — a Custom preset still colours the
        // spectrum even in Static mode where nothing else is time-varying.
        const double base = tiltShape + nodeInfluence (frac, nodesLocal, numNodesLocal, inverted);

        double pan = base;
        if (mode == 1) // Tilt — always driven by Sine by design (matches GUI: shape pill is disabled in Tilt mode)
        {
            pan = isCustom
                ? base * lerp (1.0, std::sin (lfoPhase), intensity)
                : base + lfoWaveform (lfoPhase, 0, inverted, elapsedSeconds) * intensity * x;
        }
        else if (mode == 2) // Wave
        {
            if (isCustom)
            {
                const double cyclePos = pingPongPos (frac * cycles - cycleScrollAccum);
                pan = tiltShape + nodeInfluence (cyclePos, nodesLocal, numNodesLocal, inverted) * intensity;
            }
            else
            {
                const double phase = frac * cycles * juce::MathConstants<double>::twoPi - lfoPhase;
                pan = base + lfoWaveform (phase, effectiveShape, inverted, elapsedSeconds) * intensity;
            }
        }
        else if (mode == 3) // Tilt + Wave
        {
            if (isCustom)
            {
                const double cyclePos = pingPongPos (frac * cycles - cycleScrollAccum);
                const double mirrored = base * lerp (1.0, std::sin (lfoPhase), intensity);
                const double looped = tiltShape + nodeInfluence (cyclePos, nodesLocal, numNodesLocal, inverted) * intensity;
                pan = mirrored * tiltW + looped * waveW;
            }
            else
            {
                const double phaseW = frac * cycles * juce::MathConstants<double>::twoPi - lfoPhase;
                pan = base
                    + lfoWaveform (lfoPhase, 0, inverted, elapsedSeconds) * intensity * tiltW * x
                    + lfoWaveform (phaseW, effectiveShape, inverted, elapsedSeconds) * intensity * waveW;
            }
        }
        // mode == 0 (Static): pan stays as the plain tilt/shape+node curve, no time element.

        pan = juce::jlimit (-1.0, 1.0, pan);

        // Safe Bass / protect zones push panned content back toward centre
        // rather than participating in the curve itself — applied after
        // clamping, exactly like the mockup's getFinalCurve, so protection
        // can only ever shrink |pan| toward 0, never re-expand it past ±1.
        const double prot = getProtection (frac, safeBassOn, safeBassFrac, zonesLocal, numZonesLocal);
        pan *= (1.0 - prot);

        // DC and near-DC bins are forced dead centre regardless of the
        // curve: splitting near-zero-frequency content asymmetrically
        // risks a DC-offset / mono-fold artifact for no audible benefit.
        if (freqHz < 25.0) pan = 0.0;

        // Constant-power pan law, normalised for UNITY gain at pan==0 (not
        // the classic -3dB-at-centre convention) — this tool repositions
        // content that already exists in the mix, it doesn't spread a
        // fresh mono source, so "no pan" must be a true identity pass or
        // the Static/Transparent case would quietly lose level for no reason.
        const double theta = (pan + 1.0) * (juce::MathConstants<double>::pi / 4.0);
        const double sqrt2 = juce::MathConstants<double>::sqrt2;
        binGainL[(size_t) bin] = (float) (std::cos (theta) * sqrt2);
        binGainR[(size_t) bin] = (float) (std::sin (theta) * sqrt2);
    }
}

void RefraktProcessor::processFFTFrame()
{
    computeBinGains (samplePosition - fftSize); // fills binGainL/binGainR in place — no per-hop allocation

    // Analyse the mid signal once. inFifoM is a circular buffer; fifoIndex
    // (already advanced past the sample just written, by the time this is
    // called) points at the oldest still-valid sample, i.e. the one about
    // to be overwritten next — so unwrapping from there gives the last
    // fftSize samples in correct chronological order.
    for (int i = 0; i < fftSize; ++i)
    {
        fftData[(size_t) i] = inFifoM[(size_t) ((fifoIndex + i) % fftSize)] * window[(size_t) i];
        fftData[(size_t) (i + fftSize)] = 0.0f;
    }
    fft->performRealOnlyForwardTransform (fftData.data(), true);

    // Real input spectrum for the UI's curve display — magnitude per FFT
    // bin, resampled onto the same log-frequency buckets the curve itself
    // uses (a bin's frac position via hzToFrac maps straight to a bucket).
    // RMS-AVERAGED (not summed) across every FFT bin that lands in each
    // bucket: higher buckets cover many linearly-spaced FFT bins each
    // (since the axis is log) while low buckets cover only one or two, so
    // summing (as this used to do) made bucket loudness scale with how
    // many bins happened to land there rather than actual signal level —
    // an unintended upward tilt toward the highs that quietly crushed bass
    // by comparison, on top of whichever SPECTRUM_SLOPE_DB_PER_OCT the UI
    // asks for. Averaging (sum of squares / bin count) reports the actual
    // level density in the band regardless of how many bins compose it.
    {
        const int uiNumBins = fftSize / 2 + 1;
        std::array<float, kUiSpectrumBins> bucketEnergy {};
        std::array<int, kUiSpectrumBins> bucketCount {};
        for (int bin = 1; bin < uiNumBins; ++bin) // skip DC
        {
            const float re = fftData[(size_t) (2 * bin)], im = fftData[(size_t) (2 * bin + 1)];
            const float mag = std::sqrt (re * re + im * im);
            const double freqHz = (double) bin * currentSampleRate / (double) fftSize;
            const double frac = hzToFrac (freqHz);
            const int bucket = juce::jlimit (0, kUiSpectrumBins - 1, (int) (frac * (double) (kUiSpectrumBins - 1)));
            bucketEnergy[(size_t) bucket] += mag * mag;
            bucketCount[(size_t) bucket] += 1;
        }
        std::array<float, kUiSpectrumBins> bucketAvg {};
        std::array<bool, kUiSpectrumBins> hasData {};
        for (int i = 0; i < kUiSpectrumBins; ++i)
        {
            if (bucketCount[(size_t) i] > 0)
            {
                bucketAvg[(size_t) i] = std::sqrt (bucketEnergy[(size_t) i] / (float) bucketCount[(size_t) i]);
                hasData[(size_t) i] = true;
            }
        }

        // 128 log-spaced buckets vastly outnumber the actual FFT bins
        // available below ~100Hz (only a handful of linear bins cover
        // 20-100Hz), leaving most low buckets with zero contributing bins.
        // Left at 0 they read as "no bass" even when there plainly is
        // some — it's a resolution mismatch, not silence. Fill the gaps
        // (see fillSpectrumGaps, unit-tested in Tests/CurveMathTests.cpp).
        fillSpectrumGaps (bucketAvg.data(), hasData.data(), kUiSpectrumBins);

        // Normalized by fftSize (which changes with sample rate — see
        // pickFftOrder) so a downstream dB conversion doesn't need to know
        // the current FFT size at all, just this one size-independent value.
        for (int i = 0; i < kUiSpectrumBins; ++i)
            uiSpectrum[(size_t) i].store (bucketAvg[(size_t) i] / (float) fftSize, std::memory_order_relaxed);
    }

    // Two scaled copies — one steered toward L, one toward R — each
    // inverse-transformed separately. (1 forward + 2 inverse FFTs per hop;
    // cheaper than processing L and R as independent channels, which would
    // need 2 forward + 2 inverse and still couldn't do this at all since
    // panning requires comparing across channels.)
    const int numBins = fftSize / 2 + 1;
    std::copy (fftData.begin(), fftData.end(), fftDataL.begin());
    std::copy (fftData.begin(), fftData.end(), fftDataR.begin());

    // Gain held constant across this whole hop — smoothing between hops
    // comes from the Hann-windowed overlap-add itself (see binGainL/R
    // comment in the header), not a separate ramp.
    for (int bin = 0; bin < numBins; ++bin)
    {
        const float bgL = binGainL[(size_t) bin];
        const float bgR = binGainR[(size_t) bin];
        fftDataL[(size_t) (2 * bin)]     *= bgL;
        fftDataL[(size_t) (2 * bin + 1)] *= bgL;
        fftDataR[(size_t) (2 * bin)]     *= bgR;
        fftDataR[(size_t) (2 * bin + 1)] *= bgR;
    }

    fft->performRealOnlyInverseTransform (fftDataL.data());
    fft->performRealOnlyInverseTransform (fftDataR.data());

    for (int i = 0; i < fftSize; ++i)
    {
        fftDataL[(size_t) i] *= window[(size_t) i];
        fftDataR[(size_t) i] *= window[(size_t) i];
    }

    // General overlap-add accumulator (correct for ANY hop fraction, not
    // just 50%): at 75% overlap up to 4 windowed frames can contribute to
    // the same output sample simultaneously, so this frame's full fftSize-
    // length windowed output is added into a running accumulator rather
    // than just stitching "current + one previous frame's tail" together
    // (that shortcut only happens to work at exactly 50% overlap).
    for (int i = 0; i < fftSize; ++i)
    {
        overlapL[(size_t) i] += fftDataL[(size_t) i];
        overlapR[(size_t) i] += fftDataR[(size_t) i];
    }
    for (int i = 0; i < hopSize; ++i)
    {
        outFifoLFifo[(size_t) i] = overlapL[(size_t) i];
        outFifoRFifo[(size_t) i] = overlapR[(size_t) i];
    }
    // Shift the accumulator left by one hop (the portion just output is
    // consumed) and zero-fill the newly-exposed tail for the next frame.
    std::copy (overlapL.begin() + hopSize, overlapL.end(), overlapL.begin());
    std::copy (overlapR.begin() + hopSize, overlapR.end(), overlapR.begin());
    std::fill (overlapL.end() - hopSize, overlapL.end(), 0.0f);
    std::fill (overlapR.end() - hopSize, overlapR.end(), 0.0f);
}

void RefraktProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const bool bypassed = *apvts.getRawParameterValue ("bypass") > 0.5f;
    const int numSamples = buffer.getNumSamples();
    const int numInCh = getTotalNumInputChannels();

    const auto* inL = buffer.getReadPointer (0);
    const auto* inR = numInCh > 1 ? buffer.getReadPointer (1) : buffer.getReadPointer (0); // mono-in: duplicate
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0);

    if (bypassed)
    {
        samplePosition += numSamples; // keep phase advancing so re-engaging bypass doesn't jump

        // Bypass still passes audio straight through, so the meters should
        // keep reflecting what's actually reaching the output rather than
        // freezing on their last pre-bypass reading.
        float peakL = 0.0f, peakR = 0.0f, sumLR = 0.0f, sumLL = 0.0f, sumRR = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            peakL = juce::jmax (peakL, std::abs (inL[i]));
            peakR = juce::jmax (peakR, std::abs (inR[i]));
            sumLR += inL[i] * inR[i]; sumLL += inL[i] * inL[i]; sumRR += inR[i] * inR[i];
        }
        meterPeakL.store (peakL, std::memory_order_relaxed);
        meterPeakR.store (peakR, std::memory_order_relaxed);
        meterCorrelation.store (sumLL > 1.0e-9f && sumRR > 1.0e-9f
            ? juce::jlimit (-1.0f, 1.0f, sumLR / std::sqrt (sumLL * sumRR)) : 1.0f, std::memory_order_relaxed);
        return;
    }

    const float outputGainDb = *apvts.getRawParameterValue ("output_gain");
    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outputGainDb));
    mixSmoothed.setTargetValue (*apvts.getRawParameterValue ("mix"));

    float meterPkL = 0.0f, meterPkR = 0.0f, meterSumLR = 0.0f, meterSumLL = 0.0f, meterSumRR = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = inL[i];
        const float r = inR[i];
        const float m = 0.5f * (l + r);
        const float s = 0.5f * (l - r);

        inFifoM[(size_t) fifoIndex] = m;
        sDelayLine[(size_t) delayWritePos] = s;
        dryDelayL[(size_t) delayWritePos] = l;
        dryDelayR[(size_t) delayWritePos] = r;

        // Same fftSize-sample delay applied to the side channel AND the dry
        // signal, so the Mix knob blends dry against wet perfectly aligned —
        // a misaligned blend is exactly what causes comb-filtering.
        const int readPos = (delayWritePos + 1) % fftSize; // oldest sample = fftSize behind
        const float sDelayed  = sDelayLine[(size_t) readPos];
        const float dryLDelay = dryDelayL[(size_t) readPos];
        const float dryRDelay = dryDelayR[(size_t) readPos];

        const float mOutL = outFifoLFifo[(size_t) hopCounter];
        const float mOutR = outFifoRFifo[(size_t) hopCounter];
        const float wetL = mOutL + sDelayed;
        const float wetR = mOutR - sDelayed;

        const float mixNow = mixSmoothed.getNextValue();
        const float gainNow = gainSmoothed.getNextValue();
        outL[i] = (wetL * mixNow + dryLDelay * (1.0f - mixNow)) * gainNow;
        outR[i] = (wetR * mixNow + dryRDelay * (1.0f - mixNow)) * gainNow;

        meterPkL = juce::jmax (meterPkL, std::abs (outL[i]));
        meterPkR = juce::jmax (meterPkR, std::abs (outR[i]));
        meterSumLR += outL[i] * outR[i]; meterSumLL += outL[i] * outL[i]; meterSumRR += outR[i] * outR[i];

        delayWritePos = (delayWritePos + 1) % fftSize;
        fifoIndex = (fifoIndex + 1) % fftSize; // true circular buffer — wraps at fftSize, not hopSize
        ++samplePosition;

        ++hopCounter;
        if (hopCounter >= hopSize)
        {
            hopCounter = 0;
            processFFTFrame(); // reads inFifoM via fifoIndex, refills outFifoL/RFifo for the next hopSize reads
        }
    }

    meterPeakL.store (meterPkL, std::memory_order_relaxed);
    meterPeakR.store (meterPkR, std::memory_order_relaxed);
    meterCorrelation.store (meterSumLL > 1.0e-9f && meterSumRR > 1.0e-9f
        ? juce::jlimit (-1.0f, 1.0f, meterSumLR / std::sqrt (meterSumLL * meterSumRR)) : 1.0f, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* RefraktProcessor::createEditor()
{
    return new RefraktEditor (*this);
}

bool RefraktProcessor::hasEditor() const { return true; }
const juce::String RefraktProcessor::getName() const { return "Refrakt"; }
bool RefraktProcessor::acceptsMidi() const  { return false; }
bool RefraktProcessor::producesMidi() const { return false; }
bool RefraktProcessor::isMidiEffect() const { return false; }
double RefraktProcessor::getTailLengthSeconds() const { return 0.0; }
int RefraktProcessor::getNumPrograms()    { return 1; }
int RefraktProcessor::getCurrentProgram() { return 0; }
void RefraktProcessor::setCurrentProgram (int) {}
const juce::String RefraktProcessor::getProgramName (int) { return {}; }
void RefraktProcessor::changeProgramName (int, const juce::String&) {}

void RefraktProcessor::setCustomShapeState (const std::vector<CustomNode>& nodes, const std::vector<ProtectZone>& zones)
{
    const juce::SpinLock::ScopedLockType sl (customShapeLock);
    numCustomNodes = juce::jmin ((int) nodes.size(), kMaxCustomNodes);
    for (int i = 0; i < numCustomNodes; ++i) customNodes[(size_t) i] = nodes[(size_t) i];
    numProtectZones = juce::jmin ((int) zones.size(), kMaxProtectZones);
    for (int i = 0; i < numProtectZones; ++i) protectZones[(size_t) i] = zones[(size_t) i];
}

void RefraktProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    // Custom nodes / protect zones aren't APVTS parameters (variable-length
    // state), so a DAW project save/recall would silently lose a Custom-shape
    // preset's actual shape without this — stashed as an extra child on the
    // same XML document rather than a second file/blob.
    auto* shapeXml = xml->createNewChildElement ("CUSTOM_SHAPE");
    {
        const juce::SpinLock::ScopedLockType sl (customShapeLock);
        for (int i = 0; i < numCustomNodes; ++i)
        {
            auto* n = shapeXml->createNewChildElement ("NODE");
            n->setAttribute ("freq", (double) customNodes[(size_t) i].freq);
            n->setAttribute ("pan",  (double) customNodes[(size_t) i].pan);
            n->setAttribute ("q",    (double) customNodes[(size_t) i].q);
        }
        for (int i = 0; i < numProtectZones; ++i)
        {
            auto* z = shapeXml->createNewChildElement ("ZONE");
            z->setAttribute ("lo", (double) protectZones[(size_t) i].lo);
            z->setAttribute ("hi", (double) protectZones[(size_t) i].hi);
        }
    }

    copyXmlToBinary (*xml, destData);
}

void RefraktProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    std::vector<CustomNode> nodes;
    std::vector<ProtectZone> zones;
    if (auto* shapeXml = xml->getChildByName ("CUSTOM_SHAPE"))
    {
        for (auto* child : shapeXml->getChildIterator())
        {
            if (child->hasTagName ("NODE"))
                nodes.push_back ({ (float) child->getDoubleAttribute ("freq"),
                                    (float) child->getDoubleAttribute ("pan"),
                                    (float) child->getDoubleAttribute ("q") });
            else if (child->hasTagName ("ZONE"))
                zones.push_back ({ (float) child->getDoubleAttribute ("lo"),
                                    (float) child->getDoubleAttribute ("hi") });
        }
    }
    setCustomShapeState (nodes, zones);

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RefraktProcessor();
}
