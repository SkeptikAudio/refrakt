#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// Strategy pivot (see conversation): the previous native juce::Graphics
// rebuild kept missing the mockup's actual visual language no matter how
// carefully individual values were matched. JUCE 8's WebView2 integration
// lets the real mockup HTML run inside the plugin directly — same file,
// same renderer, zero translation loss. The mockup's controls are hand-
// rolled canvas widgets (not <input> elements), so this does NOT use the
// WebSliderRelay/WebToggleButtonRelay sugar (that's for binding standard
// HTML form controls) — instead it's a small custom native-function bridge:
// JS pushes parameter changes out via a registered native function, and a
// polling Timer here detects APVTS changes (host automation, preset loads)
// and pushes them back into JS as backend events.
struct SinglePageBrowser : juce::WebBrowserComponent
{
    using WebBrowserComponent::WebBrowserComponent;

    // Keeps the view from navigating away if the page ever contains a
    // stray link — we only ever want our own local file loaded.
    bool pageAboutToLoad (const juce::String& newURL) override;
    void pageFinishedLoading (const juce::String& url) override;

    std::function<void()> onPageFinishedLoading;
};

class RefraktEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RefraktEditor (RefraktProcessor&);
    ~RefraktEditor() override;

    void resized() override;

private:
    void timerCallback() override;

    // Native functions the JS side calls (see index.html's bridge script).
    void handleSetParam (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete);
    void handleSetCustomShape (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete);
    void handleRandomise (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete);
    void handleSetScale (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete);
    void handleSaveUserPreset (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete);
    void handleDeleteUserPreset (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete);
    void handleReportContentSize (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete);
    void handleSetSoloTarget (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete);

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    // Confirmed empirically, twice, at two different requested sizes: the
    // page's own window.innerWidth/innerHeight consistently render at
    // ~1.1667x whatever logical Component size we ask for — present from
    // the very first frame, not something that grows in after a delay (not
    // a host override, a fixed mismatch). getPlatformScaleFactor() alone
    // does NOT equal that ratio directly (it read 1.75 while the mismatch
    // stayed ~1.1667) — but 1.75/1.5 = 1.1667 exactly, and compensating on
    // that basis lands the *logical* size back on 940 (the design's actual
    // width), which is too clean to be coincidence: this WebView2 build
    // appears to assume a fixed 150% baseline internally regardless of the
    // monitor's true scale, and getPlatformScaleFactor()/1.5 is the
    // resulting leftover error the page actually renders at. Dividing our
    // target by this ratio before calling setSize/setResizeLimits
    // pre-compensates so the page ends up seeing the size we intended.
    double getDpiCompensation() const
    {
        if (auto* peer = getPeer())
        {
            const double scale = peer->getPlatformScaleFactor();
            if (scale > 0.01) return scale / 1.5;
        }
        return 1.0;
    }

    // User presets: a real JSON file (not browser localStorage, which was
    // tied to a temp-directory WebView2 profile and would vanish) —
    // Source lives under the user's app-data folder, loaded once at
    // startup and pushed to JS via the 'userPresetsLoaded' backend event.
    juce::File getUserPresetsFile() const;
    void loadUserPresetsFromDisk();
    void saveUserPresetsToDisk();
    juce::Array<juce::var> userPresetList;

    RefraktProcessor& audioProcessor;
    juce::File uiFile; // resolved Source/ui/public/index.html on disk — loaded via file:// so edits show up on refresh(), no build step

    SinglePageBrowser webView;

    // On Windows, WebBrowserComponent's webview2 backend needs the Edge
    // WebView2 Runtime installed as a separate OS component — we only
    // statically link its LOADER (see CMakeLists.txt), not the runtime
    // itself. A machine missing it previously just showed a blank plugin
    // window with no explanation. Checked once at construction; if absent,
    // webView is hidden and this label shown instead. Always present but
    // only made visible in that case — see the constructor and resized().
    std::unique_ptr<juce::HyperlinkButton> missingRuntimeLink;
    juce::Label missingRuntimeLabel;
    bool webView2RuntimeAvailable = true;

    std::unordered_map<juce::String, float> lastKnownParamValues;

    // The mockup's non-embedded .plugin width is a fixed 940px — but in
    // embedded mode (see index.html's `body.embedded .plugin{width:100%}`)
    // .plugin stretches to fill whatever viewport WebView2 actually renders,
    // so measuring ITS width and feeding it back into the DPI-compensation
    // formula is circular: the measurement already reflects the very
    // distortion being corrected for, which makes the "correction" a no-op
    // every time regardless of scale (confirmed empirically at 100% scale —
    // content read as clipped even though the formula reported no change
    // needed). Height doesn't have this problem: contentH sums CHILD
    // elements' fixed-px heights, which stay constant regardless of
    // viewport/scale, so it's a genuine independent measurement the
    // compensation formula can correct against. Width needs the same kind
    // of stable reference — this constant IS that reference, standing in
    // for the (otherwise self-referential) width measurement.
    static constexpr int kDesignWidth = 940;

    // Base (100%) content size — starts as a hardcoded guess (the mockup's
    // .plugin is a fixed 940px-wide layout, height was estimated) and gets
    // corrected once at startup by handleReportContentSize, which JS calls
    // with .plugin's actual rendered size right after layout settles. A
    // wrong guess here used to show up as either clipped content or a dead
    // black bar below the last row. "Resizing" the plugin window otherwise
    // only ever means picking a scale factor via the Settings menu (CSS
    // zoom + this window resized to match), never an arbitrary drag-to-
    // resize gesture — that was exactly what clipped content and left a
    // stray scrollbar before.
    int baseWidth = 940, baseHeight = 760;

    // Some hosts (FL Studio's own generic plugin-window chrome, confirmed
    // by direct observation) resize the plugin's native window directly at
    // the OS level without ever going through VST3's checkSizeConstraint —
    // JUCE's constrainer/resizeLimits only intercepts resizes JUCE itself
    // initiates, so a host doing this bypasses it entirely and the editor's
    // Component bounds just follow whatever the OS handed it (clipped
    // content + scrollbars if smaller, dead space if bigger). resized() now
    // self-corrects: whenever the actual bounds drift from the current
    // legit target (updated by handleSetScale/handleReportContentSize), it
    // snaps straight back.
    int targetWidth = baseWidth, targetHeight = baseHeight;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RefraktEditor)
};
