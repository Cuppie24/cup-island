#include "platform/field_painting.h"

#include "core/item_metrics.h"
#include "core/text_edit.h"
#include "core/tokens.h"
#include "platform/text_format.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace cup::platform {
namespace {

constexpr float kCaretWidth = 1.5f; // a hairline, like any text box's

// The field's text laid out (one line, or the lines core broke it into), and where it sits in the field.
struct FieldLine {
    winrt::com_ptr<IDWriteTextLayout> layout;
    float inset = 0;   // the text area starts this far in from each side
    float room = 0;    // ...and is this wide
    float left = 0;    // where the text starts: the inset, less how far it's scrolled
};

// Where a position is drawn: its x, and its line's top and height.
struct Spot {
    float x = 0, top = 0, height = 0;
};

Spot spotOf(IDWriteTextLayout* layout, std::size_t utf16)
{
    FLOAT x = 0, y = 0;
    DWRITE_HIT_TEST_METRICS hit{};
    if (FAILED(layout->HitTestTextPosition(static_cast<UINT32>(utf16), FALSE, &x, &y, &hit))) return {};
    return { x, hit.top, hit.height };
}

FieldLine lineOf(IDWriteFactory* dw, const TextInput& input, Vec2 size)
{
    const Tokens& t = tokens();
    FieldLine line;
    // Edited in place: the Text's own size and place, so nothing jumps when editing starts (decision 043).
    float fontSize = input.inPlace ? t.bodyFontSize : t.inputFontSize;
    line.inset = input.inPlace ? t.editInset : t.buttonLabelPadding;
    line.room = std::max(0.0f, size.x - 2 * line.inset);
    bool empty = input.value.empty(), wrapped = !input.wrapped.empty();
    std::wstring value = widen(empty ? input.placeholder : wrapped ? input.wrapped : input.value);
    auto f = format(dw, textFamily(dw, fontSize), fontSize, t.bodyFontWeight, DWRITE_TEXT_ALIGNMENT_LEADING);
    if (wrapped) setLineHeight(f.get(), lineHeight(TextStyle::Body)); // as tall as core laid it out (decision 045)
    winrt::check_hresult(dw->CreateTextLayout(value.c_str(), static_cast<UINT32>(value.size()), f.get(), 100000.0f, size.y, line.layout.put()));
    DWRITE_TEXT_METRICS metrics{};
    winrt::check_hresult(line.layout->GetMetrics(&metrics));
    float width = metrics.widthIncludingTrailingWhitespace + kCaretWidth;
    // One line longer than the field: scrolled just enough to show the caret.
    // Worked out from the caret alone (no remembered scroll), so painting and
    // clicks agree. Wrapped lines already fit: no scrolling.
    float scroll = 0;
    if (!empty && !wrapped && width > line.room) {
        float caret = spotOf(line.layout.get(), utf16Index(input.value, input.caret)).x + kCaretWidth;
        scroll = std::clamp(caret - line.room, 0.0f, width - line.room);
    }
    line.left = line.inset - scroll;
    return line;
}

} // namespace

void paintInput(ID2D1DeviceContext* dc, IDWriteFactory* dw, const TextInput& input, Vec2 size)
{
    const Tokens& t = tokens();
    FieldLine line = lineOf(dw, input, size);
    bool empty = input.value.empty();
    // Scrolled text stops at the text area's edges (the caret may sit right on one).
    dc->PushAxisAlignedClip(D2D1::RectF(line.inset - kCaretWidth, 0, line.inset + line.room + kCaretWidth, size.y), D2D1_ANTIALIAS_MODE_ALIASED);

    if (input.focused && !empty && input.caret != input.anchor) {
        auto from = static_cast<UINT32>(utf16Index(input.value, std::min(input.caret, input.anchor)));
        auto to = static_cast<UINT32>(utf16Index(input.value, std::max(input.caret, input.anchor)));
        UINT32 count = 0;
        line.layout->HitTestTextRange(from, to - from, line.left, 0, nullptr, 0, &count); // asks how many: one per line and run
        std::vector<DWRITE_HIT_TEST_METRICS> runs(count);
        if (count > 0 && SUCCEEDED(line.layout->HitTestTextRange(from, to - from, line.left, 0, runs.data(), count, &count))) {
            auto brush = brushOf(dc, t.selectionColor);
            for (const DWRITE_HIT_TEST_METRICS& run : runs)
                dc->FillRectangle(D2D1::RectF(run.left, run.top, run.left + run.width, run.top + run.height), brush.get());
        }
    }
    dc->DrawTextLayout(D2D1::Point2F(line.left, 0), line.layout.get(), brushOf(dc, empty ? t.placeholderTextColor : t.bodyTextColor).get(),
                       D2D1_DRAW_TEXT_OPTIONS_CLIP);
    if (input.focused) {
        // A still caret, on its line: no blinking, so an idle field costs nothing between keys (decision 016).
        Spot at = spotOf(line.layout.get(), empty ? 0 : utf16Index(input.value, input.caret));
        float x = empty ? line.inset : line.left + at.x;
        dc->FillRectangle(D2D1::RectF(x, at.top, x + kCaretWidth, at.top + at.height), brushOf(dc, t.bodyTextColor).get());
    }
    dc->PopAxisAlignedClip();
}

std::size_t caretAt(IDWriteFactory* dw, const TextInput& input, Vec2 size, Vec2 at)
{
    if (input.value.empty()) return 0;
    FieldLine line = lineOf(dw, input, size);
    return hitOffset(line.layout.get(), input.value, at.x - line.left, at.y);
}

std::optional<std::size_t> caretOnLine(IDWriteFactory* dw, const TextInput& input, Vec2 size, int direction)
{
    if (input.value.empty() || input.wrapped.empty()) return std::nullopt;
    FieldLine line = lineOf(dw, input, size);
    Spot at = spotOf(line.layout.get(), utf16Index(input.value, input.caret));
    DWRITE_TEXT_METRICS metrics{};
    winrt::check_hresult(line.layout->GetMetrics(&metrics));
    float y = at.top + at.height / 2 + static_cast<float>(direction) * at.height;
    if (y < metrics.top || y > metrics.top + metrics.height) return std::nullopt; // no line there
    return hitOffset(line.layout.get(), input.value, at.x, y);
}

} // namespace cup::platform
