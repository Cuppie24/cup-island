#include "app/window_typing.h"

#include "core/text_edit.h"
#include "platform/text_input.h"
#include "providers/provider_registry.h"

#include <cmath>
#include <optional>

namespace cup::app {
namespace {

void send(ProviderRegistry& registry, const std::optional<Action>& action)
{
    if (action) registry.route(*action);
}

const Placed* onPage(const WindowScene& s, const std::string& key)
{
    for (const Placed& p : s.page)
        if (p.key == key) return &p;
    return nullptr;
}

} // namespace

Typed typingKey(WindowTyping& typing, const WindowScene& s, HWND owner, WPARAM vk, ProviderRegistry& registry)
{
    if (!typing.draft) return Typed::PassOn;
    bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
    auto keyed = [&](FieldKey key) {
        FieldOutcome out = fieldKey(typing, s, key, ctrl, shift);
        send(registry, out.send);
        return Typed::Changed;
    };
    switch (vk) {
    case VK_RETURN: return keyed(FieldKey::Enter);
    case VK_ESCAPE: return keyed(FieldKey::Escape);
    case VK_BACK: return keyed(FieldKey::Backspace);
    case VK_DELETE: return keyed(FieldKey::Delete);
    case VK_LEFT: return keyed(FieldKey::Left);
    case VK_RIGHT: return keyed(FieldKey::Right);
    case VK_HOME: return keyed(FieldKey::Home);
    case VK_END: return keyed(FieldKey::End);
    default: break;
    }
    if (ctrl && vk == 'A') return keyed(FieldKey::SelectAll);
    if (ctrl && (vk == 'C' || vk == 'X')) { // copy, cut (decision 043)
        std::string selected = selectedText(*typing.draft);
        if (selected.empty() || !platform::writeClipboardText(owner, selected)) return Typed::Taken;
        return vk == 'X' ? keyed(FieldKey::Delete) : Typed::Taken;
    }
    if (ctrl && vk == 'V') {
        send(registry, typeText(typing, s, platform::readClipboardText(owner)).send);
        return Typed::Changed;
    }
    // Tab, Up and Down, Ctrl+Tab, Ctrl+W: the window's. The field lets go first.
    if (vk == VK_TAB || vk == VK_UP || vk == VK_DOWN || vk == VK_PRIOR || vk == VK_NEXT || (ctrl && vk == 'W')) {
        send(registry, blurField(typing, s));
        return Typed::PassOn;
    }
    return Typed::Taken; // a letter, Space: WM_CHAR types it
}

bool typingChar(WindowTyping& typing, const WindowScene& s, wchar_t c, ProviderRegistry& registry)
{
    static wchar_t highSurrogate = 0; // the first half of a character outside the BMP (emoji)
    if (!typing.draft) return false;
    if (c >= 0xD800 && c <= 0xDBFF) {
        highSurrogate = c;
        return false;
    }
    if (c < 0x20 || c == 0x7F) return false; // Enter, Backspace, Ctrl+letters: handled as keys
    wchar_t pair[2] = { highSurrogate, c };
    bool low = highSurrogate && c >= 0xDC00 && c <= 0xDFFF;
    highSurrogate = 0;
    std::string text = platform::toUtf8(low ? std::wstring_view(pair, 2) : std::wstring_view(&c, 1));
    send(registry, typeText(typing, s, text).send);
    return true;
}

bool pressField(WindowTyping& typing, FieldClicks& clicks, const WindowScene& s, const platform::ProviderWindow& window, Vec2 point,
                Ms now, ProviderRegistry& registry)
{
    clicks.selecting.clear();
    WindowHit hit = windowHitTest(s, point);
    const Placed* field = hit.part == WindowPart::Content && hit.hit && isField(s, hit.hit->key) ? onPage(s, hit.hit->key) : nullptr;
    if (!field) {
        send(registry, blurField(typing, s)); // pressed elsewhere: an edit is kept (decision 043)
        clicks.count = 0;
        return false;
    }
    std::size_t at = window.caretAt(*field, toPage(s, point));
    auto* input = std::get_if<TextInput>(&field->item);
    bool focused = input && input->focused && typing.draft;
    bool sameSpot = std::abs(point.x - clicks.point.x) <= GetSystemMetrics(SM_CXDOUBLECLK) / 2 &&
                    std::abs(point.y - clicks.point.y) <= GetSystemMetrics(SM_CYDOUBLECLK) / 2;
    clicks.count = focused && sameSpot && now - clicks.at <= Ms{ GetDoubleClickTime() } ? clicks.count % 3 + 1 : 1;
    clicks.at = now;
    clicks.point = point;
    clicks.selecting = field->key;
    if (!focused) send(registry, focusField(typing, s, field->key, at));
    else if (clicks.count == 2) selectWord(typing, at);
    else if (clicks.count == 3) selectAllText(typing);
    else moveCaretTo(typing, at, GetKeyState(VK_SHIFT) < 0);
    return true;
}

bool dragInField(WindowTyping& typing, const FieldClicks& clicks, const WindowScene& s, const platform::ProviderWindow& window, Vec2 point)
{
    if (clicks.selecting.empty() || clicks.count != 1 || !typing.draft) return false;
    const Placed* field = onPage(s, clicks.selecting);
    if (!field) return false;
    std::size_t at = window.caretAt(*field, toPage(s, point)); // off either end: its first or last character
    if (at == typing.draft->caret) return false;
    moveCaretTo(typing, at, true);
    return true;
}

void keysOn(WindowTyping& typing, const WindowScene& s, const std::string& stop, ProviderRegistry& registry)
{
    std::string key = stop.rfind("el:", 0) == 0 ? stop.substr(3) : "";
    if (!key.empty() && isField(s, key)) send(registry, focusField(typing, s, key, std::nullopt));
    else send(registry, blurField(typing, s));
}

} // namespace cup::app
