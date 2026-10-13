#pragma once
// Editing the text in a field (decision 043): a caret and a selection, moved
// and changed the way any Windows text box does it. Pure: offsets are UTF-8
// bytes into Draft::text, always on a character's first byte.

#include "core/island.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace cup {

// Typed text replaces the selection, or goes in at the caret.
void insertAtCaret(Draft& draft, const std::string& text);

// Backspace (back) or Delete (forward): the selection, else one character,
// or with `word` (Ctrl) up to the next word boundary.
void eraseAtCaret(Draft& draft, bool forward, bool word);

// Arrows, Ctrl+arrows, Home and End. `select` (Shift) keeps the anchor and
// grows the selection; without it, a selection collapses to the side moved to.
void moveCaret(Draft& draft, CaretMove move, bool select);

// A click (or a drag, with `select`) at byte offset `at`, snapped to a character.
void placeCaret(Draft& draft, std::size_t at, bool select);

// A double-click: the word (or the run of spaces) at `at`.
void selectWordAt(Draft& draft, std::size_t at);

void selectAll(Draft& draft); // Ctrl+A, a triple-click

// What's selected, for Ctrl+C and Ctrl+X; empty if nothing is.
std::string selectedText(const Draft& draft);

// DirectWrite counts UTF-16 units; the draft counts UTF-8 bytes. The UTF-16
// units in `utf8`'s first `bytes` bytes, and back (snapped to a character).
std::size_t utf16Index(std::string_view utf8, std::size_t bytes);
std::size_t utf8Offset(std::string_view utf8, std::size_t utf16);

} // namespace cup
