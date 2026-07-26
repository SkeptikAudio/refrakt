#pragma once
#include "PluginProcessor.h"
#include <vector>
#include <utility>

// Factory preset bank — 40 presets, 10 per Modulation Mode (Static / Tilt /
// Wave / Tilt+Wave), ported 1:1 from the GUI mockup's PRESETS array so both
// artifacts stay in sync. Each preset is a full parameter snapshot (every
// applicable param gets an explicit value at apply time, defaulted first)
// rather than a diff, matching the mockup's mk()/nz() semantics — selecting
// a preset is always deterministic regardless of what was tweaked before.
struct PresetZoneHz { float loHz, hiHz; };

struct PresetDef
{
    const char* name;
    const char* category;
    std::vector<std::pair<const char*, float>> values; // paramID -> natural-unit value
    std::vector<RefraktProcessor::CustomNode> nodes {};
    std::vector<PresetZoneHz> zonesHz {};
};

const std::vector<PresetDef>& getFactoryPresets();

// Applies preset `index` to `proc`: resets every preset-controlled param to
// its APVTS default, then overlays this preset's values, then swaps in its
// Custom-node / protect-zone state. Message-thread only (preset selection is
// always a GUI action) — safe to call from the editor.
void applyPreset (RefraktProcessor& proc, int index);
