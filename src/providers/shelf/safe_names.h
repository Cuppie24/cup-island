#pragma once
// Names and contents for what the Shelf writes itself (decision 061): text
// becomes a .txt file, a link a .url file, and every name is one Windows
// accepts. Pure, so the rules are tested on Linux.

#include <string>

namespace cup::shelf {

// `name` with \ / : * ? " < > | and control characters turned into '_',
// without trailing dots and spaces, never a reserved name (CON, NUL, COM1…),
// cut to `maxChars` characters (UTF-8 kept whole). Blank: "Untitled".
std::string safeFileName(const std::string& name, std::size_t maxChars = 80);

// Text's file: its first words, ".txt".
std::string textFileName(const std::string& text);
// UTF-8 with a BOM: Notepad on Windows 10 1809 needs it to read UTF-8.
std::string textFileBody(const std::string& text);

// A link's file: the page's title, else the address's host, ".url".
std::string urlFileName(const std::string& url, const std::string& title);
// The Internet Shortcut format Windows opens in the browser.
std::string urlFileBody(const std::string& url);

} // namespace cup::shelf
