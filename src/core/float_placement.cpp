#include "core/float_placement.h"

#include "core/tokens.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace cup {
namespace {

const Screen* findScreen(const std::vector<Screen>& screens, const std::string& key)
{
    for (const Screen& s : screens)
        if (s.key == key) return &s;
    return nullptr;
}

// The screen holding most of `r`: where a cascade keeps it.
const Screen& screenOf(const std::vector<Screen>& screens, Rect r)
{
    const Screen* best = &screens.front();
    float most = -1;
    for (const Screen& s : screens) {
        float w = std::min(r.x + r.width, s.workArea.x + s.workArea.width) - std::max(r.x, s.workArea.x);
        float h = std::min(r.y + r.height, s.workArea.y + s.workArea.height) - std::max(r.y, s.workArea.y);
        float area = std::max(0.0f, w) * std::max(0.0f, h);
        if (area > most) most = area, best = &s;
    }
    return *best;
}

// On the work area: moved in whole if it fits; too big, its top-left stays
// in, so it can always be reached and dragged.
Rect keepOn(Rect r, Rect area)
{
    r.x = std::clamp(r.x, area.x, std::max(area.x, area.x + area.width - r.width));
    r.y = std::clamp(r.y, area.y, std::max(area.y, area.y + area.height - r.height));
    return r;
}

Rect firstPlace(const FloatSpec& spec, Vec2 size, const Screen& on, Rect pill)
{
    Rect area = on.workArea;
    Rect r{ 0, 0, size.x * on.scale, size.y * on.scale };
    if (spec.start == FloatStart::Centre) {
        r.x = area.x + (area.width - r.width) / 2;
        r.y = area.y + (area.height - r.height) / 2;
    } else { // under the pill, centred on it: it grows out of it (decision 081)
        r.x = pill.x + (pill.width - r.width) / 2;
        r.y = pill.y + pill.height + tokens().floatIslandGap * on.scale;
    }
    return keepOn(r, area);
}

bool sameCorner(Rect a, Rect b) { return std::abs(a.x - b.x) < 2 && std::abs(a.y - b.y) < 2; }

} // namespace

void remember(Places& places, const std::string& cardId, const std::string& providerId, const SavedPlace& place)
{
    places.byCard[cardId] = place;
    places.byProvider[providerId] = place;
}

std::optional<SavedPlace> recall(const Places& places, const std::string& cardId, const std::string& providerId)
{
    if (auto it = places.byCard.find(cardId); it != places.byCard.end()) return it->second;
    if (auto it = places.byProvider.find(providerId); it != places.byProvider.end()) return it->second;
    return std::nullopt;
}

Rect placeFloat(const FloatSpec& spec, Vec2 size, const std::vector<Screen>& screens, const std::string& islandScreen,
                Rect pill, const std::optional<SavedPlace>& saved, const std::vector<Rect>& open)
{
    if (screens.empty()) return { pill.x, pill.y + pill.height, size.x, size.y };
    Rect r;
    const Screen* on = saved ? findScreen(screens, saved->screen) : nullptr;
    if (on) {
        Vec2 logical = spec.resizable ? Vec2{ saved->frame.width, saved->frame.height } : size;
        r = keepOn({ on->workArea.x + saved->frame.x * on->scale, on->workArea.y + saved->frame.y * on->scale, logical.x * on->scale,
                     logical.y * on->scale },
                   on->workArea);
    } else {
        on = findScreen(screens, islandScreen);
        r = firstPlace(spec, size, on ? *on : screens.front(), pill);
    }
    // Not exactly over another one: a step right and down, as Windows cascades
    // windows, so two cards from one place both show.
    const Screen& holder = screenOf(screens, r);
    for (std::size_t step = 0; step < open.size(); ++step) {
        bool taken = std::any_of(open.begin(), open.end(), [&](Rect o) { return sameCorner(o, r); });
        if (!taken) break;
        float d = tokens().floatCascade * holder.scale;
        Rect next = keepOn({ r.x + d, r.y + d, r.width, r.height }, holder.workArea);
        if (sameCorner(next, r)) break; // against the screen's corner: no room to step
        r = next;
    }
    return r;
}

SavedPlace savedPlace(Rect frame, const Screen& screen)
{
    float scale = screen.scale > 0 ? screen.scale : 1;
    return { screen.key,
             { (frame.x - screen.workArea.x) / scale, (frame.y - screen.workArea.y) / scale, frame.width / scale, frame.height / scale } };
}

std::string formatPlaces(const Places& places)
{
    std::ostringstream out;
    auto write = [&](const char* kind, const std::map<std::string, SavedPlace>& by) {
        for (const auto& [id, p] : by)
            out << kind << '\t' << id << '\t' << p.screen << '\t' << p.frame.x << '\t' << p.frame.y << '\t' << p.frame.width << '\t'
                << p.frame.height << '\n';
    };
    write("card", places.byCard);
    write("provider", places.byProvider);
    return out.str();
}

Places parsePlaces(const std::string& text)
{
    Places places;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        std::istringstream fields(line);
        std::string kind, id, screen, x, y, w, h;
        if (!std::getline(fields, kind, '\t') || !std::getline(fields, id, '\t') || !std::getline(fields, screen, '\t') ||
            !std::getline(fields, x, '\t') || !std::getline(fields, y, '\t') || !std::getline(fields, w, '\t') || !std::getline(fields, h))
            continue;
        auto* by = kind == "card" ? &places.byCard : kind == "provider" ? &places.byProvider : nullptr;
        try {
            Rect frame{ std::stof(x), std::stof(y), std::stof(w), std::stof(h) };
            if (!by || id.empty() || screen.empty() || frame.width <= 0 || frame.height <= 0) continue;
            (*by)[id] = { screen, frame };
        } catch (...) {
            continue; // not numbers: skipped, like any line that doesn't read
        }
    }
    return places;
}

} // namespace cup
