# Refrakt Project — Handoff Summary

## 1. Project overview

"Refrakt" is a spectral panning VST3 plugin by **Skeptik Audio**. This repo/folder contains an interactive HTML/CSS/JS **mockup** of the plugin's GUI — no build tooling, no framework. The deliverable is opened directly from disk in a browser to preview/iterate on the visual design before it's implemented in the real plugin (JUCE or similar, not part of this repo).

Working directory: `C:\Projects\Refrakt\Artwork\`

## 2. Current objectives

The active thread of work is a **header/logo layout rework**:
- Center the logo image with the preset-menu pill (the preset menu's vertical center is the fixed alignment anchor — not negotiable, established earlier).
- Reduce the header's overall height back down, without losing legibility of the logo.
- Keep the logo artwork's internal proportions intact (no re-cropping the wordmark/subline relationship) while it shrinks.

This objective superseded an earlier, narrower task (fixing "SKEPTIK AUDIO" sub-line alignment inside the old logo asset), which had required inflating the header to 133–157px — undesirable long-term, hence this rework.

## 3. Completed work

- Removed the CSS hacks that had inflated the header to accommodate the old logo asset:
  - `.hdr` — removed `min-height:157px`
  - `.logo-wrap` — removed `align-self:flex-start`
  - Header now relies on plain `align-items:center` flexbox centering for both `.logo-wrap` and `.preset-bar`.
- Swapped the logo asset twice as the user supplied improved SVGs, each time renaming the file (see §10) and updating `.logo-mark-wrap` box + `<img>` dimensions to match the new asset's aspect ratio:
  1. `REFRAKT_header.svg` (old, viewBox 384×195, ratio ≈1.97) → dimensions were 173×88
  2. `REFRAKT_small.svg` (user-supplied, viewBox 384×149.999998, ratio ≈2.56) → dimensions recalculated to 173×68
  3. `REFRAKT_small_trans.svg` (user-supplied refinement, "spacing is perfect" per user, same viewBox as #2) → **currently active**, dimensions unchanged at 173×68 since aspect ratio matches
- Current `<img src>` in the HTML: `REFRAKT_small_trans.svg`

## 4. Important architectural decisions

- **Preset menu vertical center = the fixed anchor.** All header alignment work aligns other elements to it, never the reverse.
- **Scale the whole combined logo SVG down** rather than splitting the wordmark and "SKEPTIK AUDIO" subline into two separately-cropped SVG assets. This was the recommended approach going in, and the user-supplied assets confirm this direction (single flattened SVG per version, not two files).
- **Let flexbox do the centering.** Per-child height/offset hacks (`min-height`, `align-self:flex-start`) were a stopgap for the old asset and have been removed now that the new assets don't need them.

## 5. Current file structure (only relevant files)

```
C:\Projects\Refrakt\Artwork\
├── refrakt_v13_calmer_defaults.html   ← ACTIVE deliverable, edit this one
├── REFRAKT_small_trans.svg            ← ACTIVE logo asset (referenced by the HTML above)
├── REFRAKT_small.svg                  ← superseded, orphaned, still on disk
├── REFRAKT_header.svg                 ← superseded, orphaned, still on disk
├── REFRAKT.svg                        ← original full-size source logo, orphaned, still on disk
├── refrakt_v7_assembled.html          ← older mockup version, not in active use
└── refrakt_curve_knob_sync.html       ← older mockup version, not in active use
```

Relevant lines in `refrakt_v13_calmer_defaults.html` (line numbers approximate, re-grep if edited):
- Line 6: `.hdr{...}` header row flex container
- Line 7: `.logo-wrap{...}`
- Lines 10–11: `.logo-mark-wrap` box + `img` sizing (173×68)
- Line 90: the `<img src="REFRAKT_small_trans.svg">` tag itself

## 6. Outstanding tasks

1. **Visually verify the current state.** Open the HTML in a browser (or run a PowerShell + `System.Drawing` composite/pixel check — see §9) and confirm:
   - `REFRAKT_small_trans.svg` renders with no letterboxing/aspect distortion.
   - Logo's vertical center lines up with the preset pill's vertical center.
   - Header height reads as visibly reduced vs. the old ~157px row.
2. Get explicit user sign-off that the header now satisfies "center image with preset menu, reduce header height."
3. Decide the fate of the three orphaned SVG files (§7) — delete or keep.

## 7. Known issues or technical debt

- **No visual verification has been run yet** against the current `REFRAKT_small_trans.svg` swap. This project's established practice (see §9) is to never declare a visual/alignment fix "done" on code/math reasoning alone — a composite-image check is expected before reporting completion, and it hasn't happened for this latest swap.
- **Orphaned assets on disk:** `REFRAKT.svg`, `REFRAKT_header.svg`, `REFRAKT_small.svg` are no longer referenced by the HTML but haven't been deleted (no instruction given either way — leave them alone until the user says what to do).

## 8. Design decisions and rationale

- Anchoring on the preset menu (not the logo) keeps the header's height driven by the more information-dense, functionally important element, so the logo scales to fit rather than dictating layout.
- A single flattened logo SVG (wordmark + glow + subline baked into one asset via internal masks/`matrix()` transforms) was preferred over a two-asset split because it avoids maintaining two sets of transforms and two files in sync as the design iterates.
- Removing the `min-height`/`align-self` hacks rather than patching them further reflects a general principle applied this session: prefer removing special-casing over accumulating it, once the underlying asset no longer needs the workaround.

## 9. Important implementation details that should not be forgotten

- **Tool limitation — large single-line SVGs cannot be Read/Written directly.** The Read tool's size precheck evaluates the *entire* file regardless of `offset`/`limit` — a 500K+ token single-line (minified/base64-heavy) SVG will fail Read outright, which in turn blocks Write/Edit (both require a prior successful Read of that exact file). **Consequence:** any future logo/asset swap where the user pastes new raw SVG content must be saved to disk *by the user*, not transcribed by the assistant. The assistant's role is limited to renaming the file via Bash (no read restriction there) and editing the referencing HTML/CSS.
- The logo SVG's internal structure: two base64-encoded raster PNGs layered through `<mask>` + `<clipPath>` with `matrix()` transforms — one layer is the wordmark, one is the glow/SKEPTIK subline. This is baked into the asset itself; the mockup's CSS only sizes the outer `<img>` box, it does not manipulate the internal layers.
- Global CSS reset `*{box-sizing:border-box;margin:0;padding:0}` is in effect — remember that `min-height` on a padded flex container includes its own padding (this caused confusion earlier in the session and was the reason the old `min-height:157px` value looked arbitrary).

## 10. Coding conventions established during this session

- **Asset filenames must never contain spaces or parentheses.** Every time the user supplies a new SVG file, rename it to `underscore_case` immediately via Bash before referencing it in `<img src>` — raw pasted filenames with spaces/special characters have caused silent `<img>` load failures in-browser twice this session.
- Keep `.logo-mark-wrap` width fixed at whatever the current design width is (173px so far) and only recompute the **height** from the new asset's viewBox ratio when swapping logo SVGs — don't recompute both unless the user asks to change the footprint further.

## 11. Open questions or decisions still to be made

- Should the three orphaned SVG files be deleted, or kept as version history / fallback? No decision made yet.
- Is 173×68 (current logo box size) actually small enough to satisfy "reduce header height," or does the user want the footprint reduced further once they see it rendered? Pending visual verification.

## 12. Recommended first steps for the next session

1. Read `refrakt_v13_calmer_defaults.html` lines ~1–95 to confirm the header/logo CSS and markup match what's described in §5 above (in case it was edited after this handoff was written).
2. Perform the outstanding visual verification (§6.1) — open the file in a browser or run a PowerShell/`System.Drawing` composite check — before touching anything further or reporting the header rework as complete.
3. Report the verification result to the user and get sign-off; only then consider the orphaned-file cleanup question in §11.
