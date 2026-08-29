# Skeptik Audio Plugin Workflow — Skill Design

## Purpose

Skeptik Audio will ship multiple JUCE-based audio plugins beyond Refrakt (the first). Each will have a different DSP function (spectral panner, compressor, synth, etc.) but the *process* used to build Refrakt — mockup-first GUI design, WebView2-hosted production UI, DSP regression testing, cross-platform CI, always-on git deploy — should become a repeatable workflow rather than something re-derived from scratch each time.

This is captured as a personal skill, `skeptik-plugin-workflow`, invoked at `~/.claude/skills/skeptik-plugin-workflow/SKILL.md` so it's available regardless of which plugin repo is the current working directory.

## Non-goals

- Not a DSP cookbook. No plugin-specific audio math (Refrakt's `CurveMath.h` is spectral-panning-specific and does not generalize). The skill describes *how* to port and test DSP, not *what* the DSP does.
- Not a replacement for `superpowers:brainstorming`, `impeccable`, or `superpowers:test-driven-development`. The workflow skill is a thin sequencer that hands off to these at the right points rather than reimplementing their logic.

## Trigger

Skill description fires on: "new Skeptik Audio plugin", "start a Skeptik plugin", "port [mockup] to JUCE" (for an existing Skeptik Audio project), or resuming an existing Skeptik plugin repo that needs its next phase identified.

## Phases

### Phase 1 — Brand & Product doc

New plugin gets its own repo at `C:\Projects\<PluginName>\`, mirroring Refrakt's top-level layout (`Artwork/`, `Source/`, `Tests/`, `.github/workflows/`, `CMakeLists.txt`, `INSTALL.md`, `LICENSE`, `.gitignore`).

First artifact: `Artwork/PRODUCT.md`, following Refrakt's skeleton — Register, Users, Product Purpose, Brand Personality, Anti-references, Design Principles, Accessibility & Inclusion. This document is the durable brand contract later phases check visual/interaction decisions against.

The skill does not dictate PRODUCT.md's content — it hands off to `superpowers:brainstorming` to fill it in through dialogue with the user, since brand personality is a per-plugin creative decision.

### Phase 2 — Mockup design

Single self-contained HTML/CSS/JS file in `Artwork/`, no build tooling, opened directly from disk in a browser. The skill hands off to the `impeccable` skill for the actual visual/UX iteration.

Carried-over hard rule from Refrakt: **never declare a visual or alignment fix "done" on code/math reasoning alone.** A browser open or composite-image pixel check is required before reporting completion — this was an explicit, repeated practice on Refrakt (see `Artwork/Refrakt_handoff.md` §9) and is written into the skill as a non-negotiable gate, not a suggestion.

Asset-handling rule, also carried over: rename any user-supplied asset file to `underscore_case` immediately via Bash before referencing it — filenames with spaces or parentheses caused silent `<img>` load failures in-browser on Refrakt.

### Phase 3 — JUCE scaffold

Repo-layout and CMake snippets, parameterized with `{{PLUGIN_NAME}}` (not hardcoded to Refrakt, since future plugins differ in function).

Key architectural pattern to carry forward: **the GUI is not reimplemented as native JUCE components.** The finished mockup HTML is copied to `Source/ui/public/index.html` and hosted live via JUCE's WebView2 integration, loaded via `file://` during development so UI edits are visible without a C++ rebuild. Before shipping, this switches to an embedded/BinaryData resource provider (Refrakt's `PluginEditor.cpp` already has this fallback path commented in, ready to uncomment).

Parameter sync pattern: one array of param IDs (`kSyncedParamIDs` in Refrakt) drives both the outgoing APVTS→JS sync and the incoming JS→APVTS `setState` handler, so the two directions cannot drift out of sync with each other. The skill includes this as a generic template — the actual param list is plugin-specific.

WebView2 gotchas to carry forward as documented pitfalls:
- WebView2 needs an explicit writable user-data folder (defaults to a location relative to the *host* DAW executable otherwise, which often isn't writable).
- The WebView2 SDK isn't preinstalled on GitHub Actions Windows runners — CI must fetch it via NuGet directly (see Phase 5).

### Phase 4 — DSP port + regression tests

The mockup's JS math (whatever it is — this is the part that's genuinely different per plugin) gets ported to C++. This phase hands off to `superpowers:test-driven-development`: tests are written against the ported math before/alongside the port, mirroring Refrakt's `Tests/CurveMathTests.cpp` and `Tests/AudioProcessingTests.cpp` structure (a pure-math unit-test file plus an audio-processing/integration-test file, run via CTest).

### Phase 5 — CI

`.github/workflows/build.yml` template included in the skill, parameterized with `{{PLUGIN_NAME}}`: Windows + macOS matrix, WebView2 SDK fetched from NuGet on the Windows leg (not needed on macOS), `cmake --build`, `ctest --output-on-failure` (the DSP regression gate — build succeeding is not sufficient, tests must pass), VST3 artifact upload on both platforms plus AU upload on macOS.

### Phase 6 — Git convention

Every change is committed and pushed automatically, without asking — this carries forward the existing `skeptik-auto-deploy` personal-memory rule, but is written directly into the skill body (not left solely in memory) so the convention travels with the skill to any session or environment, including ones where the memory file isn't loaded.

## Skill file structure

```
~/.claude/skills/skeptik-plugin-workflow/
  SKILL.md              — phase sequencer, hands off to other skills, hard-gate rules
  templates/
    CMakeLists.txt.tmpl
    build.yml.tmpl
    webview-host-snippet.cpp.tmpl
    param-sync-snippet.cpp.tmpl
    PRODUCT.md.tmpl
```

Templates use `{{PLUGIN_NAME}}` placeholders. SKILL.md references them by relative path and instructs copying + substituting rather than inlining large code blocks into the SKILL.md body itself (keeps the main skill file readable).

## Open items resolved during brainstorming

- Scope: full pipeline (brand doc through CI/deploy), not a narrower slice.
- Audience: Skeptik Audio plugins specifically, not a brand-agnostic JUCE skill.
- Structure: single sequencer skill delegating to existing skills per phase, not split into multiple phase-skills and not a passive checklist.
- Detail level: includes concrete, adaptable snippets/templates, not prose-only pointers to Refrakt.
- Genericity: DSP content is explicitly out of scope for the skill's *content* (varies per plugin) — only the *process* of porting + testing it is standardized.
