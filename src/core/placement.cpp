#include "core/placement.h"

#include "core/tokens.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace cup {
namespace {

float width(Footprint f) { return f.pillWidth + f.rightOfPill; }

// Snapping to an edge must catch every drop that would leave a flare cut by
// the screen edge, so its zone is never narrower than the flare.
float edgeZone() { return std::max(tokens().edgeSnap, tokens().flareRadius); }

} // namespace

Placement placeAt(Spot spot, Footprint footprint, float screenWidth)
{
    float w = width(footprint);
    float left = 0;
    switch (spot.dock) {
    case Dock::Centre: left = (screenWidth - footprint.pillWidth) / 2; break; // the pill centred; a bubble hangs right of it
    case Dock::Left: left = 0; break;
    case Dock::Right: left = screenWidth - w; break;
    case Dock::Free: left = spot.fraction * screenWidth - footprint.pillWidth / 2; break;
    }
    // A side closer to its edge than the flare would show half a flare, cut
    // by the monitor's edge: push it flush instead. Off the screen too.
    float flare = tokens().flareRadius;
    if (screenWidth - (left + w) < flare) left = screenWidth - w;
    if (left < flare) left = 0;
    if (w > screenWidth) left = (screenWidth - w) / 2; // can't fit: centred, both sides off
    return { left, left <= 0, left + w >= screenWidth };
}

float clampLifted(float pillCentre, float pillWidth, float screenWidth)
{
    if (pillWidth >= screenWidth) return screenWidth / 2;
    return std::clamp(pillCentre, pillWidth / 2, screenWidth - pillWidth / 2);
}

Spot spotForDrop(float pillCentre, Footprint footprint, float screenWidth)
{
    if (std::abs(pillCentre - screenWidth / 2) <= tokens().centreSnap) return { Dock::Centre };
    float left = pillCentre - footprint.pillWidth / 2;
    float right = screenWidth - (left + width(footprint));
    if (left <= edgeZone()) return { Dock::Left };
    if (right <= edgeZone()) return { Dock::Right };
    return { Dock::Free, screenWidth > 0 ? pillCentre / screenWidth : 0.5f };
}

std::map<std::string, Spot> parseSpots(const std::string& text)
{
    std::map<std::string, Spot> spots;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string word = line.substr(0, tab), display = line.substr(tab + 1);
        if (word == "centre") spots[display] = { Dock::Centre };
        else if (word == "left") spots[display] = { Dock::Left };
        else if (word == "right") spots[display] = { Dock::Right };
        else {
            char* end = nullptr;
            float f = std::strtof(word.c_str(), &end);
            if (end != word.c_str() && *end == '\0' && f > 0 && f < 1) spots[display] = { Dock::Free, f };
        }
    }
    return spots;
}

std::string formatSpots(const std::map<std::string, Spot>& spots)
{
    std::ostringstream out;
    for (const auto& [display, spot] : spots) {
        switch (spot.dock) {
        case Dock::Centre: out << "centre"; break;
        case Dock::Left: out << "left"; break;
        case Dock::Right: out << "right"; break;
        case Dock::Free: out << spot.fraction; break;
        }
        out << '\t' << display << '\n';
    }
    return out.str();
}

} // namespace cup
