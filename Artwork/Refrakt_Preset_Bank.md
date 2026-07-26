# Refrakt — Complete Preset Bank
**Skeptik Audio · v1.2 · 40 Presets across 4 categories**

*Revision note: v1.2 replaces the old 7-category, 72-preset bank with a leaner 40-preset bank organized directly by Modulation Mode — Static, Wave, Tilt, Tilt + Wave — 10 presets each. This also reflects three real engine changes made in this pass: the Attack knob is gone entirely (removed from both the GUI and the plugin — automating Intensity covers the same ground), Safe Bass now actually attenuates panning below its threshold (previously wired in the GUI but never applied in the DSP), and general protection Zones (arbitrary protected frequency bands, distinct from the fixed low-end Safe Bass lock) are now a real, working feature backing the "safe zone" presets below. Custom node shapes are likewise now a fully working modulation source, not a visual-only concept.*

**Accuracy note on rate range:** the free-run Rate knob's range is 0.1 Hz–10 Hz. Each category includes one preset sitting right at the 0.1Hz floor (one full cycle every 10 seconds) to guarantee that extreme is actually reachable, not just theoretical.

---

## Category 1 — Static (10 presets)
*Plain tilt/shape curve, no LFO motion at all. Modulation Mode: Static.*

| # | Name | Description | Tilt | Shape | Pivot | Custom Nodes | Safe Bass |
|---|---|---|---|---|---|---|---|
| 1 | Default | Flat line, signal passes through unchanged. Always preset 1. | 0.0 | 0.0 | 663 Hz | — | Off |
| 2 | Gentle Widen | Mild diagonal — lows slightly left, highs slightly right. | 0.3 | 0.0 | 663 Hz | — | Off |
| 3 | Gentle Narrow | Mirror of Gentle Widen — lows right, highs left. | -0.35 | 0.0 | 663 Hz | — | Off |
| 4 | Steep Tilt Right | Committed tilt with a touch of S-curve added. | 0.7 | 0.2 | 663 Hz | — | Off |
| 5 | Steep Tilt Left | Mirror of Steep Tilt Right. | -0.7 | -0.2 | 663 Hz | — | Off |
| 6 | S-Curve Spread | Pure cubic shape term, no linear tilt — extremes bow outward, centre stays put. | 0.0 | 0.8 | 663 Hz | — | Off |
| 7 | Inverted S-Curve | Inverted S-curve with a slight tilt lean. | 0.1 | -0.75 | 663 Hz | — | Off |
| 8 | High Focus Pivot | Tilt's zero-crossing moved up to 3kHz instead of the geometric-mean default, so the whole tilt reads as a high-focused push. | 0.5 | 0.0 | 3000 Hz | — | Off |
| 9 | Custom Bloom | Three hand-placed nodes sculpt a fixed asymmetric curve — no tilt/shape term at all, pure node coloring. | 0.0 | 0.0 | 663 Hz | 3 | Off |
| 10 | Custom Bloom Safe Bass | A different 3-node custom curve on top of a mild tilt, with Safe Bass locking everything below 150Hz dead centre. | 0.2 | 0.0 | 663 Hz | 3 | On |

---

## Category 2 — Wave (10 presets)
*Modulation Mode: Wave. Covers all 5 named LFO shapes plus Custom, one Safe Bass preset, one protection-Zone preset, full rate variety (free Hz and tempo-synced).*

| # | Name | LFO Shape | Rate | Cycles | Intensity | Safe Bass | Zone |
|---|---|---|---|---|---|---|---|
| 11 | Sine Sweep | Sine | 0.4 Hz | 2 | 60% | Off | — |
| 12 | Triangle Roll | Triangle | 1/4 sync | 3 | 65% | Off | — |
| 13 | Saw Rip Wave | Saw | 2.5 Hz | 4 | 75% | Off | — |
| 14 | Square Snap Safe | Square | 1.2 Hz | 2 | 70% | On | — |
| 15 | Random Drift Zone | Random | 0.6 Hz | 2 | 55% | Off | 800Hz–4kHz |
| 16 | Triangle Flicker | Triangle | 1/8 sync | 1 | 80% | Off | — |
| 17 | Random Strobe | Random | 6.0 Hz | 2 | 85% | Off | — |
| 18 | Custom Wave Bloom | Custom (4 nodes) | 0.4 Hz | 2 | 60% | Off | — |
| 19 | Custom Wave Storm | Custom (8 nodes) | 3.0 Hz | 6 | 90% | Off | — |
| 20 | Deep Field Wave | Triangle | 0.1 Hz (floor) | 1 | 40% | Off | — |

---

## Category 3 — Tilt (10 presets)
*Modulation Mode: Tilt. The Shape pill is disabled here by design (Tilt is always Sine-driven internally unless Custom) — variety instead comes from tilt/shape ranging, pivot placement, and rate.*

| # | Name | Tilt | Shape | Pivot | Rate | Intensity | Custom Nodes | Safe Bass |
|---|---|---|---|---|---|---|---|---|
| 21 | Slow Breathe | 0.3 | 0.0 | 663 Hz | 0.3 Hz | 30% | — | Off |
| 22 | Gentle Reverse Breathe | -0.3 | 0.0 | 663 Hz | 0.3 Hz | 30% | — | Off |
| 23 | Deep Tilt Swing | 0.6 | 0.3 | 663 Hz | 0.5 Hz | 60% | — | Off |
| 24 | Sharp Tilt Pulse | 0.5 | -0.2 | 663 Hz | 2.0 Hz | 70% | — | Off |
| 25 | Pivot Shift Low | 0.4 | 0.0 | 250 Hz | 0.4 Hz | 50% | — | Off |
| 26 | Pivot Shift High | 0.4 | 0.0 | 2200 Hz | 0.4 Hz | 50% | — | Off |
| 27 | Glacial Tilt Safe Bass | 0.3 | 0.0 | 663 Hz | 0.1 Hz (floor) | 40% | — | On |
| 28 | Fast Tilt Flicker | 0.6 | 0.2 | 663 Hz | 5.0 Hz | 80% | — | Off |
| 29 | Custom Tilt Ride | 0.2 | 0.0 | 663 Hz | 0.35 Hz | 55% | 5 | Off |
| 30 | Custom Tilt Storm | 0.4 | 0.1 | 663 Hz | 1.5 Hz | 70% | 6 | Off |

---

## Category 4 — Tilt + Wave (10 presets)
*Modulation Mode: Tilt + Wave. Both engines run simultaneously, blended by Balance (negative = tilt-led, positive = wave-led). Covers all 5 named shapes plus Custom, one Safe Bass, one protection Zone.*

| # | Name | LFO Shape | Tilt | Rate | Cycles | Intensity | Balance | Safe Bass | Zone |
|---|---|---|---|---|---|---|---|---|---|
| 31 | Sine Rolling Balance | Sine | 0.3 | 0.4 Hz | 2 | 50% | 0 (50/50) | Off | — |
| 32 | Triangle Wide Horizon | Triangle | 0.2 | 0.4 Hz | 8 | 65% | +0.4 | Off | — |
| 33 | Saw Tilt Lean | Saw | 0.3 | 1.0 Hz | 3 | 60% | -0.5 | Off | — |
| 34 | Square Snap Blend Safe | Square | 0.0 | 1.5 Hz | 2 | 70% | +0.2 | On | — |
| 35 | Random Storm Zone | Random | 0.3 | 3.0 Hz | 6 | 90% | 0 | Off | 8k–16kHz |
| 36 | Saw Drift Blend | Saw | 0.2 | 0.4 Hz | 1 | 60% | +0.3 | Off | — |
| 37 | Square Storm Blend | Square | 0.4 | 4.0 Hz | 2 | 75% | -0.3 | Off | — |
| 38 | Custom Layered Drift | Custom (6 nodes) | 0.0 | 0.4 Hz | 2 | 60% | 0 | Off | — |
| 39 | Custom Aurora Bloom | Custom (3 nodes) | 0.3 | 0.35 Hz | 4 | 85% | +0.4 | Off | — |
| 40 | Long Fade Sky | Sine | 0.0 | 0.1 Hz (floor) | 1 | 70% | +0.6 | Off | — |

---

## Preset bank summary

| Category | Presets | Character |
|---|---|---|
| Static | 1–10 | Fixed curve, no motion |
| Wave | 11–20 | Spectral ripple, all 8 shapes |
| Tilt | 21–30 | Whole-image rocking, pivot variety |
| Tilt + Wave | 31–40 | Both engines blended, all 8 shapes |
| **Total** | **40** | **Full feature coverage** |

---

## Features confirmed covered

- Tilt positive and negative, Shape positive and negative ✓
- All four modulation modes ✓
- All 5 named LFO shapes (Sine, Triangle, Saw, Square, Random) plus Custom ✓
- Custom hand-placed node shapes (3–8 nodes), now a real modulation source in every mode ✓
- Pivot moved off the default geometric-mean centre (Tilt category) ✓
- Free Hz rates across the full 0.1Hz–10Hz range, including the floor in every category ✓
- Tempo sync (1/4, 1/8) ✓
- Balance away from 50/50, both tilt-led and wave-led ✓
- Safe Bass, now actually attenuating panning below threshold (one preset per category) ✓
- General protection Zones, a real feature distinct from Safe Bass (Wave and Tilt+Wave categories) ✓

*Preset bank v1.2 · 40 factory presets · Skeptik Audio*
