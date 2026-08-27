// Regression suite for the pure math in Source/CurveMath.h — this is the
// code the black-bar/pivot-spike/ping-pong-jerk/bass-starvation bugs all
// lived in, and every one of them was only ever caught by eye during manual
// testing. These tests pin down the properties that broke before, so a
// future edit can't silently reintroduce the same class of bug.
#include "../Source/CurveMath.h"
#include <cstdio>
#include <cmath>
#include <vector>

using namespace RefraktCurveMath;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { \
    ++checks; \
    if (!(cond)) { ++failures; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::abs((double)(a) - (double)(b)) < (eps))

struct TestNode { float freq, pan, q; };
struct TestZone { double lo, hi; };

static void testHzToFrac()
{
    CHECK_NEAR(hzToFrac (20.0), 0.0, 1e-9);
    CHECK_NEAR(hzToFrac (22000.0), 1.0, 1e-9);
    CHECK_NEAR(hzToFrac (1.0), 0.0, 1e-9);      // clamps below range
    CHECK_NEAR(hzToFrac (999999.0), 1.0, 1e-9); // clamps above range

    // Monotonic increasing across the whole range.
    double prev = -1.0;
    for (double hz = 20.0; hz <= 22000.0; hz *= 1.1)
    {
        const double f = hzToFrac (hz);
        CHECK(f >= prev);
        prev = f;
    }
}

static void testWarpX()
{
    for (double k : { 0.12, 0.5, 1.0 })
    {
        // Endpoints always pinned regardless of k.
        CHECK_NEAR(warpX (0.0, k), 0.0, 1e-9);
        CHECK_NEAR(warpX (1.0, k), 1.0, 1e-9);
        CHECK_NEAR(warpX (-1.0, k), -1.0, 1e-9);

        // Monotonic increasing in x.
        double prev = -2.0;
        for (double x = -1.0; x <= 1.0; x += 0.05)
        {
            const double v = warpX (x, k);
            CHECK(v >= prev);
            prev = v;
        }

        // Finite slope at x=0 (the original pow()-based curve had infinite
        // slope here, which produced an audible spike right at the pivot).
        const double eps = 1e-4;
        const double slope = (warpX (eps, k) - warpX (-eps, k)) / (2.0 * eps);
        CHECK(std::isfinite (slope));
        CHECK(std::abs (slope) < 50.0);
    }

    // k=1 collapses to identity (no warp).
    for (double x = -1.0; x <= 1.0; x += 0.1)
        CHECK_NEAR(warpX (x, 1.0), x, 1e-9);
}

static void testBoostToK()
{
    CHECK_NEAR(boostToK (0.0), 1.0, 1e-9);
    CHECK_NEAR(boostToK (1.0), 0.12, 1e-9);
    CHECK_NEAR(boostToK (-5.0), 1.0, 1e-9);  // clamps
    CHECK_NEAR(boostToK (5.0), 0.12, 1e-9);  // clamps

    // Monotonic decreasing (more boost -> smaller k -> sharper pivot).
    double prev = 2.0;
    for (double b = 0.0; b <= 1.0; b += 0.1)
    {
        const double k = boostToK (b);
        CHECK(k <= prev + 1e-9);
        prev = k;
    }
}

static void testPingPongPos()
{
    CHECK_NEAR(pingPongPos (0.0), 0.0, 1e-9);
    CHECK_NEAR(pingPongPos (1.0), 1.0, 1e-9);
    CHECK_NEAR(pingPongPos (2.0), 0.0, 1e-9);
    CHECK_NEAR(pingPongPos (3.0), 1.0, 1e-9);
    CHECK_NEAR(pingPongPos (-1.0), 1.0, 1e-9);

    // Always in [0,1], and continuous — no discontinuity at any period
    // wrap (this is exactly the class of bug the ping-pong "jerk" was:
    // a derived scroll term wrapping at a mismatched period).
    double prevV = pingPongPos (-10.0);
    const double step = 0.01;
    for (double t = -10.0; t <= 10.0; t += step)
    {
        const double v = pingPongPos (t);
        CHECK(v >= -1e-9 && v <= 1.0 + 1e-9);
        CHECK(std::abs (v - prevV) < step * 1.5); // no jump bigger than the step itself
        prevV = v;
    }
}

static void testTiltShapeAt()
{
    CHECK_NEAR(tiltShapeAt (1.0, 0.0, 0.5), 0.5, 1e-9);
    CHECK_NEAR(tiltShapeAt (0.0, 1.0, 0.5), 0.125, 1e-9); // 0.5^3
    CHECK_NEAR(tiltShapeAt (1.0, 0.0, 0.0), 0.0, 1e-9);
}

static void testNodeInfluence()
{
    std::vector<TestNode> nodes = { { 0.5f, 1.0f, 4.0f } };

    // Peak exactly at the node's own frequency equals its pan value.
    CHECK_NEAR(nodeInfluence (0.5, nodes, 1, false), 1.0, 1e-6);

    // Decays away from the node.
    CHECK(std::abs (nodeInfluence (0.9, nodes, 1, false)) < 0.05);

    // Inversion flips sign exactly.
    CHECK_NEAR(nodeInfluence (0.5, nodes, 1, true), -1.0, 1e-6);

    // Non-finite nodes are skipped, not propagated as NaN.
    std::vector<TestNode> badNodes = { { NAN, 1.0f, 4.0f }, { 0.5f, 1.0f, 4.0f } };
    CHECK(std::isfinite (nodeInfluence (0.5, badNodes, 2, false)));
}

static void testGetProtection()
{
    std::vector<TestZone> noZones;
    CHECK_NEAR(getProtection (0.1, true, 0.2, noZones, 0), 1.0, 1e-9);  // below safe-bass freq
    CHECK_NEAR(getProtection (0.5, true, 0.2, noZones, 0), 0.0, 1e-9);  // well above it

    std::vector<TestZone> zones = { { 0.4, 0.6 } };
    CHECK_NEAR(getProtection (0.5, false, 0.0, zones, 1), 1.0, 1e-9);   // inside zone
    CHECK_NEAR(getProtection (0.9, false, 0.0, zones, 1), 0.0, 1e-9);   // far outside

    // Edge fade is bounded within [0,1] and never negative.
    for (double frac = 0.3; frac <= 0.7; frac += 0.01)
    {
        const double p = getProtection (frac, false, 0.0, zones, 1);
        CHECK(p >= -1e-9 && p <= 1.0 + 1e-9);
    }
}

static void testGetSoloMask()
{
    std::vector<TestZone> zones = { { 0.4, 0.6 }, { 0.8, 0.9 } };

    // No solo active -> nothing muted anywhere.
    for (double frac = 0.0; frac <= 1.0; frac += 0.1)
        CHECK_NEAR(getSoloMask (frac, -1, true, 0.2, zones, 2), 1.0, 1e-9);

    // Safe Bass solo (-2): inside its band untouched, outside silenced.
    CHECK_NEAR(getSoloMask (0.05, -2, true, 0.2, zones, 2), 1.0, 1e-9);
    CHECK_NEAR(getSoloMask (0.5, -2, true, 0.2, zones, 2), 0.0, 1e-9);

    // Safe Bass solo requested but Safe Bass itself is off -> fails open
    // (no muting) rather than silencing the whole mix on stale state.
    CHECK_NEAR(getSoloMask (0.05, -2, false, 0.2, zones, 2), 1.0, 1e-9);

    // Zone solo (index 0 = the 0.4-0.6 band): inside untouched, outside silenced.
    CHECK_NEAR(getSoloMask (0.5, 0, true, 0.2, zones, 2), 1.0, 1e-9);
    CHECK_NEAR(getSoloMask (0.05, 0, true, 0.2, zones, 2), 0.0, 1e-9); // even Safe Bass's own range is silenced when a zone is soloed
    CHECK_NEAR(getSoloMask (0.85, 0, true, 0.2, zones, 2), 0.0, 1e-9); // the OTHER zone is silenced too

    // Out-of-range zone index (e.g. deleted while solo was active) -> fails
    // open, never silences everything.
    CHECK_NEAR(getSoloMask (0.5, 5, true, 0.2, zones, 2), 1.0, 1e-9);

    // Always bounded [0,1], and the edge fade is smooth (no discontinuity
    // bigger than the step itself) crossing a zone boundary.
    double prev = getSoloMask (0.3, 0, true, 0.2, zones, 2);
    const double step = 0.001;
    for (double frac = 0.3; frac <= 0.7; frac += step)
    {
        const double m = getSoloMask (frac, 0, true, 0.2, zones, 2);
        CHECK(m >= -1e-9 && m <= 1.0 + 1e-9);
        CHECK(std::abs (m - prev) < step / kSoloSnap * 1.5 + 1e-6);
        prev = m;
    }
}

static void testFillSpectrumGaps()
{
    // All buckets already have data -> untouched.
    {
        float vals[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
        bool has[4] = { true, true, true, true };
        fillSpectrumGaps (vals, has, 4);
        CHECK_NEAR(vals[0], 1.0f, 1e-9); CHECK_NEAR(vals[3], 4.0f, 1e-9);
    }

    // Single gap between two real buckets -> exact linear interpolation.
    {
        float vals[3] = { 0.0f, 0.0f, 10.0f };
        bool has[3] = { true, false, true };
        fillSpectrumGaps (vals, has, 3);
        CHECK_NEAR(vals[1], 5.0f, 1e-6);
    }

    // Leading gap (no left neighbour) -> nearest-neighbour from the right —
    // this is the exact low-frequency-bucket-starvation case that made bass
    // read as falsely absent.
    {
        float vals[4] = { 0.0f, 0.0f, 0.0f, 8.0f };
        bool has[4] = { false, false, false, true };
        fillSpectrumGaps (vals, has, 4);
        CHECK_NEAR(vals[0], 8.0f, 1e-6);
        CHECK_NEAR(vals[1], 8.0f, 1e-6);
        CHECK_NEAR(vals[2], 8.0f, 1e-6);
    }

    // Trailing gap -> nearest-neighbour from the left.
    {
        float vals[3] = { 6.0f, 0.0f, 0.0f };
        bool has[3] = { true, false, false };
        fillSpectrumGaps (vals, has, 3);
        CHECK_NEAR(vals[1], 6.0f, 1e-6);
        CHECK_NEAR(vals[2], 6.0f, 1e-6);
    }

    // No bucket anywhere has data (true silence) -> stays at initial value,
    // no crash, nothing invented.
    {
        float vals[3] = { 0.0f, 0.0f, 0.0f };
        bool has[3] = { false, false, false };
        fillSpectrumGaps (vals, has, 3);
        CHECK_NEAR(vals[0], 0.0f, 1e-9);
        CHECK_NEAR(vals[1], 0.0f, 1e-9);
        CHECK_NEAR(vals[2], 0.0f, 1e-9);
    }
}

static void testLfoWaveformContinuity()
{
    const double twoPi = juce::MathConstants<double>::twoPi;
    for (int shape = 0; shape <= 4; ++shape)
    {
        // Wrapping the phase (e.g. 2*pi + x vs x) must give the same value —
        // fmod-based wrap, no seam at the cycle boundary.
        for (double x : { 0.0, 0.3, 1.5, 3.14, 6.0 })
        {
            const float a = lfoWaveform (x, shape, false, 0.0);
            const float b = lfoWaveform (x + twoPi, shape, false, 0.0);
            CHECK_NEAR(a, b, 1e-4);
        }

        // Inversion flips sign exactly.
        const float v = lfoWaveform (1.0, shape, false, 0.0);
        const float vi = lfoWaveform (1.0, shape, true, 0.0);
        CHECK_NEAR(v, -vi, 1e-6);

        // Always stays in [-1, 1] (no runaway from the soft-saturation /
        // easing math in the square/saw/random cases).
        for (double x = 0.0; x < twoPi; x += 0.1)
        {
            const float v2 = lfoWaveform (x, shape, false, 1.23);
            CHECK(v2 >= -1.0001f && v2 <= 1.0001f);
        }
    }
}

int main()
{
    testHzToFrac();
    testWarpX();
    testBoostToK();
    testPingPongPos();
    testTiltShapeAt();
    testNodeInfluence();
    testGetProtection();
    testGetSoloMask();
    testFillSpectrumGaps();
    testLfoWaveformContinuity();

    std::printf ("%d/%d checks passed\n", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
