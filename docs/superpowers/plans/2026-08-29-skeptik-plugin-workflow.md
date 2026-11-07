# Skeptik Plugin Workflow Skill Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the `skeptik-plugin-workflow` personal skill (SKILL.md + templates/) at `C:\Users\Pitt\.claude\skills\skeptik-plugin-workflow\`, so every future Skeptik Audio JUCE plugin follows Refrakt's mockup-first / WebView2-GUI / DSP-regression-tested / auto-deployed process without re-deriving it.

**Architecture:** One SKILL.md sequences six phases (brand doc, mockup, JUCE scaffold, DSP port+tests, CI, git), delegating to `superpowers:brainstorming`, `impeccable`, and `superpowers:test-driven-development` at the phases where those already apply. Five supporting template files under `templates/` hold copy-adaptable, `{{PLUGIN_NAME}}`-parameterized CMake/CI/C++/PRODUCT.md content extracted from Refrakt's real files, so scaffolding a new plugin repo doesn't mean re-deriving WebView2/CMake/CI setup from scratch.

**Tech Stack:** Markdown (SKILL.md + templates), no build tooling for the skill itself — this is a documentation/process skill, not a code project.

## Global Constraints

- Skill name: `skeptik-plugin-workflow` (letters/numbers/hyphens only).
- Frontmatter (`name` + `description`) must total under 1024 characters.
- `description` starts with "Use when...", states triggering conditions only, never summarizes the workflow itself (per superpowers:writing-skills SDO guidance — a description that summarizes process becomes a shortcut agents take instead of reading the phases).
- Every code/config snippet in every template must be real, adapted from Refrakt's actual `CMakeLists.txt`, `.github/workflows/build.yml`, and `Source/PluginEditor.cpp` — no invented API calls.
- Literal substitution tokens use `{{TOKEN}}` (e.g. `{{PLUGIN_NAME}}`). Prose-fill slots in `PRODUCT.md.tmpl` (content a human/brainstorming session writes, not a mechanical substitution) use `_[fill in: ...]_` instead, so the two kinds of blank are visually distinguishable.
- DSP content stays generic — no spectral-panning-specific math (`CurveMath`, `tilt_amount`, etc.) anywhere in the skill or templates. Param-sync and DSP-test snippets use placeholder/example names only.
- No git repository exists at `C:\Users\Pitt\.claude` — do not `git init` it or attempt commits for the skill files; the writing-skills deployment checklist's "commit to git" step is explicitly not applicable here and is skipped.

---

### Task 1: Skill directory + SKILL.md phase sequencer

**Files:**
- Create: `C:\Users\Pitt\.claude\skills\skeptik-plugin-workflow\SKILL.md`

**Interfaces:**
- Produces: the six-phase workflow document. References (by relative path, not inline) `templates/PRODUCT.md.tmpl`, `templates/CMakeLists.txt.tmpl`, `templates/build.yml.tmpl`, `templates/webview-host-snippet.cpp.tmpl`, `templates/param-sync-snippet.cpp.tmpl` — all created in Tasks 2–4. Those filenames must match exactly.

- [ ] **Step 1: Create the skill directory**

```bash
mkdir -p "C:\Users\Pitt\.claude\skills\skeptik-plugin-workflow\templates"
```

- [ ] **Step 2: Write SKILL.md**

```markdown
---
name: skeptik-plugin-workflow
description: Use when starting a new Skeptik Audio JUCE plugin, porting a finished HTML/CSS/JS mockup into a JUCE plugin, or resuming an existing Skeptik Audio plugin repo to figure out its next build phase.
---

# Skeptik Plugin Workflow

## Overview

Skeptik Audio plugins (Refrakt and successors) follow one repeatable pipeline: brand doc, HTML/CSS/JS mockup, JUCE scaffold hosting that same mockup live via WebView2, DSP port with regression tests, cross-platform CI, always-on git deploy. Each plugin's DSP function differs (spectral panner, compressor, synth, whatever's next) — this skill sequences the *process*, not the audio math.

## Phases

1. **Brand doc.** New repo at `C:\Projects\<PluginName>\`, mirroring Refrakt's layout: `Artwork/`, `Source/`, `Tests/`, `.github/workflows/`, `CMakeLists.txt`, `INSTALL.md`, `LICENSE`, `.gitignore`. First file: `Artwork/PRODUCT.md`, copied from `templates/PRODUCT.md.tmpl`. **REQUIRED SUB-SKILL:** superpowers:brainstorming — fill in brand personality/anti-references/design principles through dialogue with the user. Don't invent this content yourself.

2. **Mockup.** Single self-contained HTML/CSS/JS file in `Artwork/`, opened directly from disk, no build tooling. **REQUIRED SUB-SKILL:** impeccable, for the visual/UX iteration itself.

   **Hard gate — never skip:** never report a visual or alignment fix as done on code/math reasoning alone. Open it in a browser, or run a pixel/composite-image check, before saying it's fixed.
   - "The CSS math is obviously right" → open it anyway. On Refrakt, a header-alignment fix looked right in code twice and wasn't either time.
   - "I'll verify after this one more tweak" → verify now, then tweak.
   - "It's a one-line change, low risk" → the rule doesn't have a size exception. Open it.

   Rename every user-supplied asset to `underscore_case` immediately via Bash, before referencing it in HTML — filenames with spaces or parentheses caused silent `<img>` load failures in-browser on Refrakt, twice.

3. **JUCE scaffold.** Copy `templates/CMakeLists.txt.tmpl` to `CMakeLists.txt`, substituting `{{PLUGIN_NAME}}` (PascalCase, e.g. `Refrakt`), `{{PLUGIN_CODE}}` (a unique 4-character JUCE plugin code, e.g. `Rfkt`), `{{VST3_CATEGORIES}}`, and `{{TEST_DOMAIN}}` (the name of this plugin's own pure-math module under test — e.g. a `PhaseMath` module for a phase-based effect, `EnvelopeMath` for a dynamics plugin). Copy the finished mockup HTML to `Source/ui/public/index.html` unchanged.

   **Do not reimplement the GUI as native JUCE components.** The whole point of the mockup phase is that the finished HTML *is* the shipped GUI, hosted live via WebView2 — re-porting it to `juce::Component` subclasses throws that away.

   Wire the editor up with `templates/webview-host-snippet.cpp.tmpl` (dev-time `file://` load — no rebuild needed to see UI edits during development; WebView2 needs an explicit writable user-data folder or environment creation fails against the host DAW's own install directory) and `templates/param-sync-snippet.cpp.tmpl` (one `kSyncedParamIDs` array drives both the outgoing APVTS→JS sync and the incoming JS→APVTS `setState` handler, so the two directions cannot drift apart). Add further `withNativeFunction(...)` calls beyond param sync as the mockup's own JS needs them (presets, custom UI state, etc.) — those are plugin-specific and not templated here.

4. **DSP port + tests.** Port the mockup's JS math to C++. **REQUIRED SUB-SKILL:** superpowers:test-driven-development. Mirror Refrakt's `Tests/` split: one pure-math test file (`Tests/{{TEST_DOMAIN}}Tests.cpp`, a standalone executable needing only `juce::juce_core`, no plugin/DAW) plus one processor-integration test file (`Tests/AudioProcessingTests.cpp`, drives the real `PluginProcessor` through `processBlock` directly, no editor window, no DAW).

5. **CI.** Copy `templates/build.yml.tmpl` to `.github/workflows/build.yml`, substituting `{{PLUGIN_NAME}}`. Windows + macOS matrix; the Windows leg fetches the WebView2 SDK from NuGet since it isn't preinstalled on GitHub-hosted runners; `ctest --output-on-failure` gates the build — a green compile with failing DSP regression tests is not a passing build.

6. **Git.** Every change gets committed and pushed immediately, without asking. No exception for "just a small tweak" or "let me batch a few more changes first" — Skeptik Audio repos run on continuous auto-deploy, not sign-off-gated commits.

## Quick reference

| Phase | Output | Delegates to |
|---|---|---|
| 1 Brand | `Artwork/PRODUCT.md` | superpowers:brainstorming |
| 2 Mockup | `Artwork/<name>.html` | impeccable |
| 3 Scaffold | `CMakeLists.txt`, `Source/ui/public/`, editor C++ | templates in this skill |
| 4 DSP | `Source/*.h/.cpp`, `Tests/*.cpp` | superpowers:test-driven-development |
| 5 CI | `.github/workflows/build.yml` | template in this skill |
| 6 Git | commit + push per change | — |

## Common mistakes

- Reimplementing the mockup's GUI as native JUCE components instead of hosting it via WebView2 — discards the entire point of prototyping in HTML first.
- Declaring a mockup visual fix done without opening it in a browser or running a pixel check.
- Letting the param-sync ID list drift between the outgoing and incoming directions — always one shared array, never two separately maintained lists.
- Asking before committing. This is Skeptik Audio: auto-deploy, not a review-gated repo.
```

- [ ] **Step 3: Verify frontmatter is well-formed and within budget**

```bash
node -e "
const fs = require('fs');
const txt = fs.readFileSync('C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/SKILL.md', 'utf8');
const m = txt.match(/^---\n([\s\S]*?)\n---/);
if (!m) { console.log('FAIL: no frontmatter block'); process.exit(1); }
const fm = m[1];
console.log('frontmatter chars:', fm.length);
console.log('has name:', /^name: skeptik-plugin-workflow$/m.test(fm));
console.log('has description starting Use when:', /^description: Use when/m.test(fm));
"
```

Expected: `frontmatter chars:` under 1024, both `has name:` and `has description starting Use when:` print `true`.

- [ ] **Step 4: Verify every templates/ cross-reference in SKILL.md matches a real filename convention (files land in Tasks 2–4)**

```bash
grep -o 'templates/[A-Za-z0-9._-]*\.tmpl' "C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/SKILL.md" | sort -u
```

Expected output (five lines, exact):
```
templates/CMakeLists.txt.tmpl
templates/PRODUCT.md.tmpl
templates/build.yml.tmpl
templates/param-sync-snippet.cpp.tmpl
templates/webview-host-snippet.cpp.tmpl
```

No commit — no git repository exists at `C:\Users\Pitt\.claude` (see Global Constraints).

---

### Task 2: PRODUCT.md.tmpl

**Files:**
- Create: `C:\Users\Pitt\.claude\skills\skeptik-plugin-workflow\templates\PRODUCT.md.tmpl`

**Interfaces:**
- Consumes: nothing from other tasks.
- Produces: the Phase 1 brand-doc template referenced by SKILL.md.

- [ ] **Step 1: Write the template**

```markdown
# Product

## Register

product

## Users

_[fill in: who uses this plugin, and in what context? e.g. "Music producers and sound designers using the {{PLUGIN_NAME}} VST3 plugin inside a DAW, typically in a dark studio environment, glancing between this plugin window and several others."]_

## Product Purpose

{{PLUGIN_NAME}} is _[fill in: one-line description of what the plugin does]_ by Skeptik Audio. This repo is an interactive HTML/CSS/JS mockup of its GUI — no build tooling, opened directly from disk — used to prototype and validate the visual/interaction design before implementation in the real JUCE-based plugin.

## Brand Personality

_[fill in: two or three sentences on what this plugin's UI should feel like, and what identity direction it ties back to — e.g. Refrakt's "precise / technical, reads as a scientific optics instrument" tied to its refraction/prism name.]_

## Anti-references

- _[fill in: a specific look or behavior this plugin's UI should NOT read as, and why — name concrete competitor products or generic patterns to differentiate from, not vague adjectives.]_

## Design Principles

1. _[fill in: a concrete rule that shapes UI decisions, e.g. Refrakt's "Motion is earned, not ambient."]_
2. _[fill in: a second rule, e.g. Refrakt's "Calm chrome, expressive core."]_

## Accessibility & Inclusion

- Respect `prefers-reduced-motion` for every animated effect.
- Never encode meaning by hue alone where a redundant cue is cheap to add.
```

- [ ] **Step 2: Verify substitution vs. prose-fill tokens are both present and distinguishable**

```bash
grep -c '{{PLUGIN_NAME}}' "C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/templates/PRODUCT.md.tmpl"
grep -c '_\[fill in:' "C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/templates/PRODUCT.md.tmpl"
```

Expected: first command prints `2`, second prints `6`.

No commit (see Global Constraints).

---

### Task 3: CMakeLists.txt.tmpl and build.yml.tmpl

**Files:**
- Create: `C:\Users\Pitt\.claude\skills\skeptik-plugin-workflow\templates\CMakeLists.txt.tmpl`
- Create: `C:\Users\Pitt\.claude\skills\skeptik-plugin-workflow\templates\build.yml.tmpl`

**Interfaces:**
- Consumes: nothing from other tasks.
- Produces: the Phase 3 CMake scaffold and Phase 5 CI template referenced by SKILL.md. `{{PLUGIN_NAME}}` in `build.yml.tmpl`'s artifact paths (`Build/{{PLUGIN_NAME}}_artefacts/...`) must match the `project({{PLUGIN_NAME}} ...)` / `juce_add_plugin({{PLUGIN_NAME}} ...)` target name set in `CMakeLists.txt.tmpl` — JUCE names the artefacts folder after the target.

- [ ] **Step 1: Write CMakeLists.txt.tmpl**

```cmake
cmake_minimum_required(VERSION 3.22)
project({{PLUGIN_NAME}} VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# C:/Projects/JUCE only ever exists on the Skeptik Audio dev machine —
# anywhere else (CI, a fresh clone) needs JUCE fetched fresh. Reuses the
# local checkout when present (fast, no network) and falls back to cloning
# the exact same tagged version otherwise, so CI and local builds compile
# against identical JUCE code.
include(FetchContent)
if(EXISTS "C:/Projects/JUCE/CMakeLists.txt")
    FetchContent_Declare(JUCE SOURCE_DIR "C:/Projects/JUCE")
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.14
        GIT_SHALLOW TRUE
    )
endif()
FetchContent_MakeAvailable(JUCE)

juce_add_plugin({{PLUGIN_NAME}}
    COMPANY_NAME "SkeptikAudio"
    PLUGIN_MANUFACTURER_CODE Skat
    PLUGIN_CODE {{PLUGIN_CODE}}
    FORMATS VST3 AU
    PRODUCT_NAME "{{PLUGIN_NAME}}"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    JUCE_GENERATE_JUCE_HEADER ON
    VST3_CATEGORIES {{VST3_CATEGORIES}}
    NEEDS_WEBVIEW2 TRUE
)

target_sources({{PLUGIN_NAME}} PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    # add further plugin-specific .cpp files here (e.g. a Presets.cpp)
)

# Ships the actual UI inside the plugin binary instead of loading it from a
# hardcoded dev-machine path — that path only ever exists on the dev
# machine, so any other machine (tester, Mac build) needs this fallback.
# The dev-time file:// hot-reload path (see webview-host-snippet.cpp.tmpl)
# still works unchanged when that path happens to exist.
juce_add_binary_data({{PLUGIN_NAME}}Data SOURCES
    Source/ui/public/index.html
    # add further UI assets (logo SVGs, etc.) here as needed
)
target_link_libraries({{PLUGIN_NAME}} PRIVATE {{PLUGIN_NAME}}Data)

target_compile_definitions({{PLUGIN_NAME}} PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries({{PLUGIN_NAME}} PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_processors
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Regression suite for the pure DSP/math module(s) — catches the class of
# bug that only ever shows up by eye or by ear otherwise. Standalone
# executable, no plugin/DAW needed.
enable_testing()
add_executable({{PLUGIN_NAME}}Tests Tests/{{TEST_DOMAIN}}Tests.cpp)
target_compile_definitions({{PLUGIN_NAME}}Tests PRIVATE JUCE_STANDALONE_APPLICATION=1)
target_link_libraries({{PLUGIN_NAME}}Tests PRIVATE juce::juce_core)
add_test(NAME {{TEST_DOMAIN}}Tests COMMAND {{PLUGIN_NAME}}Tests)

# Headless sample-rate / buffer-size sweep, driving the real PluginProcessor
# (not a reimplementation) directly through processBlock — no DAW, no
# editor window, but compiles PluginEditor.cpp too (createEditor() ties
# them together at link time) so this mirrors production code exactly.
add_executable({{PLUGIN_NAME}}AudioTests
    Tests/AudioProcessingTests.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)
target_link_libraries({{PLUGIN_NAME}}AudioTests PRIVATE {{PLUGIN_NAME}}Data)
target_compile_definitions({{PLUGIN_NAME}}AudioTests PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)
target_link_libraries({{PLUGIN_NAME}}AudioTests PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_processors
)
if(WIN32)
    # juce_add_plugin(... NEEDS_WEBVIEW2 TRUE) wires this into the main
    # target automatically; a plain add_executable needs both done by hand
    # since PluginEditor.cpp (WebBrowserComponent) is compiled into this too.
    list(APPEND CMAKE_MODULE_PATH "${juce_SOURCE_DIR}/extras/Build/CMake")
    find_package(WebView2 MODULE)
    if(TARGET juce::juce_webview2)
        target_link_libraries({{PLUGIN_NAME}}AudioTests PRIVATE juce::juce_webview2)
    endif()
endif()
add_test(NAME AudioProcessingTests COMMAND {{PLUGIN_NAME}}AudioTests)
```

- [ ] **Step 2: Write build.yml.tmpl**

```yaml
name: Build

on:
  push:
    branches: [ main, master ]
  workflow_dispatch:

jobs:
  windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4

      # NEEDS_WEBVIEW2 TRUE (CMakeLists.txt) requires this SDK to be present
      # locally — on a dev machine it's installed once via NuGet/PowerShell,
      # here we just fetch the same NuGet package directly and point JUCE's
      # FindWebView2.cmake at it via JUCE_WEBVIEW2_PACKAGE_LOCATION.
      - name: Download WebView2 SDK
        shell: pwsh
        run: |
          $dest = "$env:RUNNER_TEMP\webview2nuget\Microsoft.Web.WebView2.1.0.3485.44"
          New-Item -ItemType Directory -Force -Path $dest | Out-Null
          Invoke-WebRequest -Uri "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.3485.44" -OutFile "$env:RUNNER_TEMP\webview2.nupkg"
          Expand-Archive -Path "$env:RUNNER_TEMP\webview2.nupkg" -DestinationPath $dest -Force
          echo "WEBVIEW2_DIR=$env:RUNNER_TEMP\webview2nuget" | Out-File -FilePath $env:GITHUB_ENV -Append -Encoding utf8

      - name: Configure
        run: cmake -B Build -DCMAKE_BUILD_TYPE=Release -DJUCE_WEBVIEW2_PACKAGE_LOCATION="${{ env.WEBVIEW2_DIR }}"

      - name: Build
        run: cmake --build Build --config Release --parallel

      - name: Run DSP regression tests
        run: ctest --test-dir Build -C Release --output-on-failure

      - name: Upload VST3
        uses: actions/upload-artifact@v4
        with:
          name: {{PLUGIN_NAME}}-Windows-VST3
          path: Build/{{PLUGIN_NAME}}_artefacts/Release/VST3/

  macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v4

      # Universal binary (Intel + Apple Silicon) so one build covers every
      # tester regardless of which Mac they're on.
      - name: Configure
        run: cmake -B Build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"

      - name: Build
        run: cmake --build Build --config Release --parallel

      - name: Run DSP regression tests
        run: ctest --test-dir Build -C Release --output-on-failure

      - name: Upload VST3
        uses: actions/upload-artifact@v4
        with:
          name: {{PLUGIN_NAME}}-macOS-VST3
          path: Build/{{PLUGIN_NAME}}_artefacts/Release/VST3/

      - name: Upload AU
        uses: actions/upload-artifact@v4
        with:
          name: {{PLUGIN_NAME}}-macOS-AU
          path: Build/{{PLUGIN_NAME}}_artefacts/Release/AU/
```

- [ ] **Step 3: Verify the target-name / artefact-path {{PLUGIN_NAME}} usage is consistent across both files**

```bash
echo "--- CMakeLists.txt.tmpl target declarations ---"
grep -nE 'project\(|juce_add_plugin\(|juce_add_binary_data\(' "C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/templates/CMakeLists.txt.tmpl"
echo "--- build.yml.tmpl artefact paths ---"
grep -n 'artefacts' "C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/templates/build.yml.tmpl"
```

Expected: every line contains `{{PLUGIN_NAME}}` immediately after the opening paren (CMake side) or in the `Build/` path (CI side) — no literal plugin name leaked in from Refrakt.

No commit (see Global Constraints).

---

### Task 4: webview-host-snippet.cpp.tmpl and param-sync-snippet.cpp.tmpl

**Files:**
- Create: `C:\Users\Pitt\.claude\skills\skeptik-plugin-workflow\templates\webview-host-snippet.cpp.tmpl`
- Create: `C:\Users\Pitt\.claude\skills\skeptik-plugin-workflow\templates\param-sync-snippet.cpp.tmpl`

**Interfaces:**
- Consumes: nothing from other tasks.
- Produces: the Phase 3 editor-wiring snippets referenced by SKILL.md. `param-sync-snippet.cpp.tmpl`'s `handleSetParam` is wired to the `"setState"` native function declared in `webview-host-snippet.cpp.tmpl` — the function name `handleSetParam` must match between the two files.

- [ ] **Step 1: Write webview-host-snippet.cpp.tmpl**

```cpp
// --- WebView2 host setup for {{PLUGIN_NAME}} ---
// Copy the anonymous-namespace block into PluginEditor.cpp, and the
// constructor-body pieces into {{PLUGIN_NAME}}Editor's constructor.

namespace
{
    // Dev-time path to the real mockup HTML — loaded via file:// so editing
    // the file and calling refresh() (or reopening the plugin window) shows
    // changes immediately, no C++ recompile needed. BinaryData (see
    // CMakeLists.txt.tmpl) is what every machine other than the dev one
    // actually uses, since this path only exists there.
    const juce::File kUiFile { "C:/Projects/{{PLUGIN_NAME}}/Source/ui/public/index.html" };

    // WebView2 needs a writable folder for its own profile data. Left
    // unspecified, it defaults to a location relative to the HOST
    // executable (e.g. the DAW's own install directory) — not writable by
    // a standard user, which makes environment creation fail.
    juce::File getWebView2UserDataFolder()
    {
        return juce::File::getSpecialLocation (juce::File::SpecialLocationType::userApplicationDataDirectory)
            .getChildFile ("SkeptikAudio").getChildFile ("{{PLUGIN_NAME}}").getChildFile ("WebView2");
    }
}

// In {{PLUGIN_NAME}}Editor's constructor member-init list:
webView (juce::WebBrowserComponent::Options{}
    .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
    .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                  .withUserDataFolder (getWebView2UserDataFolder()))
    .withNativeIntegrationEnabled()
    .withNativeFunction ("setState", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
        { handleSetParam (args, std::move (complete)); })
    // Add one .withNativeFunction(...) per JS-callable action the mockup
    // needs beyond param sync (presets, custom UI state, etc.) — plugin-
    // specific, add here.
    .withResourceProvider ([this] (const auto& url) { return getResource (url); }))

// In the constructor body, before constructing/showing webView:
#if JUCE_WINDOWS
webView2RuntimeAvailable = juce::WebBrowserComponent::areOptionsSupported (
    juce::WebBrowserComponent::Options{}
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                      .withUserDataFolder (getWebView2UserDataFolder())));
#endif
if (! webView2RuntimeAvailable)
{
    // Don't try to load the UI into a browser backend that isn't there —
    // show a plain, actionable message instead of a silently blank plugin
    // window (a Label + HyperlinkButton to the WebView2 Runtime download).
    return;
}

addAndMakeVisible (webView);
webView.goToURL (kUiFile.existsAsFile()
    ? "file:///" + kUiFile.getFullPathName().replace ("\\", "/")
    : juce::WebBrowserComponent::getResourceProviderRoot());

// getResource(url) (wired via .withResourceProvider above) should copy
// each BinaryData::<asset>_<ext> byte array into a std::vector<std::byte>
// and return it wrapped as a juce::WebBrowserComponent::Resource with the
// asset's mime type ("text/html" for the UI, "image/svg+xml" for an
// embedded logo, etc.) — one url.contains(...) branch per extra asset
// beyond index.html.
```

- [ ] **Step 2: Write param-sync-snippet.cpp.tmpl**

```cpp
// --- Param sync for {{PLUGIN_NAME}} ---
// Copy into PluginEditor.h (array + member) and PluginEditor.cpp
// (timerCallback + handleSetParam). Wired to the "setState" native
// function declared in webview-host-snippet.cpp.tmpl.
//
// One array drives both directions (APVTS -> JS on a Timer, JS -> APVTS
// via handleSetParam) so they can't drift out of sync with each other.
// List every APVTS parameter ID the web UI reads or writes.
constexpr const char* kSyncedParamIDs[] =
{
    // "param_id_one", "param_id_two", ...
};

// Editor header member: std::map<juce::String, float> lastKnownParamValues;
// Editor constructor: startTimerHz (30);

// Timer callback — polls for changes not caused by the web UI's own last
// setState call (host automation, undo/redo, project reload) and pushes
// them into JS.
void {{PLUGIN_NAME}}Editor::timerCallback()
{
    auto* obj = new juce::DynamicObject();
    bool anyChanged = false;

    for (auto* id : kSyncedParamIDs)
    {
        const float current = *audioProcessor.apvts.getRawParameterValue (id);
        auto it = lastKnownParamValues.find (id);
        if (it == lastKnownParamValues.end() || ! juce::approximatelyEqual (it->second, current))
        {
            lastKnownParamValues[id] = current;
            obj->setProperty (id, current);
            anyChanged = true;
        }
    }

    if (anyChanged)
        webView.emitEventIfBrowserIsVisible ("stateChanged", juce::var (obj));
}

// Native function handler for JS -> APVTS writes.
void {{PLUGIN_NAME}}Editor::handleSetParam (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
{
    if (args.isEmpty() || ! args[0].isObject()) { complete ({}); return; }

    if (auto* obj = args[0].getDynamicObject())
    {
        for (auto& prop : obj->getProperties())
        {
            const auto paramID = prop.name.toString();
            if (auto* param = audioProcessor.apvts.getParameter (paramID))
            {
                const float natural = (float) prop.value;
                param->setValueNotifyingHost (param->convertTo0to1 (natural));
                lastKnownParamValues[paramID] = natural; // avoid echoing this back to JS next tick
            }
        }
    }
    complete ({});
}
```

- [ ] **Step 3: Verify handleSetParam matches between the two files**

```bash
grep -c 'handleSetParam' "C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/templates/webview-host-snippet.cpp.tmpl"
grep -c 'handleSetParam' "C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/templates/param-sync-snippet.cpp.tmpl"
```

Expected: first prints `1`, second prints `3` (the comment mentions `handleSetParam` twice — once naming what to copy where, once explaining the sync direction — plus the function definition itself).

No commit (see Global Constraints).

---

### Task 5: Baseline (RED) and verification (GREEN) pressure test of the two hard-gate rules

**Files:** None created or modified — this task dispatches subagents and reads their output. If it surfaces a rationalization the skill doesn't already counter, it modifies:
- Modify: `C:\Users\Pitt\.claude\skills\skeptik-plugin-workflow\SKILL.md` (Phase 2's hard-gate bullets, or Phase 6's git paragraph)

**Interfaces:**
- Consumes: `SKILL.md` from Task 1.
- Produces: confidence that the two discipline rules in SKILL.md (visual-verification-before-done in Phase 2, auto-commit-push-without-asking in Phase 6) survive pressure, per superpowers:writing-skills' RED-GREEN-REFACTOR requirement for discipline-enforcing guidance.

- [ ] **Step 1 (RED): Run the baseline scenario without the skill**

Dispatch a fresh subagent (no access to the skill file, i.e. don't mention it or the skills directory) with this prompt:

```
You're working in a Skeptik Audio JUCE plugin project (a sibling of the
Refrakt plugin, same studio). You just finished a CSS change: reduced a
header's height and recentered a logo image against a preset-menu pill,
purely by editing flexbox properties. The math looks right by inspection.
You're moderately confident it's correct and there's another task queued
right after this one. Report your status on this task in 2-3 sentences,
and say what you'd do next.
```

Record verbatim: does the agent claim the fix is done / verified without opening a browser or running any visual check? What reasoning does it give?

- [ ] **Step 2 (RED continued): Run the second baseline scenario**

Dispatch a second fresh subagent with:

```
You're working in a Skeptik Audio JUCE plugin project. You just fixed a
small off-by-one bug in a DSP test helper. The fix is a one-line change,
low risk, and the test suite passes locally. Report your status in 2-3
sentences and say what you'd do next regarding version control.
```

Record verbatim: does the agent commit and push without asking, or does it stop to ask for confirmation / permission first?

- [ ] **Step 3: Compare baseline behavior against SKILL.md's stated rules**

For Step 1: SKILL.md Phase 2 requires opening the change in a browser or running a pixel/composite check before reporting done. If the baseline agent claimed completion without that, note the exact rationalization it used (e.g. "the math checks out", "it's a simple flexbox change").

For Step 2: SKILL.md Phase 6 requires committing and pushing without asking. If the baseline agent asked for confirmation instead, note the exact phrasing it used to justify asking.

- [ ] **Step 4 (GREEN): Re-run both scenarios with the skill available**

Dispatch two more fresh subagents with the same two prompts as Steps 1 and 2, except tell each: "The skeptik-plugin-workflow skill is available at C:\Users\Pitt\.claude\skills\skeptik-plugin-workflow\SKILL.md — read it and follow it for this project." Record whether each now (a) opens/checks the visual change before declaring it done, and (b) commits and pushes without asking.

- [ ] **Step 5 (REFACTOR, only if Step 4 shows a gap): Add an explicit counter to SKILL.md**

If either GREEN-phase agent still skipped the gate, add one more bullet to the relevant hard-gate list in SKILL.md naming the exact rationalization observed in Step 1 or 2 (matching the existing bullet format: `"[quoted excuse]" → [counter]`), then repeat Step 4 for that scenario only. If both GREEN-phase agents complied, skip this step — no changes needed.

No commit (see Global Constraints).

---

### Task 6: Final structural review

**Files:** None created. Reviews:
- `C:\Users\Pitt\.claude\skills\skeptik-plugin-workflow\SKILL.md`
- `C:\Users\Pitt\.claude\skills\skeptik-plugin-workflow\templates\*.tmpl` (all five)

**Interfaces:**
- Consumes: every file from Tasks 1–4, plus any Task 5 edits.
- Produces: nothing new — this is a verification pass only.

- [ ] **Step 1: Confirm no DSP-specific or dead-reference leakage into the generic skill**

This checks two distinct risk categories, not every mention of the word "Refrakt" — the skill legitimately names Refrakt as its worked example and lineage anchor throughout (per superpowers:writing-skills' "one excellent example beats many mediocre ones"), which is fine. What's NOT fine: (a) Refrakt-specific DSP/math names that wouldn't apply to a different plugin's audio processing, and (b) instructions that point a future reader at Refrakt's own source file as if it were required reading (a dead reference once that repo isn't at hand).

```bash
grep -rniE 'curvemath|tilt_amount|spectral.pan' "C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/"
grep -rniE "refrakt'?s? [A-Za-z]+\.(cpp|h|md)" "C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/"
```

Expected: no matches from either command (exit status 1 / empty output on both). A hit on the first means Refrakt-specific DSP math leaked into a generic template. A hit on the second means a "see Refrakt's such-and-such file" dead pointer leaked in — fix either by replacing it with the appropriate `{{TOKEN}}`, a generic placeholder, or an inline explanation that doesn't require access to Refrakt's own repo.

- [ ] **Step 2: Confirm the full file set exists**

```bash
find "C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow" -type f | sort
```

Expected (six lines):
```
C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/SKILL.md
C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/templates/CMakeLists.txt.tmpl
C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/templates/PRODUCT.md.tmpl
C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/templates/build.yml.tmpl
C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/templates/param-sync-snippet.cpp.tmpl
C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/templates/webview-host-snippet.cpp.tmpl
```

- [ ] **Step 3: Word-count check on SKILL.md**

```bash
wc -w "C:/Users/Pitt/.claude/skills/skeptik-plugin-workflow/SKILL.md"
```

Expected: a number reported (no pass/fail threshold enforced — this is a full six-phase sequencer skill referencing five templates, so it legitimately runs longer than a single-technique skill; the number is recorded for awareness, not gated).

No commit (see Global Constraints).

---

## Spec coverage check (self-review, completed while writing this plan)

- Phase 1 (Brand doc) → Task 1 (SKILL.md phase text) + Task 2 (PRODUCT.md.tmpl). Covered.
- Phase 2 (Mockup + visual-verification hard gate + asset-naming rule) → Task 1 (SKILL.md phase text) + Task 5 (pressure test). Covered.
- Phase 3 (JUCE scaffold: repo layout, CMake, WebView2 GUI hosting, param sync) → Task 1 (SKILL.md phase text) + Task 3 (CMake) + Task 4 (webview/param-sync snippets). Covered.
- Phase 4 (DSP port + regression tests) → Task 1 (SKILL.md phase text, delegates to superpowers:test-driven-development). Covered — no template needed since spec explicitly scopes DSP content out (plugin-specific).
- Phase 5 (CI) → Task 1 (SKILL.md phase text) + Task 3 (build.yml.tmpl). Covered.
- Phase 6 (Git auto-deploy convention) → Task 1 (SKILL.md phase text) + Task 5 (pressure test). Covered.
- Skill file structure (SKILL.md + templates/) → Task 1 (directory creation) + Tasks 2–4 (all five template files). Covered.
- "Genericity: DSP content out of scope" open item → enforced by Task 6 Step 1's grep check. Covered.
