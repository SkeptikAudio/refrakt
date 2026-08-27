#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

class RefraktProcessor : public juce::AudioProcessor
{
public:
    RefraktProcessor();
    ~RefraktProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Real undo/redo — APVTS supports this natively given an UndoManager;
    // every SliderAttachment/ButtonAttachment/ComboBoxAttachment change
    // routes through it automatically, so the header's Undo/Redo buttons
    // just call undoManager.undo()/redo() directly.
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    // Custom-shape node curve (per-bin static bias / modulation source) and
    // protection zones (arbitrary protected freq bands, distinct from the
    // fixed-low-end Safe Bass toggle) — both ported 1:1 from the GUI
    // mockup's node/zone math. Neither is a flat automatable value, so
    // neither lives in APVTS: they're variable-length lists, only ever
    // authored via preset selection (no live node/zone editor in this GUI
    // pass), so a SpinLock guarding a fixed-capacity array is simpler and
    // just as safe as a lock-free scheme — writes are rare (preset loads,
    // message thread) and each read (once per hop, audio thread) is a tiny
    // fixed-size copy, never a blocking wait.
    struct CustomNode { float freq, pan, q; }; // freq is a 0-1 frac, same space as binFrac
    struct ProtectZone { float lo, hi; };      // frac 0-1, inclusive band
    static constexpr int kMaxCustomNodes = 16;
    static constexpr int kMaxProtectZones = 5;

    void setCustomShapeState (const std::vector<CustomNode>& nodes, const std::vector<ProtectZone>& zones);

    // Which band (if any) is currently soloed: -1 none, -2 Safe Bass,
    // 0..kMaxProtectZones-1 a protect zone index — see CurveMath.h's
    // getSoloMask for how this actually silences everything else. A single
    // int, written rarely (UI click) and read once per hop, so a plain
    // atomic is enough — no need for the SpinLock nodes/zones use.
    void setSoloTarget (int target) { soloTarget.store (target, std::memory_order_relaxed); }

    // UI-thread-safe copy for the curve display — same tiny fixed-capacity
    // copy-under-lock as computeBinGains does on the audio thread, just
    // called at UI framerate instead of once per hop.
    void getCustomShapeStateForDisplay (std::vector<CustomNode>& nodesOut, std::vector<ProtectZone>& zonesOut)
    {
        const juce::SpinLock::ScopedLockType sl (customShapeLock);
        nodesOut.assign (customNodes.begin(), customNodes.begin() + numCustomNodes);
        zonesOut.assign (protectZones.begin(), protectZones.begin() + numProtectZones);
    }

    // Real input spectrum, resampled onto the same log-frequency axis the
    // curve display uses (so a bar at x lines up with the curve's pan value
    // at that same frequency) — not the mockup's fabricated simSpec()
    // silhouette, actual magnitude from the FFT already being computed for
    // the DSP. One atomic array, written once per hop, read at UI framerate;
    // no locking needed since each element is independent.
    static constexpr int kUiSpectrumBins = 128;
    std::array<std::atomic<float>, kUiSpectrumBins> uiSpectrum {};

    // LFO phase, mirrored from the audio thread's lfoPhaseAccum so the curve
    // display can animate in exact sync with what's actually being applied
    // to the audio — not a separate wall-clock-driven UI animation that
    // would drift from the real modulation. Written once per hop.
    std::atomic<double> uiLfoPhase { 0.0 };

    // Meter state — real inter-channel analysis of what's actually leaving
    // the plugin each block (post-effect, post-bypass), not decorative math.
    // Written once per block on the audio thread, read at UI framerate by
    // the editor's Timer; std::atomic<float> is lock-free on every platform
    // this targets, so no locking needed for a single float each way.
    std::atomic<float> meterPeakL { 0.0f }, meterPeakR { 0.0f };
    // Phase correlation: +1 = mono/identical, 0 = decorrelated/wide, -1 = out of phase.
    std::atomic<float> meterCorrelation { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // --- FFT / STFT config (order picked at prepareToPlay time so bin width
    //     and latency in *seconds* stay consistent across sample rates) ---
    int fftOrder { 11 };                 // 2048 @ 44.1/48k; doubled per SR doubling below
    int fftSize  { 1 << 11 };
    // 75% overlap (not 50%): the window here is applied TWICE — analysis
    // AND synthesis — which means what actually needs to sum to a constant
    // across overlapping frames is window *squared*, not the window itself.
    // A squared Hann literally cannot satisfy that at 50% hop (verified:
    // w(i)+w(i+N/2)=1 exactly, but a²+(1-a)² is not constant for any
    // window shape). At 75% hop (N/4) it's exact — verified numerically,
    // ripple down to floating-point noise. This is the actual fix for the
    // raspy/buzzy artifact; the periodic-vs-symmetric window choice below
    // matters too but is secondary (leaves a ~4e-5 residual ripple on its own).
    int hopSize  { fftSize / 4 };
    static int pickFftOrder (double sampleRate);

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;

    // Mid path: analysed/re-synthesised through the FFT (this is the signal
    // that actually gets spectrally repositioned).
    //
    // inFifoM is a TRUE circular buffer of length fftSize (fifoIndex wraps
    // at fftSize, not hopSize — an earlier draft capped it at hopSize,
    // which meant only the first quarter of the analysis window ever held
    // fresh audio and the rest was permanently-stale zeros; verified this
    // was the actual cause of the raspy/glitchy passthrough, not the window
    // shape). outFifoL/R only ever need to hold one hop's worth of
    // already-finished output samples, so they're sized hopSize, read out
    // via the separate hopCounter below.
    std::vector<float> inFifoM;
    std::vector<float> outFifoLFifo, outFifoRFifo;
    std::vector<float> fftData;      // working forward-transform buffer (interleaved re/im)
    std::vector<float> fftDataL, fftDataR; // scratch copies scaled by gL(bin)/gR(bin) before inverse transform
    std::vector<float> overlapL, overlapR;

    // Side path: NOT run through the FFT at all. Whatever inter-channel
    // difference the input already had is preserved untouched — we only
    // ever reposition the mid content, never the listener's existing width.
    // Delayed by fftSize samples so it re-aligns with the FFT path's
    // inherent block latency. Dry (for Mix) needs the identical delay for
    // the identical reason, so both share one write cursor — they are, by
    // construction, always the same position.
    std::vector<float> sDelayLine, dryDelayL, dryDelayR;
    int delayWritePos { 0 };

    int fifoIndex { 0 };   // circular write pointer into inFifoM, wraps at fftSize
    int hopCounter { 0 };  // counts up to hopSize; also the read index into outFifoL/R

    // Sample-accurate modulation phase. Never derived from wall-clock time:
    // a bounce must match realtime playback exactly, and starting playback
    // from two different points in a song must not change the LFO's
    // relationship to what's audible. Prefers the host's play-head position;
    // falls back to a free-running per-instance sample counter when a host
    // provides no transport (some standalone contexts).
    juce::int64 samplePosition { 0 };

    // LFO phase is a running accumulator (advanced each hop using whatever
    // Rate is CURRENT that hop), not phase=rate*elapsedTime. The
    // direct-formula version implicitly assumes rate was constant since
    // the FFT window started — the instant Rate actually changes
    // (automation, or a live knob move), that formula jumps the phase to
    // a different value instead of continuing smoothly from wherever it
    // already was. This is what a real analog LFO's FM input does, and
    // it's what makes automating Rate from a slow pulse into fast movement
    // actually flow instead of clicking at every change. (Unrelated to
    // samplePosition above, which stays tied to absolute sample position
    // specifically for Random-shape reproducibility — this accumulator is
    // deliberately decoupled from that.)
    double lfoPhaseAccum { 0.0 };

    // Drives the Custom-shape ping-pong scroll (pingPongPos), tracked
    // separately from lfoPhaseAccum and wrapped at period 2 to match
    // pingPongPos's own period exactly — deriving it from lfoPhase/twoPi
    // instead (wrapped at period 1) caused an audible click once per LFO
    // cycle, right at the wrap point, since shifting a period-2 function's
    // argument by an odd amount (1) lands on the wrong half of its fold.
    double cycleScrollAccum { 0.0 };

    // Bin gains are only recomputed once per hop and held constant across
    // it — that's fine, not a gap: with 75% overlap, the Hann-windowed
    // overlap-add already cross-fades each bin's contribution continuously
    // between consecutive hops (that cross-fade *is* the interpolation).
    // At the Rate ceiling (10Hz => 100ms/cycle) against a hop of ~5-21ms,
    // there are still several hops per cycle, so no separate gain-ramping
    // state is needed. If narrower/faster settings are added later and
    // audible stepping shows up, the fix is denser overlap still (87.5%),
    // not per-sample gain interpolation on top of what overlap-add already gives.
    std::vector<float> binGainL, binGainR; // reused every hop — never (re)allocated on the audio thread

    // Output Gain and Mix are read once per BLOCK (unlike the per-hop bin
    // gains, which get their smoothing for free from the FFT overlap-add
    // crossfade — see the comment on binGainL/R above). Applied flat with no
    // smoothing, a host automating either fast enough to change value
    // between blocks previously produced a hard step at the block boundary
    // — audible as a click/zipper, worst-case with small block sizes where
    // block boundaries are frequent. Ramped per-sample instead now.
    juce::SmoothedValue<float> gainSmoothed, mixSmoothed;

    // Bin-frequency mapping (binFrac) is a pure function of (bin index,
    // fftSize, sampleRate) — none of which change between hops — so it's
    // computed once here instead of twice per bin (two log10 calls), every
    // single hop. There's no equivalent binX cache: the actual pan-curve x
    // per bin depends on the live, automatable Pivot parameter too, so it
    // has to be recomputed each hop from binFrac (see computeBinGains).
    std::vector<double> binFrac;

    double currentSampleRate { 44100.0 };

    juce::SpinLock customShapeLock;
    std::array<CustomNode, kMaxCustomNodes> customNodes {};
    int numCustomNodes { 0 };
    std::array<ProtectZone, kMaxProtectZones> protectZones {};
    int numProtectZones { 0 };
    std::atomic<int> soloTarget { -1 };

    void computeBinGains (juce::int64 frameStartSample);
    void processFFTFrame();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RefraktProcessor)
};
