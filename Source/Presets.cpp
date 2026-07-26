#include "Presets.h"
#include "CurveMath.h"
#include <cmath>

namespace
{
    using RefraktCurveMath::hzToFrac;
    using N = RefraktProcessor::CustomNode;

    // mod_mode indices: 0 Static, 1 Tilt, 2 Wave, 3 Tilt+Wave
    // lfo_shape indices: 0 Sine, 1 Triangle, 2 Saw, 3 Square, 4 Random, 5 Custom
    // sync_division indices: 0 1/16, 1 1/8, 2 1/4, 3 1/2, 4 1 Bar, 5 2 Bar

    const std::vector<PresetDef> presets =
    {
        // --- Static (10): plain tilt/shape curve, no LFO motion ---
        { "Default", "Static", { {"mod_mode",0.0f} } },
        { "Gentle Widen", "Static", { {"mod_mode",0.0f}, {"tilt_amount",0.3f} } },
        { "Gentle Narrow", "Static", { {"mod_mode",0.0f}, {"tilt_amount",-0.35f} } },
        { "Steep Tilt Right", "Static", { {"mod_mode",0.0f}, {"tilt_amount",0.7f}, {"curve_shape",0.2f} } },
        { "Steep Tilt Left", "Static", { {"mod_mode",0.0f}, {"tilt_amount",-0.7f}, {"curve_shape",-0.2f} } },
        { "S-Curve Spread", "Static", { {"mod_mode",0.0f}, {"curve_shape",0.8f} } },
        { "Inverted S-Curve", "Static", { {"mod_mode",0.0f}, {"tilt_amount",0.1f}, {"curve_shape",-0.75f} } },
        { "High Focus Pivot", "Static", { {"mod_mode",0.0f}, {"tilt_amount",0.5f}, {"pivot_freq",3000.0f} } },
        { "Custom Bloom", "Static", { {"mod_mode",0.0f}, {"lfo_shape",5.0f} },
            { N{0.15f,-0.5f,4.0f}, N{0.5f,0.55f,4.0f}, N{0.85f,0.0f,4.0f} } },
        { "Custom Bloom Safe Bass", "Static", { {"mod_mode",0.0f}, {"tilt_amount",0.2f}, {"lfo_shape",5.0f}, {"safe_bass_on",1.0f} },
            { N{0.25f,0.5f,3.0f}, N{0.55f,-0.5f,3.0f}, N{0.8f,0.4f,3.0f} } },

        // --- Wave (10): all 7 fixed LFO shapes + Custom, rate variety, one safe bass, one safe zone ---
        { "Sine Sweep", "Wave", { {"mod_mode",2.0f}, {"lfo_shape",0.0f}, {"tilt_amount",0.2f}, {"lfo_rate",0.4f}, {"wave_cycles",2.0f}, {"lfo_intensity",0.6f} } },
        { "Triangle Roll", "Wave", { {"mod_mode",2.0f}, {"lfo_shape",1.0f}, {"tilt_amount",0.1f}, {"tempo_sync",1.0f}, {"sync_division",2.0f}, {"wave_cycles",3.0f}, {"lfo_intensity",0.65f} } },
        { "Saw Rip Wave", "Wave", { {"mod_mode",2.0f}, {"lfo_shape",2.0f}, {"lfo_rate",2.5f}, {"wave_cycles",4.0f}, {"lfo_intensity",0.75f} } },
        { "Square Snap Safe", "Wave", { {"mod_mode",2.0f}, {"lfo_shape",3.0f}, {"lfo_rate",1.2f}, {"wave_cycles",2.0f}, {"lfo_intensity",0.7f}, {"safe_bass_on",1.0f} } },
        { "Random Drift Zone", "Wave", { {"mod_mode",2.0f}, {"lfo_shape",4.0f}, {"lfo_rate",0.6f}, {"wave_cycles",2.0f}, {"lfo_intensity",0.55f} }, {}, { {800.0f,4000.0f} } },
        { "Triangle Flicker", "Wave", { {"mod_mode",2.0f}, {"lfo_shape",1.0f}, {"tempo_sync",1.0f}, {"sync_division",1.0f}, {"wave_cycles",1.0f}, {"lfo_intensity",0.8f} } },
        { "Random Strobe", "Wave", { {"mod_mode",2.0f}, {"lfo_shape",4.0f}, {"lfo_rate",6.0f}, {"wave_cycles",2.0f}, {"lfo_intensity",0.85f} } },
        { "Custom Wave Bloom", "Wave", { {"mod_mode",2.0f}, {"lfo_shape",5.0f}, {"lfo_rate",0.4f}, {"wave_cycles",2.0f}, {"lfo_intensity",0.6f} },
            { N{0.2f,0.5f,9.0f}, N{0.4f,-0.5f,9.0f}, N{0.6f,0.5f,9.0f}, N{0.8f,-0.5f,9.0f} } },
        { "Custom Wave Storm", "Wave", { {"mod_mode",2.0f}, {"lfo_shape",5.0f}, {"lfo_rate",3.0f}, {"wave_cycles",6.0f}, {"lfo_intensity",0.9f} },
            { N{0.08f,0.4f,4.0f}, N{0.2f,-0.4f,4.0f}, N{0.32f,0.35f,4.0f}, N{0.45f,-0.35f,4.0f}, N{0.58f,0.3f,4.0f}, N{0.7f,-0.3f,4.0f}, N{0.82f,0.25f,4.0f}, N{0.93f,-0.25f,4.0f} } },
        { "Deep Field Wave", "Wave", { {"mod_mode",2.0f}, {"lfo_shape",1.0f}, {"lfo_rate",0.1f}, {"wave_cycles",1.0f}, {"lfo_intensity",0.4f} } },

        // --- Tilt (10): tilt/shape ranging, subtle pivot moves, one safe bass, custom shape, rate variety ---
        { "Slow Breathe", "Tilt", { {"mod_mode",1.0f}, {"tilt_amount",0.3f}, {"lfo_rate",0.3f}, {"lfo_intensity",0.3f} } },
        { "Gentle Reverse Breathe", "Tilt", { {"mod_mode",1.0f}, {"tilt_amount",-0.3f}, {"lfo_rate",0.3f}, {"lfo_intensity",0.3f} } },
        { "Deep Tilt Swing", "Tilt", { {"mod_mode",1.0f}, {"tilt_amount",0.6f}, {"curve_shape",0.3f}, {"lfo_rate",0.5f}, {"lfo_intensity",0.6f} } },
        { "Sharp Tilt Pulse", "Tilt", { {"mod_mode",1.0f}, {"tilt_amount",0.5f}, {"curve_shape",-0.2f}, {"lfo_rate",2.0f}, {"lfo_intensity",0.7f} } },
        { "Pivot Shift Low", "Tilt", { {"mod_mode",1.0f}, {"tilt_amount",0.4f}, {"pivot_freq",250.0f}, {"lfo_rate",0.4f}, {"lfo_intensity",0.5f} } },
        { "Pivot Shift High", "Tilt", { {"mod_mode",1.0f}, {"tilt_amount",0.4f}, {"pivot_freq",2200.0f}, {"lfo_rate",0.4f}, {"lfo_intensity",0.5f} } },
        { "Glacial Tilt Safe Bass", "Tilt", { {"mod_mode",1.0f}, {"tilt_amount",0.3f}, {"lfo_rate",0.1f}, {"lfo_intensity",0.4f}, {"safe_bass_on",1.0f} } },
        { "Fast Tilt Flicker", "Tilt", { {"mod_mode",1.0f}, {"tilt_amount",0.6f}, {"curve_shape",0.2f}, {"lfo_rate",5.0f}, {"lfo_intensity",0.8f} } },
        { "Custom Tilt Ride", "Tilt", { {"mod_mode",1.0f}, {"tilt_amount",0.2f}, {"lfo_shape",5.0f}, {"lfo_rate",0.35f}, {"lfo_intensity",0.55f} },
            { N{0.1f,-0.4f,5.0f}, N{0.3f,0.3f,5.0f}, N{0.5f,-0.2f,5.0f}, N{0.7f,0.4f,5.0f}, N{0.9f,-0.3f,5.0f} } },
        { "Custom Tilt Storm", "Tilt", { {"mod_mode",1.0f}, {"tilt_amount",0.4f}, {"curve_shape",0.1f}, {"lfo_shape",5.0f}, {"lfo_rate",1.5f}, {"lfo_intensity",0.7f} },
            { N{0.12f,0.5f,6.0f}, N{0.28f,-0.5f,6.0f}, N{0.44f,0.45f,6.0f}, N{0.6f,-0.45f,6.0f}, N{0.76f,0.4f,6.0f}, N{0.92f,-0.4f,6.0f} } },

        // --- Tilt+Wave (10): all shapes, tilt/wave biases, one safe bass, one safe zone, custom shape ---
        { "Sine Rolling Balance", "Tilt+Wave", { {"mod_mode",3.0f}, {"lfo_shape",0.0f}, {"tilt_amount",0.3f}, {"curve_shape",0.1f}, {"lfo_rate",0.4f}, {"wave_cycles",2.0f}, {"lfo_intensity",0.5f}, {"balance",0.0f} } },
        { "Triangle Wide Horizon", "Tilt+Wave", { {"mod_mode",3.0f}, {"lfo_shape",1.0f}, {"tilt_amount",0.2f}, {"curve_shape",0.1f}, {"wave_cycles",8.0f}, {"lfo_rate",0.4f}, {"lfo_intensity",0.65f}, {"balance",0.4f} } },
        { "Saw Tilt Lean", "Tilt+Wave", { {"mod_mode",3.0f}, {"lfo_shape",2.0f}, {"tilt_amount",0.3f}, {"lfo_rate",1.0f}, {"wave_cycles",3.0f}, {"lfo_intensity",0.6f}, {"balance",-0.5f} } },
        { "Square Snap Blend Safe", "Tilt+Wave", { {"mod_mode",3.0f}, {"lfo_shape",3.0f}, {"lfo_rate",1.5f}, {"wave_cycles",2.0f}, {"lfo_intensity",0.7f}, {"balance",0.2f}, {"safe_bass_on",1.0f} } },
        { "Random Storm Zone", "Tilt+Wave", { {"mod_mode",3.0f}, {"lfo_shape",4.0f}, {"tilt_amount",0.3f}, {"curve_shape",0.3f}, {"wave_cycles",6.0f}, {"lfo_rate",3.0f}, {"lfo_intensity",0.9f}, {"balance",0.0f} }, {}, { {8000.0f,16000.0f} } },
        { "Saw Drift Blend", "Tilt+Wave", { {"mod_mode",3.0f}, {"lfo_shape",2.0f}, {"tilt_amount",0.2f}, {"wave_cycles",1.0f}, {"lfo_rate",0.4f}, {"lfo_intensity",0.6f}, {"balance",0.3f} } },
        { "Square Storm Blend", "Tilt+Wave", { {"mod_mode",3.0f}, {"lfo_shape",3.0f}, {"tilt_amount",0.4f}, {"lfo_rate",4.0f}, {"wave_cycles",2.0f}, {"lfo_intensity",0.75f}, {"balance",-0.3f} } },
        { "Custom Layered Drift", "Tilt+Wave", { {"mod_mode",3.0f}, {"lfo_shape",5.0f}, {"wave_cycles",2.0f}, {"lfo_rate",0.4f}, {"lfo_intensity",0.6f}, {"balance",0.0f} },
            { N{0.1f,0.45f,5.0f}, N{0.25f,-0.45f,5.0f}, N{0.42f,0.4f,5.0f}, N{0.58f,-0.4f,5.0f}, N{0.75f,0.4f,5.0f}, N{0.9f,-0.4f,5.0f} } },
        { "Custom Aurora Bloom", "Tilt+Wave", { {"mod_mode",3.0f}, {"lfo_shape",5.0f}, {"tilt_amount",0.3f}, {"curve_shape",0.3f}, {"wave_cycles",4.0f}, {"lfo_rate",0.35f}, {"lfo_intensity",0.85f}, {"balance",0.4f} },
            { N{0.18f,-0.5f,4.0f}, N{0.5f,0.6f,4.0f}, N{0.82f,-0.5f,4.0f} } },
        { "Long Fade Sky", "Tilt+Wave", { {"mod_mode",3.0f}, {"lfo_shape",0.0f}, {"lfo_rate",0.1f}, {"lfo_intensity",0.7f}, {"balance",0.6f} } },
    };

    // Every param a preset is allowed to touch — reset to its APVTS default
    // before applying preset-specific overrides, so selecting a preset is a
    // full deterministic snapshot rather than a diff against whatever was
    // dialed in before (matches the mockup's mk()/nz() semantics). Output
    // Gain and Bypass are deliberately excluded — those are mixing-desk-style
    // controls that persist across preset changes, not part of a preset's identity.
    constexpr const char* kResettableParams[] =
    {
        "tilt_amount", "curve_shape", "pivot_freq", "lfo_rate", "lfo_intensity",
        "wave_cycles", "balance", "inverted", "mod_mode", "lfo_shape",
        "tempo_sync", "sync_division", "safe_bass_on", "safe_bass_freq", "mix"
    };
}

const std::vector<PresetDef>& getFactoryPresets() { return presets; }

void applyPreset (RefraktProcessor& proc, int index)
{
    if (index < 0 || (size_t) index >= presets.size()) return;
    const auto& def = presets[(size_t) index];

    for (auto* id : kResettableParams)
        if (auto* p = proc.apvts.getParameter (id))
            p->setValueNotifyingHost (p->getDefaultValue());

    for (auto& kv : def.values)
        if (auto* p = proc.apvts.getParameter (kv.first))
            p->setValueNotifyingHost (p->convertTo0to1 (kv.second));

    std::vector<RefraktProcessor::CustomNode> nodes (def.nodes.begin(), def.nodes.end());
    std::vector<RefraktProcessor::ProtectZone> zones;
    zones.reserve (def.zonesHz.size());
    for (auto& z : def.zonesHz)
        zones.push_back ({ (float) hzToFrac ((double) z.loHz), (float) hzToFrac ((double) z.hiHz) });

    proc.setCustomShapeState (nodes, zones);
}
