#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    // Dev-time path to the real mockup HTML — loaded via file:// so editing
    // the file and calling refresh() (or just reopening the plugin window)
    // shows changes immediately, no C++ recompile and no bundler/dev-server
    // needed for a single self-contained HTML file. Before shipping this
    // needs to switch to an embedded/BinaryData resource provider instead
    // (see the commented-out getResource() path below, already wired as a
    // fallback) — flagged rather than silently deferred.
    const juce::File kUiFile { "C:/Projects/Refrakt/Source/ui/public/index.html" };

    // WebView2 needs a writable folder for its own profile data. Left
    // unspecified, it defaults to a location relative to the HOST
    // executable (e.g. FL64.exe under Program Files) — not writable by a
    // standard user, which makes environment creation fail. Shared by the
    // real webView construction below AND the areOptionsSupported() runtime
    // check in the constructor body, so the check actually reflects reality
    // instead of failing on a default the real construction never uses.
    juce::File getWebView2UserDataFolder()
    {
        return juce::File::getSpecialLocation (juce::File::SpecialLocationType::userApplicationDataDirectory)
            .getChildFile ("SkeptikAudio").getChildFile ("Refrakt").getChildFile ("WebView2");
    }

    // Every APVTS param the web UI can read/write, and how the "setState"
    // native function's values map onto them. Kept as one list so both the
    // outgoing (APVTS -> JS) and incoming (JS -> APVTS) directions stay in
    // sync with the same set — see timerCallback / handleSetParam.
    constexpr const char* kSyncedParamIDs[] =
    {
        "tilt_amount", "curve_shape", "pivot_freq", "pivot_boost", "wave_cycles",
        "lfo_intensity", "balance", "lfo_rate", "output_gain", "mix",
        "mod_mode", "lfo_shape", "inverted", "tempo_sync", "sync_division",
        "safe_bass_on", "safe_bass_freq", "bypass"
    };
}

bool SinglePageBrowser::pageAboutToLoad (const juce::String& newURL)
{
    return newURL == kUiFile.getFullPathName().replace ("\\", "/")
        || newURL.startsWith ("file:")
        || newURL == getResourceProviderRoot();
}

void SinglePageBrowser::pageFinishedLoading (const juce::String& url)
{
    juce::ignoreUnused (url);
    if (onPageFinishedLoading) onPageFinishedLoading();
}

RefraktEditor::RefraktEditor (RefraktProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), uiFile (kUiFile),
      webView (juce::WebBrowserComponent::Options{}
                   .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                   .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                                 .withUserDataFolder (getWebView2UserDataFolder()))
                   .withNativeIntegrationEnabled()
                   .withNativeFunction ("setState", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
                       { handleSetParam (args, std::move (complete)); })
                   .withNativeFunction ("setCustomShape", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
                       { handleSetCustomShape (args, std::move (complete)); })
                   .withNativeFunction ("randomise", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
                       { handleRandomise (args, std::move (complete)); })
                   .withNativeFunction ("setScale", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
                       { handleSetScale (args, std::move (complete)); })
                   .withNativeFunction ("saveUserPreset", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
                       { handleSaveUserPreset (args, std::move (complete)); })
                   .withNativeFunction ("deleteUserPreset", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
                       { handleDeleteUserPreset (args, std::move (complete)); })
                   .withNativeFunction ("reportContentSize", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
                       { handleReportContentSize (args, std::move (complete)); })
                   .withResourceProvider ([this] (const auto& url) { return getResource (url); }))
{
   #if JUCE_WINDOWS
    webView2RuntimeAvailable = juce::WebBrowserComponent::areOptionsSupported (
        juce::WebBrowserComponent::Options{}
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                          .withUserDataFolder (getWebView2UserDataFolder())));
   #endif

    if (! webView2RuntimeAvailable)
    {
        // Don't even try to load the UI into a browser backend that isn't
        // actually there — leave webView inert and hidden, show a plain,
        // actionable message instead of a silently blank plugin window.
        missingRuntimeLabel.setText (
            "Refrakt needs the Microsoft Edge WebView2 Runtime, which isn't "
            "installed on this system.\n\nInstall it (free, from Microsoft), "
            "then reopen this plugin:",
            juce::dontSendNotification);
        missingRuntimeLabel.setJustificationType (juce::Justification::centred);
        missingRuntimeLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (missingRuntimeLabel);

        missingRuntimeLink = std::make_unique<juce::HyperlinkButton> (
            "Download WebView2 Runtime",
            juce::URL ("https://go.microsoft.com/fwlink/p/?LinkId=2124703"));
        addAndMakeVisible (*missingRuntimeLink);

        setSize (baseWidth, baseHeight);
        setResizable (false, false);
        return;
    }

    loadUserPresetsFromDisk();

    addAndMakeVisible (webView);

    webView.onPageFinishedLoading = [this]
    {
        // Forces the next timer tick to treat every synced param as
        // "changed", which pushes a full initial state to the page —
        // handles first load and any manual refresh() during development
        // identically, with no separate "send everything" code path.
        lastKnownParamValues.clear();
        webView.emitEventIfBrowserIsVisible ("userPresetsLoaded", juce::var (userPresetList));

        // Custom-shape nodes and protect zones live outside APVTS entirely
        // (see header) — reopening the editor recreates the WebView from
        // scratch, resetting JS's own nodes/zones arrays to empty, and
        // nothing was pushing the processor's real values back. Any
        // Custom-shape curve or protect zone silently vanished on
        // reconnect, reading as "reverted to default" even though the
        // ordinary APVTS-backed params (Tilt, Rate, Mix, etc.) were fine.
        std::vector<RefraktProcessor::CustomNode> nodes;
        std::vector<RefraktProcessor::ProtectZone> zones;
        audioProcessor.getCustomShapeStateForDisplay (nodes, zones);

        juce::Array<juce::var> nodesArr;
        for (auto& n : nodes)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("freq", n.freq); o->setProperty ("pan", n.pan); o->setProperty ("q", n.q);
            nodesArr.add (juce::var (o));
        }
        juce::Array<juce::var> zonesArr;
        for (auto& z : zones)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("lo", z.lo); o->setProperty ("hi", z.hi);
            zonesArr.add (juce::var (o));
        }
        auto* shapeObj = new juce::DynamicObject();
        shapeObj->setProperty ("nodes", nodesArr);
        shapeObj->setProperty ("zones", zonesArr);
        webView.emitEventIfBrowserIsVisible ("customShapeLoaded", juce::var (shapeObj));
    };

    webView.goToURL (uiFile.existsAsFile()
        ? "file:///" + uiFile.getFullPathName().replace ("\\", "/")
        : juce::WebBrowserComponent::getResourceProviderRoot());

    // Resizing only ever happens via the Settings-menu scale buttons now
    // (see handleSetScale) — the mockup's .plugin is a fixed 940px-wide
    // layout with no responsive breakpoints, so an arbitrary drag-to-resize
    // gesture just clipped content and left a stray scrollbar instead of
    // actually reflowing anything. setResizable(false, ...) alone isn't
    // enough — FL Studio's own generic plugin window still offered a manual
    // resize handle regardless (its wrapper chrome, not ours). Locking
    // min==max via setResizeLimits goes through the actual VST3 size-
    // constraint protocol instead of the resizable hint, so even if the
    // host offers a resize gesture, dragging it snaps right back — handleSetScale
    // moves min and max together so scale changes still work.
    setSize (baseWidth, baseHeight);
    setResizable (true, false);
    setResizeLimits (baseWidth, baseHeight, baseWidth, baseHeight);

    startTimerHz (30);
}

RefraktEditor::~RefraktEditor() { stopTimer(); }

void RefraktEditor::resized()
{
    if (! webView2RuntimeAvailable)
    {
        auto bounds = getLocalBounds().reduced (20);
        missingRuntimeLabel.setBounds (bounds.removeFromTop (bounds.getHeight() - 30));
        if (missingRuntimeLink != nullptr)
            missingRuntimeLink->setBounds (bounds);
        return;
    }

    if (getWidth() != targetWidth || getHeight() != targetHeight)
    {
        setSize (targetWidth, targetHeight); // triggers resized() again with the corrected size
        return;
    }

    webView.setBounds (getLocalBounds());
}

// Polls APVTS for changes not caused by the web UI's own last "setState"
// call — host automation, undo/redo, a DAW project reloading saved state —
// and pushes them into JS. Diff-based rather than per-parameter-listener
// callbacks: APVTS listener callbacks can fire from the audio thread when a
// change originates from host automation, and touching the WebView from
// there would be unsafe; polling on this Timer keeps everything on the
// message thread with no extra locking.
void RefraktEditor::timerCallback()
{
    // Cheap insurance against any host that resizes the window outside
    // JUCE's own notification chain (resized()'s self-correct only fires
    // on JUCE-initiated resizes) — snaps back within ~33ms if it ever
    // drifts from the correct (DPI-compensated) target.
    if (getWidth() != targetWidth || getHeight() != targetHeight)
        setSize (targetWidth, targetHeight);

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

    // Real post-processing output levels/correlation — the JS meters used
    // to fake these from the displayed curve shape (moved with Phase/params
    // even in total silence, since the curve isn't audio). These atomics
    // are written every processBlock from the actual output samples.
    auto* meterObj = new juce::DynamicObject();
    meterObj->setProperty ("l", audioProcessor.meterPeakL.load (std::memory_order_relaxed));
    meterObj->setProperty ("r", audioProcessor.meterPeakR.load (std::memory_order_relaxed));
    meterObj->setProperty ("corr", audioProcessor.meterCorrelation.load (std::memory_order_relaxed));
    webView.emitEventIfBrowserIsVisible ("meterUpdate", juce::var (meterObj));

    // Real input spectrum, resampled onto the same log-frequency buckets
    // the curve display uses — the JS background silhouette used to be
    // simSpec(), a purely decorative procedural shape with no relation to
    // what's actually playing.
    juce::Array<juce::var> specArr;
    specArr.ensureStorageAllocated (RefraktProcessor::kUiSpectrumBins);
    for (int i = 0; i < RefraktProcessor::kUiSpectrumBins; ++i)
        specArr.add (audioProcessor.uiSpectrum[(size_t) i].load (std::memory_order_relaxed));
    webView.emitEventIfBrowserIsVisible ("spectrumUpdate", juce::var (specArr));
}

void RefraktEditor::handleSetParam (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
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
                lastKnownParamValues[paramID] = natural; // avoid immediately echoing this back to JS next tick
            }
        }
    }
    complete ({});
}

void RefraktEditor::handleSetCustomShape (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
{
    std::vector<RefraktProcessor::CustomNode> nodes;
    std::vector<RefraktProcessor::ProtectZone> zones;

    if (args.size() >= 1 && args[0].isArray())
        for (auto& n : *args[0].getArray())
            if (auto* o = n.getDynamicObject())
                nodes.push_back ({ (float) o->getProperty ("freq"), (float) o->getProperty ("pan"), (float) o->getProperty ("q") });

    if (args.size() >= 2 && args[1].isArray())
        for (auto& z : *args[1].getArray())
            if (auto* o = z.getDynamicObject())
                zones.push_back ({ (float) o->getProperty ("lo"), (float) o->getProperty ("hi") });

    audioProcessor.setCustomShapeState (nodes, zones);
    complete ({});
}

void RefraktEditor::handleRandomise (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
{
    // The mockup's own randomise() already mutates its local JS state and
    // will sync the result back out via setState on the next frame — this
    // native function exists only in case a future pass wants server-side
    // (C++-driven) randomisation instead. No-op for now.
    complete ({});
}

void RefraktEditor::handleSetScale (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
{
    if (! args.isEmpty())
    {
        const double pct = juce::jlimit (50.0, 200.0, (double) args[0]);
        const int w = juce::roundToInt (baseWidth * pct / 100.0), h = juce::roundToInt (baseHeight * pct / 100.0);
        // Locked limits move together with the size — a stale min==max at
        // the old scale would just clamp this setSize() call right back.
        // targetWidth/Height also move so resized()'s self-correct snaps to
        // the new scale instead of fighting it back to the base size.
        targetWidth = w; targetHeight = h;
        setResizeLimits (w, h, w, h);
        setSize (w, h);
    }
    complete ({});
}

// JS measures .plugin's actual rendered size once at startup (before any
// scale change, so this is always the true 100% base size) and reports it
// here — corrects baseWidth/baseHeight away from their hardcoded initial
// guess, which is what caused either clipped content or a dead black bar
// below the last row when the guess didn't match reality.
void RefraktEditor::handleReportContentSize (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
{
    const int receivedH = args.size() >= 2 ? (int) args[1] : -1;
    if (args.size() >= 2)
    {
        const double dpiComp = getDpiCompensation();
        // Width uses the known design constant, not the received value —
        // see kDesignWidth's comment in the header for why .plugin's own
        // measured width can't be used here (it's self-referential in
        // embedded mode). Height's received value stays genuinely useful
        // (children's fixed-px heights aren't viewport-relative).
        const int w = juce::roundToInt ((double) kDesignWidth / dpiComp);
        const int h = juce::roundToInt ((double) receivedH / dpiComp);

        if (w > 0 && h > 0 && (w != baseWidth || h != baseHeight))
        {
            baseWidth = w; baseHeight = h;
            targetWidth = w; targetHeight = h;
            setResizeLimits (w, h, w, h);
            setSize (w, h);
        }
    }
    complete ({});
}

juce::File RefraktEditor::getUserPresetsFile() const
{
    return juce::File::getSpecialLocation (juce::File::SpecialLocationType::userApplicationDataDirectory)
        .getChildFile ("SkeptikAudio").getChildFile ("Refrakt").getChildFile ("user_presets.json");
}

void RefraktEditor::loadUserPresetsFromDisk()
{
    userPresetList.clear();
    auto file = getUserPresetsFile();
    if (! file.existsAsFile()) return;

    auto parsed = juce::JSON::parse (file);
    if (auto* arr = parsed.getArray())
        userPresetList = *arr;
}

void RefraktEditor::saveUserPresetsToDisk()
{
    auto file = getUserPresetsFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::JSON::toString (juce::var (userPresetList)));
}

// name: String, state: { params, zones, nodes, mode, lfoShape, inverted,
// syncOn, currentDiv, safeBassOn, safeBassHi } — matches the mockup's own
// snapshotForSave() shape exactly, so it round-trips through
// applyLoadedUserPresets() on the JS side with no translation.
void RefraktEditor::handleSaveUserPreset (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
{
    if (args.size() >= 2)
    {
        const auto name = args[0].toString();
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("name", name);
        if (auto* stateObj = args[1].getDynamicObject())
            for (auto& prop : stateObj->getProperties())
                obj->setProperty (prop.name, prop.value);

        const juce::var entry (obj);
        bool replaced = false;
        for (auto& existing : userPresetList)
        {
            if (existing.getProperty ("name", "").toString() == name)
            {
                existing = entry;
                replaced = true;
                break;
            }
        }
        if (! replaced) userPresetList.add (entry);

        saveUserPresetsToDisk();
    }
    complete ({});
}

void RefraktEditor::handleDeleteUserPreset (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
{
    if (! args.isEmpty())
    {
        const auto name = args[0].toString();
        for (int i = userPresetList.size(); --i >= 0;)
            if (userPresetList[i].getProperty ("name", "").toString() == name)
                userPresetList.remove (i);
        saveUserPresetsToDisk();
    }
    complete ({});
}

std::optional<juce::WebBrowserComponent::Resource> RefraktEditor::getResource (const juce::String& url)
{
    // This is what every machine other than the dev one actually uses (the
    // hardcoded C:/Projects/Refrakt/... dev path in uiFile only ever
    // exists here) — the UI is embedded in the binary via BinaryData
    // (see CMakeLists.txt) rather than read from disk, so a packaged
    // build works on a tester's machine with no extra files alongside it.
    const auto makeResource = [] (const char* data, int size, const char* mimeType)
    {
        std::vector<std::byte> bytes (
            reinterpret_cast<const std::byte*> (data),
            reinterpret_cast<const std::byte*> (data) + size);
        return std::make_optional (juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mimeType) });
    };

    if (url.contains ("REFRAKT_small_trans.svg"))
        return makeResource (BinaryData::REFRAKT_small_trans_svg, BinaryData::REFRAKT_small_trans_svgSize, "image/svg+xml");

    return makeResource (BinaryData::index_html, BinaryData::index_htmlSize, "text/html");
}
