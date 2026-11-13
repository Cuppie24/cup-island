#pragma once
// Typing in a provider's window on Windows (decision 105): keys, WM_CHAR,
// the clipboard and clicks in a field, turned into core's window_fields
// calls; what they send goes to the registry. provider_windows.cpp keeps a
// WindowTyping per window and lays it out again when these say so.

#include "core/window_fields.h"
#include "platform/provider_window.h"
#include "platform/win.h"

#include <string>

namespace cup {
class ProviderRegistry;
}

namespace cup::app {

// Clicks in a row on the same spot: a double click selects a word, a triple everything.
struct FieldClicks {
    Ms at{};
    Vec2 point;
    int count = 0;
    std::string selecting; // the field a press went down in: a drag selects in it
};

enum class Typed {
    PassOn,  // not the field's key: the window handles it (a field there let go of the keys first)
    Changed, // the field took it: lay the window out again
    Taken,   // the field took it, nothing to show (a key WM_CHAR types next)
};

// A key down while a field has the keys.
Typed typingKey(WindowTyping& typing, const WindowScene& scene, HWND owner, WPARAM vk, ProviderRegistry& registry);

// A WM_CHAR: printable text into the field (control characters were keys). True: it changed.
bool typingChar(WindowTyping& typing, const WindowScene& scene, wchar_t c, ProviderRegistry& registry);

// A press at `point` (window coordinates): on a field, it gets the keys
// and the caret, and true; elsewhere, a field that had the keys lets go
// (an edit is kept and sent) and false.
bool pressField(WindowTyping& typing, FieldClicks& clicks, const WindowScene& scene, const platform::ProviderWindow& window, Vec2 point,
                Ms now, ProviderRegistry& registry);

// The pointer moved with the button down after pressField: the selection follows it. True: it changed.
bool dragInField(WindowTyping& typing, const FieldClicks& clicks, const WindowScene& scene, const platform::ProviderWindow& window, Vec2 point);

// The keyboard landed on `stop` (window_keys.h): a field gets the keys, anything else takes them from a field.
void keysOn(WindowTyping& typing, const WindowScene& scene, const std::string& stop, ProviderRegistry& registry);

} // namespace cup::app
