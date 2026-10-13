#pragma once
// Typing into the island (decision 015): the clipboard's text and UTF-16 to
// UTF-8, so core and providers only ever see UTF-8.

#include <string>
#include <string_view>

namespace cup::platform {

// The clipboard's text as UTF-8; empty if it holds no text. owner: one of
// our windows (the clipboard needs one).
std::string readClipboardText(void* ownerHwnd);

// Puts UTF-8 `text` on the clipboard (Ctrl+C, Ctrl+X in a field, decision 030). False if it couldn't.
bool writeClipboardText(void* ownerHwnd, const std::string& text);

std::string toUtf8(std::wstring_view text);

} // namespace cup::platform
