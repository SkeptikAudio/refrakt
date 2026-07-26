# Product

## Register

product

## Users

Music producers and sound designers using the Refrakt VST3 plugin inside a DAW, making real-time spectral-pan decisions while mixing or producing — typically in a dark studio environment, glancing between this plugin window and several others.

## Product Purpose

Refrakt is a spectral panning plugin (per-frequency-band stereo positioning) by Skeptik Audio. This repo is an interactive HTML/CSS/JS mockup of its GUI — no build tooling, opened directly from disk — used to prototype and validate the visual/interaction design before implementation in the real JUCE-based plugin.

## Brand Personality

Precise / technical. Reads as a scientific optics instrument rather than a generic synth skin — ties directly to the "refraction/prism" identity direction (the plugin's name literally means light-bending). Calm at rest; controls only become expressive in direct response to interaction, never decoratively.

## Anti-references

- **Busy/gimmicky motion.** No continuous ambient animation loops, no decoration that runs regardless of user action. Confirmed by direct trial this session: a continuously-looping ambient shimmer and a static glowing "entry point" glyph were both built and explicitly rejected as gimmicky. Motion must be triggered by and tied to a real user action.
- **The generic dark+purple boutique-plugin look** (Baby Audio / Ozone / Serum-style: near-black background, single neon accent, uniform white glow on every hover state). This is the default the whole project is trying to differentiate from, not lean into further.
- **FabFilter Pro-Q3's band-editor silhouette.** The curve-with-draggable-zone-handles interaction is structurally similar (by necessity — it's a good pattern for editing frequency ranges), but the visual treatment shouldn't read as a repaint of it.

## Design Principles

1. **Motion is earned, not ambient.** Animations trigger only in direct response to a user action (e.g. a knob turn) and finish on their own — never a passive/looping background effect.
2. **One signature idea at a time.** Test one visual concept in isolation (a glyph, a border treatment, a shimmer) rather than stacking multiple experiments, so each can be judged cleanly and reverted independently if it doesn't land.
3. **Reuse the existing control vocabulary before inventing new ones.** Bipolar rotary knobs, the established purple/interaction-flash color language, existing disabled-state dimming — extend these before adding a new widget type or a new color system.
4. **Calm chrome, expressive core.** Buttons, borders, and labels stay quiet and visually uniform; visual energy concentrates in the one place it matters — the spectral curve display — rather than being spread evenly across the whole interface.

## Accessibility & Inclusion

- Respect `prefers-reduced-motion` for every animated effect (already implemented for the interaction-triggered shimmer).
- Never encode meaning by hue alone where a redundant cue is cheap to add — e.g. the tilt/wave balance knob's direction stays legible without color (checked explicitly this session for colorblind users), backed by a text label and readout, not hue alone.
