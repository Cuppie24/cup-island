#include "core/window_placement.h"

#include "core/tokens.h"

#include <algorithm>
#include <sstream>

namespace cup {
namespace {

const Screen* findScreen(const std::vector<Screen>& screens, const std::string& key)
{
    for (const Screen& s : screens)
        if (s.key == key) return &s;
    return nullptr;
}

// On the work area: moved in whole if it fits; too big, its top-left stays
// in, so the title bar can always be reached and dragged.
Rect keepOn(Rect r, Rect area)
{
    r.x = std::clamp(r.x, area.x, std::max(area.x, area.x + area.width - r.width));
    r.y = std::clamp(r.y, area.y, std::max(area.y, area.y + area.height - r.height));
    return r;
}

} // namespace

Rect placeWindow(const WindowSpec& spec, Vec2 size, const std::vector<Screen>& screens, const std::string& islandScreen,
                 Rect pill, const std::optional<SavedPlace>& saved)
{
    if (screens.empty()) return { pill.x, pill.y + pill.height, size.x, size.y };
    if (saved) {
        if (const Screen* on = findScreen(screens, saved->screen)) {
            Vec2 logical = spec.resizable ? Vec2{ saved->frame.width, saved->frame.height } : size;
            Rect r{ on->workArea.x + saved->frame.x * on->scale, on->workArea.y + saved->frame.y * on->scale,
                    logical.x * on->scale, logical.y * on->scale };
            return keepOn(r, on->workArea);
        }
    }
    const Screen* on = findScreen(screens, islandScreen);
    if (!on) on = &screens.front();
    Rect area = on->workArea;
    Rect r{ 0, 0, size.x * on->scale, size.y * on->scale };
    if (spec.start == WindowStart::Centre) {
        r.x = area.x + (area.width - r.width) / 2;
        r.y = area.y + (area.height - r.height) / 2;
    } else { // under the pill, centred on it: it grows out of it (decision 081)
        r.x = pill.x + (pill.width - r.width) / 2;
        r.y = pill.y + pill.height + tokens().windowIslandGap * on->scale;
    }
    return keepOn(r, area);
}

SavedPlace savedPlace(Rect frame, const Screen& screen)
{
    float scale = screen.scale > 0 ? screen.scale : 1;
    return { screen.key,
             { (frame.x - screen.workArea.x) / scale, (frame.y - screen.workArea.y) / scale, frame.width / scale, frame.height / scale } };
}

std::string formatPlaces(const std::map<std::string, SavedPlace>& places)
{
    std::ostringstream out;
    for (const auto& [provider, place] : places)
        out << provider << '\t' << place.screen << '\t' << place.frame.x << '\t' << place.frame.y << '\t' << place.frame.width << '\t'
            << place.frame.height << '\n';
    return out.str();
}

std::map<std::string, SavedPlace> parsePlaces(const std::string& text)
{
    std::map<std::string, SavedPlace> places;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        std::istringstream fields(line);
        std::string provider, screen, x, y, w, h;
        if (!std::getline(fields, provider, '\t') || !std::getline(fields, screen, '\t') || !std::getline(fields, x, '\t') ||
            !std::getline(fields, y, '\t') || !std::getline(fields, w, '\t') || !std::getline(fields, h))
            continue;
        try {
            Rect frame{ std::stof(x), std::stof(y), std::stof(w), std::stof(h) };
            if (provider.empty() || screen.empty() || frame.width <= 0 || frame.height <= 0) continue;
            places[provider] = { screen, frame };
        } catch (...) {
            continue; // not numbers: skipped, like any line that doesn't read
        }
    }
    return places;
}

} // namespace cup
