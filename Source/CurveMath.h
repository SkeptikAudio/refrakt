#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

// The pan-curve math, shared verbatim between the audio-thread DSP
// (PluginProcessor::computeBinGains) and the UI-thread curve display
// (CurveView) — both need EXACTLY the same formulas, not just formulas that
// happen to match, or the on-screen curve would silently drift from what's
// actually audible. Previously this was duplicated by hand between the two;
// now there is exactly one copy. Also mirrors the GUI mockup's JS 1:1 (see
// comments there) so mockup / DSP / display never diverge three ways.
namespace RefraktCurveMath
{
    constexpr double kFreqMinHz = 20.0;
    constexpr double kFreqMaxHz = 22000.0;

    inline double hzToFrac (double hz)
    {
        const double logMin = std::log10 (kFreqMinHz);
        const double logMax = std::log10 (kFreqMaxHz);
        const double f = (std::log10 (juce::jmax (1.0, hz)) - logMin) / (logMax - logMin);
        return juce::jlimit (0.0, 1.0, f);
    }

    // Each shape stays continuous at its phase wrap point (soft saturation
    // for square, eased tail for saw, smoothstep interpolation for random)
    // so none introduce a hard discontinuity into the control signal
    // regardless of Rate — this is why no audio-rate oversampling is needed
    // for this effect.
    inline float lfoWaveform (double phase, int shapeIndex, bool inverted, double randomPhaseSeconds)
    {
        phase = std::fmod (phase, juce::MathConstants<double>::twoPi);
        if (phase < 0.0) phase += juce::MathConstants<double>::twoPi;
        const double x = phase / juce::MathConstants<double>::twoPi;
        const double dir = inverted ? -1.0 : 1.0;
        double v = 0.0;

        switch (shapeIndex)
        {
            case 0: // Sine
                v = std::sin (phase);
                break;
            case 1: // Triangle
                v = x < 0.5 ? (4.0 * x - 1.0) : (3.0 - 4.0 * x);
                break;
            case 2: // Saw
            {
                const double w = 0.05;
                if (x > 1.0 - w)
                {
                    const double tt = (x - (1.0 - w)) / w;
                    const double eased = tt * tt * (3.0 - 2.0 * tt);
                    v = juce::jmap (eased, 2.0 * x - 1.0, -1.0);
                }
                else v = 2.0 * x - 1.0;
                break;
            }
            case 3: // Square — soft-saturated, not a hard step, so it never aliases the control signal
            {
                const double k = 14.0;
                static const double norm = std::tanh (14.0);
                v = std::tanh (k * std::sin (phase)) / norm;
                break;
            }
            case 4: // Random — seeded by elapsed *sample* time, never wall-clock,
                    // so a bounce reproduces bit-for-bit and re-opening a session
                    // gives back the same modulation trajectory.
            {
                const double segF = x * 3.2 + randomPhaseSeconds * 2.5;
                const double seg = std::floor (segF);
                const double fr = segF - seg;
                auto hash = [] (double n)
                {
                    // C++ fmod keeps the sign of the dividend (like JS %), so
                    // wrap twice to land in [0,1) before mapping to [-1,1).
                    double h = std::fmod (std::sin (n * 12.9898) * 43758.5453, 1.0);
                    h = std::fmod (h + 1.0, 1.0);
                    return h * 2.0 - 1.0;
                };
                const double v0 = hash (seg);
                const double v1 = hash (seg + 1.0);
                const double te = fr * fr * (3.0 - 2.0 * fr);
                v = juce::jmap (te, v0, v1);
                break;
            }
            default:
                v = std::sin (phase);
        }
        return (float) (v * dir);
    }

    // The frequency axis is linear in log-frequency, which compresses most
    // of the audible midrange near x=0 — at 500Hz-1kHz, even Tilt=1 only
    // reached ~8-12% pan raw. This warp boosts the middle of the range
    // while x=±1 (20Hz/22kHz) stays exactly ±1. Rational "soft-knee" form
    // (not a pow(|x|,exponent) power curve) so the slope at x=0 — right at
    // the pivot crossing — is finite instead of vertical: a pow curve with
    // exponent<1 has infinite slope at 0, which produced an audible (not
    // just visual) spike right at the pivot, since this formula feeds the
    // real per-bin gain. Mirrors the mockup's JS warpX() exactly.
    //
    // k controls how strong that boost is: 0.12 is the original sharp
    // character, 1.0 collapses to identity (x/(1+0)=x, i.e. no warp — a
    // plain log-linear response) — this is what the Boost knob drives.
    inline double warpX (double x, double k = 1.0)
    {
        return x / (k + (1.0 - k) * std::abs (x));
    }

    // Maps the Boost parameter (0=flat/no warp, the default..1=full sharp
    // pivot emphasis) onto warpX's k.
    inline double boostToK (double boost) { return juce::jmap (juce::jlimit (0.0, 1.0, boost), 1.0, 0.12); }

    // Two independent linear-in-log segments either side of the pivot: 20Hz..pivot
    // maps to -1..0, pivot..22000Hz maps to 0..+1. Endpoints stay pinned exactly
    // where they always were — only where the zero-crossing sits moves.
    inline double pivotX (double frac, double pivotFrac)
    {
        return frac <= pivotFrac ? (frac / pivotFrac - 1.0) : ((frac - pivotFrac) / (1.0 - pivotFrac));
    }

    inline double tiltShapeAt (double tilt, double shape, double x)
    {
        return tilt * x + shape * x * x * x;
    }

    inline double lerp (double a, double b, double t) { return a + (b - a) * t; }

    // Reflects t (which runs unbounded as frac*cycles-phase advances) into a
    // 0-1 ping-pong ramp — 0..1..0..1... — so a Custom-shape node pattern
    // sweeps back and forth across the curve instead of snapping back to the
    // start each cycle.
    inline double pingPongPos (double t)
    {
        double w = std::fmod (std::fmod (t, 2.0) + 2.0, 2.0);
        return w <= 1.0 ? w : 2.0 - w;
    }

    // Gaussian-bump width for a custom node: higher Q = narrower influence.
    inline double nodeWidth (double q) { return 0.09 / q; }

    // Sum of every node's Gaussian pan bump at this frac position. This is
    // the ONLY thing that makes Custom-shape nodes audible — there is no
    // separate per-node filter; each node is just a soft pan bias centred
    // at its frequency, exactly like the mockup's node-editor preview.
    template <typename NodeArray>
    double nodeInfluence (double frac, const NodeArray& nodes, int numNodes, bool inverted)
    {
        double v = 0.0;
        for (int i = 0; i < numNodes; ++i)
        {
            const auto& n = nodes[(size_t) i];
            if (! std::isfinite (n.freq) || ! std::isfinite (n.pan) || ! std::isfinite (n.q)) continue;
            const double d = frac - (double) n.freq;
            const double width = nodeWidth ((double) n.q);
            v += (double) n.pan * std::exp (-(d * d) / (2.0 * width * width));
        }
        return v * (inverted ? -1.0 : 1.0);
    }

    // Protection amount (0=untouched, 1=fully forced to centre) at this
    // frac: Safe Bass locks everything below its frequency dead centre, and
    // each protect zone locks its band dead centre — both with a SNAP-wide
    // linear fade on the edges so protection never introduces a hard seam
    // in the spectrum.
    constexpr double kProtSnap = 0.025;
    template <typename ZoneArray>
    double getProtection (double frac, bool safeBassOn, double safeBassFrac, const ZoneArray& zones, int numZones)
    {
        double p = 0.0;
        if (safeBassOn)
        {
            if (frac <= safeBassFrac) p = 1.0;
            else if (frac <= safeBassFrac + kProtSnap) p = juce::jmax (p, 1.0 - (frac - safeBassFrac) / kProtSnap);
        }
        for (int i = 0; i < numZones; ++i)
        {
            const auto& z = zones[(size_t) i];
            if (frac >= z.lo && frac <= z.hi) { p = 1.0; continue; }
            if (frac >= z.lo - kProtSnap && frac < z.lo) p = juce::jmax (p, (frac - (z.lo - kProtSnap)) / kProtSnap);
            else if (frac > z.hi && frac <= z.hi + kProtSnap) p = juce::jmax (p, 1.0 - (frac - z.hi) / kProtSnap);
        }
        return p;
    }
}
