#include "providers/shelf/shelf_items.h"

#include <algorithm>
#include <sstream>

namespace cup::shelf {
namespace {

// Tabs, newlines and backslashes escaped, so a field is always one cell.
std::string escape(const std::string& text)
{
    std::string out;
    for (char c : text) {
        if (c == '\\') out += "\\\\";
        else if (c == '\t') out += "\\t";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}

std::string unescape(const std::string& text)
{
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\\' || i + 1 == text.size()) { out += text[i]; continue; }
        char c = text[++i];
        out += c == 't' ? '\t' : c == 'n' ? '\n' : c == 'r' ? '\r' : c;
    }
    return out;
}

std::vector<std::string> cells(const std::string& line)
{
    std::vector<std::string> out;
    std::size_t from = 0;
    for (std::size_t tab; (tab = line.find('\t', from)) != std::string::npos; from = tab + 1) out.push_back(unescape(line.substr(from, tab - from)));
    out.push_back(unescape(line.substr(from)));
    return out;
}

const char* kWhat[] = { "file", "folder", "text", "url" };
const char* kState[] = { "ready", "copying", "failed", "missing" };

// A saved word back to its index in `names`; unknown: `fallback`.
int indexOf(const std::string& word, const char* const* names, int count, int fallback)
{
    for (int i = 0; i < count; ++i)
        if (word == names[i]) return i;
    return fallback;
}

} // namespace

const std::string& shownName(const Entry& item) { return item.label.empty() ? item.name : item.label; }

const Entry* Items::find(std::uint64_t id) const
{
    auto it = std::find_if(items_.begin(), items_.end(), [&](const Entry& i) { return i.id == id; });
    return it == items_.end() ? nullptr : &*it;
}

Entry* Items::at(std::uint64_t id) { return const_cast<Entry*>(find(id)); }

std::uint64_t Items::add(Entry item)
{
    item.id = nextId_++;
    items_.push_back(std::move(item));
    return items_.back().id;
}

bool Items::remove(std::uint64_t id) { return std::erase_if(items_, [&](const Entry& i) { return i.id == id; }) > 0; }
void Items::clear() { items_.clear(); }

bool Items::setProgress(std::uint64_t id, float fraction)
{
    Entry* item = at(id);
    if (!item || item->state != State::Copying) return false;
    item->progress = std::clamp(fraction, 0.0f, 1.0f);
    return true;
}

bool Items::copied(std::uint64_t id, const std::string& stored, std::uint64_t bytes)
{
    Entry* item = at(id);
    if (!item || item->state != State::Copying) return false;
    item->state = State::Ready;
    item->path = stored;
    item->bytes = bytes;
    item->progress = 1;
    return true;
}

bool Items::failed(std::uint64_t id, const std::string& problem)
{
    Entry* item = at(id);
    if (!item || item->state != State::Copying) return false;
    item->state = State::Failed;
    item->problem = problem;
    item->bytes = 0;
    return true;
}

bool Items::keepAsLink(std::uint64_t id)
{
    Entry* item = at(id);
    if (!item || item->state != State::Failed || item->source.empty()) return false;
    item->kind = Kind::Link;
    item->state = State::Ready;
    item->path = item->source;
    item->source.clear();
    item->problem.clear();
    return true;
}

bool Items::setMissing(std::uint64_t id, bool missing)
{
    Entry* item = at(id);
    if (!item || item->kind != Kind::Link) return false;
    State want = missing ? State::Missing : State::Ready;
    if (item->state == want || (item->state != State::Ready && item->state != State::Missing)) return false;
    item->state = want;
    return true;
}

bool Items::relabel(std::uint64_t id, const std::string& label)
{
    Entry* item = at(id);
    if (!item || item->kind != Kind::Link) return false;
    std::string next = label == item->name ? std::string() : label;
    if (next == item->label) return false;
    item->label = next;
    return true;
}

bool Items::renamed(std::uint64_t id, const std::string& stored, const std::string& name)
{
    Entry* item = at(id);
    if (!item || item->kind != Kind::Copy || item->state != State::Ready) return false;
    item->path = stored;
    item->name = name;
    return true;
}

std::uint64_t Items::storeUsed() const
{
    std::uint64_t used = 0;
    for (const Entry& i : items_)
        if (i.kind == Kind::Copy) used += i.bytes;
    return used;
}

std::uint64_t Items::totalBytes() const
{
    std::uint64_t total = 0;
    for (const Entry& i : items_) total += i.bytes;
    return total;
}

bool Items::anyLinks() const
{
    return std::any_of(items_.begin(), items_.end(), [](const Entry& i) { return i.kind == Kind::Link; });
}

std::string Items::save() const
{
    std::string out;
    for (const Entry& i : items_) {
        out += std::to_string(i.id) + '\t' + (i.kind == Kind::Copy ? "copy" : "link") + '\t' + kState[static_cast<int>(i.state)] + '\t' +
               kWhat[static_cast<int>(i.what)] + '\t' + std::to_string(i.bytes) + '\t' + escape(i.name) + '\t' + escape(i.path) + '\t' +
               escape(i.source) + '\t' + escape(i.problem) + '\t' + escape(i.label) + '\n';
    }
    return out;
}

Items Items::load(const std::string& saved)
{
    Items list;
    std::istringstream in(saved);
    for (std::string line; std::getline(in, line);) {
        std::vector<std::string> c = cells(line);
        if (c.size() < 9) continue; // not ours: skipped
        Entry item;
        try {
            item.id = std::stoull(c[0]);
            item.bytes = std::stoull(c[4]);
        } catch (...) {
            continue;
        }
        item.kind = c[1] == "copy" ? Kind::Copy : Kind::Link;
        item.state = static_cast<State>(indexOf(c[2], kState, 4, 0));
        item.what = static_cast<DragKind>(indexOf(c[3], kWhat, 4, 0));
        item.name = c[5];
        item.path = c[6];
        item.source = c[7];
        item.problem = c[8];
        if (c.size() > 9) item.label = c[9]; // saved before decision 063: no label
        if (item.state == State::Copying) { // the app quit mid-copy (decision 061)
            item.state = State::Failed;
            item.problem = "The copy didn't finish";
            item.bytes = 0;
        }
        if (item.id == 0 || list.find(item.id)) continue;
        list.nextId_ = std::max(list.nextId_, item.id + 1);
        list.items_.push_back(std::move(item));
    }
    return list;
}

} // namespace cup::shelf
