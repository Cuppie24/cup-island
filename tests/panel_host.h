#pragma once
// A Host that records what a panel provider publishes and saves, and
// helpers to read a published widget and card. For the system widgets' tests.

#include "core/provider.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cup::test {

struct PanelHost : Host {
    std::vector<Panel> published;
    std::vector<std::string> removed;

    void startActivity(Activity) override {}
    void endActivity(const std::string&) override {}
    std::vector<Alert> alerts;
    void postAlert(Alert a) override { alerts.push_back(std::move(a)); }
    Ms clock{ 0 };
    std::optional<Ms> wake;
    Ms now() const override { return clock; }
    void publishPanel(Panel p) override { published.push_back(std::move(p)); }
    void removePanel(const std::string& id) override { removed.push_back(id); }
    void wakeAt(std::optional<Ms> at) override { wake = at; }
    void ask(Dialog) override {}
    std::string revealed; // "<list>@<row>"
    void reveal(const std::string&, const std::string& list, std::size_t row) override { revealed = list + "@" + std::to_string(row); }
    std::map<std::string, std::string> store; // Host::load/save
    std::map<std::string, int> saves;         // per key
    std::string load(const std::string& key) const override { auto it = store.find(key); return it == store.end() ? "" : it->second; }
    void save(const std::string& key, const std::string& text) override { store[key] = text; ++saves[key]; }
    Ms wallClock() const override { return clock; }
};

inline std::string textAt(const Panel& p, std::size_t row, std::size_t item)
{
    if (row >= p.content.tile.size() || item >= p.content.tile[row].items.size()) return "?";
    auto* text = std::get_if<Text>(&p.content.tile[row].items[item]);
    return text ? text->value : "?";
}

// A card row's text, or its button's label.
inline std::string cardText(const Panel& p, std::size_t row)
{
    const Slot& card = p.content.expandedCenter;
    if (row >= card.size() || card[row].items.empty()) return "?";
    if (auto* text = std::get_if<Text>(&card[row].items[0])) return text->value;
    if (auto* button = std::get_if<Button>(&card[row].items[0])) return button->label;
    return "?";
}

// A card row's `item`th item's text.
inline std::string cardItem(const Panel& p, std::size_t row, std::size_t item)
{
    const Slot& card = p.content.expandedCenter;
    if (row >= card.size() || item >= card[row].items.size()) return "?";
    auto* text = std::get_if<Text>(&card[row].items[item]);
    return text ? text->value : "?";
}

inline Glyph glyphAt(const Panel& p, std::size_t row)
{
    auto* icon = std::get_if<Icon>(&p.content.tile[row].items[0]);
    return icon ? icon->glyph : Glyph::Warning;
}

} // namespace cup::test
