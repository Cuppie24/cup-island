#pragma once
// A Host that records what a panel provider publishes and saves, and
// helpers to read a published widget and card, or its window (decision 104).
// For the system widgets' tests.

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

    // Its window (decision 104): the last content sent while it's open.
    std::optional<WindowContent> window;
    std::optional<WindowSpec> windowSpec;
    WindowOpen openWindow(const WindowSpec& spec, WindowContent content) override
    {
        WindowOpen result = window ? WindowOpen::Raised : WindowOpen::Opened;
        window = std::move(content);
        windowSpec = spec;
        return result;
    }
    void updateWindow(const std::string&, WindowContent content) override
    {
        if (window) window = std::move(content);
    }
    void closeWindow(const std::string&) override { window.reset(); }
};

// A window's page by id, or nullptr.
inline const WindowPage* pageOf(const PanelHost& h, const std::string& id)
{
    if (!h.window) return nullptr;
    for (const WindowPage& p : h.window->pages)
        if (p.id == id) return &p;
    return nullptr;
}

// A row's name: its first Text's value.
inline std::string nameOf(const Row& r)
{
    for (const Item& item : r.items)
        if (auto* text = std::get_if<Text>(&item)) return text->value;
    return "";
}

// The window's row with this name, on any page, or nullptr.
inline const Row* windowRow(const PanelHost& h, const std::string& name)
{
    if (!h.window) return nullptr;
    for (const WindowPage& p : h.window->pages)
        for (const WindowSection& s : p.sections)
            for (const Row& r : s.rows)
                if (nameOf(r) == name) return &r;
    return nullptr;
}

// What that row says under its name (its Text's detail), "?" if there's no row.
inline std::string detailOf(const PanelHost& h, const std::string& name)
{
    const Row* r = windowRow(h, name);
    if (!r) return "?";
    for (const Item& item : r->items)
        if (auto* text = std::get_if<Text>(&item)) return text->detail;
    return "?";
}

// That row's item of type T (its switch, its button), or nullptr.
template <typename T> const T* itemOf(const Row* r)
{
    if (!r) return nullptr;
    for (const Item& item : r->items)
        if (auto* x = std::get_if<T>(&item)) return x;
    return nullptr;
}

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

// A card row's `item`th item's text (or its key caps' text).
inline std::string cardItem(const Panel& p, std::size_t row, std::size_t item)
{
    const Slot& card = p.content.expandedCenter;
    if (row >= card.size() || item >= card[row].items.size()) return "?";
    if (auto* keys = std::get_if<Keys>(&card[row].items[item])) return keys->text; // key caps (decision 092)
    auto* text = std::get_if<Text>(&card[row].items[item]);
    return text ? text->value : "?";
}

// The widget's side-square icon (tileLeading, decision 073).
inline Glyph leadGlyph(const Panel& p)
{
    if (p.content.tileLeading.empty() || p.content.tileLeading[0].items.empty()) return Glyph::Warning;
    auto* icon = std::get_if<Icon>(&p.content.tileLeading[0].items[0]);
    return icon ? icon->glyph : Glyph::Warning;
}

// The card's row whose first item is this text (a setting's name), or nullptr.
inline const Row* rowNamed(const Panel& p, const std::string& name)
{
    for (const Row& r : p.content.expandedCenter)
        if (!r.items.empty())
            if (auto* text = std::get_if<Text>(&r.items[0]); text && text->value == name) return &r;
    return nullptr;
}

// That row's value: the description under its name (decision 090), else its second item's text; "?" if there's none.
inline std::string valueOf(const Panel& p, const std::string& name)
{
    const Row* r = rowNamed(p, name);
    if (r && !std::get<Text>(r->items[0]).detail.empty()) return std::get<Text>(r->items[0]).detail;
    if (!r || r->items.size() < 2) return "?";
    auto* text = std::get_if<Text>(&r->items[1]);
    return text ? text->value : "?";
}

} // namespace cup::test
