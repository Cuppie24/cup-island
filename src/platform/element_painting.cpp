#include "platform/element_painting.h"

#include "core/item_metrics.h"
#include "core/text_edit.h"
#include "core/tokens.h"
#include "platform/text_format.h"

#include <dwrite_3.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace cup::platform {

std::wstring widen(const std::string& utf8)
{
    if (utf8.empty()) return {};
    int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), length);
    return out;
}

namespace {

bool hasFont(IDWriteFactory* dw, const wchar_t* family)
{
    winrt::com_ptr<IDWriteFontCollection> fonts;
    UINT32 index = 0;
    BOOL exists = FALSE;
    if (SUCCEEDED(dw->GetSystemFontCollection(fonts.put())))
        fonts->FindFamilyName(family, &index, &exists);
    return exists;
}

} // namespace

// Windows 11's Segoe UI Variable comes in optical sizes, cut for the pixel
// size they're shown at: Small stays sturdy and open at caption sizes, where
// the Text cut turns thin and soft; Display is tighter for big numbers. The
// ranges are physical pixels (sizes here are already scaled), a property of
// the font, not of the design, so they aren't tokens (decision 024). Windows 10: Segoe UI.
const wchar_t* textFamily(IDWriteFactory* dw, float pixelSize)
{
    // (Not named `small`: the Windows SDK's rpcndr.h has `#define small char`.)
    static const bool hasVariable = hasFont(dw, L"Segoe UI Variable Text");
    static const bool hasSmallCut = hasFont(dw, L"Segoe UI Variable Small");
    static const bool hasDisplayCut = hasFont(dw, L"Segoe UI Variable Display");
    if (!hasVariable) return L"Segoe UI";
    if (hasSmallCut && pixelSize <= 12.5f) return L"Segoe UI Variable Small";
    if (hasDisplayCut && pixelSize >= 28.0f) return L"Segoe UI Variable Display";
    return L"Segoe UI Variable Text";
}

namespace {

const wchar_t* iconFamily(IDWriteFactory* dw)
{
    static const wchar_t* family = hasFont(dw, L"Segoe Fluent Icons") ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets";
    return family;
}

// Same code points in Segoe Fluent Icons and Segoe MDL2 Assets.
const wchar_t* codepoint(Glyph glyph)
{
    switch (glyph) {
    case Glyph::Play: return L"";
    case Glyph::Pause: return L"";
    case Glyph::Next: return L"";
    case Glyph::Previous: return L"";
    case Glyph::Shuffle: return L"";
    case Glyph::Repeat: return L"";
    case Glyph::RepeatOne: return L"";
    case Glyph::Source: return L"";
    case Glyph::Music: return L"";
    case Glyph::Warning: return L"";
    case Glyph::Timer: return L"";
    case Glyph::Stop: return L"\uE71A";
    case Glyph::Home: return L"\uE80F";
    case Glyph::Sun: return L"\uE706";  // Brightness
    case Glyph::Moon: return L"\uE708"; // QuietHours (crescent)
    case Glyph::Copy: return L"\uE8C8";
    case Glyph::Checked: return L"\uE73A";   // CheckboxComposite
    case Glyph::Unchecked: return L"\uE739"; // Checkbox
    case Glyph::Delete: return L"\uE74D";
    case Glyph::Edit: return L"\uE70F";
    case Glyph::OpenApp: return L"\uE8A7"; // OpenInNewWindow
    case Glyph::Settings: return L"\uE713";
    case Glyph::ChevronLeft: return L"\uE76B";
    case Glyph::ChevronRight: return L"\uE76C";
    case Glyph::Download: return L"\uE896";
    case Glyph::Centre: return L"\uE8E3"; // AlignCenter: back to the middle
    case Glyph::Minus: return L"\uE738";  // Remove
    case Glyph::Plus: return L"\uE710";   // Add
    case Glyph::Pin: return L"\uE718";
    case Glyph::Pinned: return L"\uE840"; // Pinned, the outline with its needle; paintGlyph fills it (PinnedFill under it)
    }
    return L"?";
}

} // namespace

winrt::com_ptr<IDWriteTextFormat> format(IDWriteFactory* dw, const wchar_t* family, float size,
                                         float weight, DWRITE_TEXT_ALIGNMENT align)
{
    weight = std::clamp(weight, 1.0f, 999.0f);
    winrt::com_ptr<IDWriteTextFormat> f;
    winrt::check_hresult(dw->CreateTextFormat(family, nullptr, static_cast<DWRITE_FONT_WEIGHT>(std::lround(weight)),
                                              DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", f.put()));
    // A variable font (Segoe UI Variable) takes the exact weight on its weight
    // axis, e.g. 500 between Regular and Semibold. Without IDWriteTextFormat3
    // (before Windows 10 1803) the nearest weight above is used (decision 015).
    if (auto variable = f.try_as<IDWriteTextFormat3>()) {
        DWRITE_FONT_AXIS_VALUE axis{ DWRITE_FONT_AXIS_TAG_WEIGHT, weight };
        variable->SetFontAxisValues(&axis, 1);
    }
    f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    f->SetTextAlignment(align);
    f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    // Long titles end in "…" instead of being cut mid-letter.
    DWRITE_TRIMMING trimming{ DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    winrt::com_ptr<IDWriteInlineObject> ellipsis;
    if (SUCCEEDED(dw->CreateEllipsisTrimmingSign(f.get(), ellipsis.put()))) f->SetTrimming(&trimming, ellipsis.get());
    return f;
}

namespace {

D2D1::ColorF toD2d(const Color& c) { return D2D1::ColorF(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f); }

} // namespace

winrt::com_ptr<ID2D1SolidColorBrush> brushOf(ID2D1DeviceContext* dc, const Color& color)
{
    winrt::com_ptr<ID2D1SolidColorBrush> brush;
    winrt::check_hresult(dc->CreateSolidColorBrush(toD2d(color), brush.put()));
    return brush;
}

void setLineHeight(IDWriteTextFormat* format, float height)
{
    format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, height, height * 0.8f); // the baseline about where Segoe UI's own spacing puts it
}

std::size_t hitOffset(IDWriteTextLayout* layout, const std::string& utf8, float x, float y)
{
    BOOL trailing = FALSE, inside = FALSE;
    DWRITE_HIT_TEST_METRICS hit{};
    if (FAILED(layout->HitTestPoint(x, y, &trailing, &inside, &hit))) return utf8.size();
    return utf8Offset(utf8, hit.textPosition + (trailing ? hit.length : 0)); // the nearer edge of the character hit
}

namespace {

void drawString(ID2D1DeviceContext* dc, IDWriteTextFormat* f, const std::wstring& text, D2D1_RECT_F rect, const Color& color)
{
    dc->DrawText(text.c_str(), static_cast<UINT32>(text.size()), f, rect, brushOf(dc, color).get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void drawString(ID2D1DeviceContext* dc, IDWriteTextFormat* f, const std::wstring& text, Vec2 size, const Color& color)
{
    drawString(dc, f, text, D2D1::RectF(0, 0, size.x, size.y), color);
}

struct TextLook {
    float fontSize;
    Color color;
    float weight; // 1..999, from tokens
};

TextLook lookOf(TextStyle style)
{
    const Tokens& t = tokens();
    switch (style) {
    case TextStyle::Title: return { t.titleFontSize, t.titleTextColor, t.titleFontWeight };
    case TextStyle::Body: break;
    case TextStyle::Caption:
    case TextStyle::Digits: return { t.captionFontSize, t.captionTextColor, t.captionFontWeight };
    case TextStyle::Display: return { t.displayFontSize, t.titleTextColor, t.displayFontWeight };
    case TextStyle::Heading: return { t.headingFontSize, t.titleTextColor, t.headingFontWeight };
    }
    return { t.bodyFontSize, t.bodyTextColor, t.bodyFontWeight };
}

winrt::com_ptr<IDWriteTextFormat> formatOf(IDWriteFactory* dw, const Text& text)
{
    TextLook look = lookOf(text.style);
    return format(dw, textFamily(dw, look.fontSize), look.fontSize, look.weight,
                  text.centred ? DWRITE_TEXT_ALIGNMENT_CENTER
                  : text.alignEnd ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
}

// Wrapped text (decision 045): the lines core broke it into, exactly lineHeight apart.
winrt::com_ptr<IDWriteTextLayout> wrappedLayout(IDWriteFactory* dw, const Text& text, float width, float height)
{
    std::wstring value = widen(text.wrapped.empty() ? text.value : text.wrapped);
    auto f = formatOf(dw, text);
    if (!text.wrapped.empty()) setLineHeight(f.get(), lineHeight(text.style));
    winrt::com_ptr<IDWriteTextLayout> layout;
    winrt::check_hresult(dw->CreateTextLayout(value.c_str(), static_cast<UINT32>(value.size()), f.get(), width, height, layout.put()));
    return layout;
}

} // namespace

void paintText(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Text& text, Vec2 size)
{
    if (!text.struck && text.wrapped.empty()) {
        drawString(dc, formatOf(dw, text).get(), widen(text.value), size, lookOf(text.style).color);
        return;
    }
    // Wrapped, or struck through (a solved to-do): a layout carries the lines and the strikethrough.
    auto layout = wrappedLayout(dw, text, size.x, size.y);
    if (text.struck) layout->SetStrikethrough(TRUE, DWRITE_TEXT_RANGE{ 0, static_cast<UINT32>(widen(text.value).size()) });
    dc->DrawTextLayout(D2D1::Point2F(0, 0), layout.get(), brushOf(dc, lookOf(text.style).color).get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void paintLabel(ID2D1DeviceContext* dc, IDWriteFactory* dw, const std::string& label, Vec2 size, const Color& color)
{
    Text text{ .value = label, .style = TextStyle::Body, .centred = true };
    drawString(dc, formatOf(dw, text).get(), widen(label), size, color);
}

std::size_t caretAt(IDWriteFactory* dw, const Text& text, Vec2 size, Vec2 at)
{
    // As drawn (paintText), but unbounded across: a click on a trimmed text's "…" lands at its end.
    // Its lines are already broken (Text::wrapped), so nothing rewraps.
    auto layout = wrappedLayout(dw, text, 100000.0f, size.y);
    return hitOffset(layout.get(), text.value, at.x, at.y);
}

float measureText(IDWriteFactory* dw, const Text& text)
{
    std::wstring value = widen(text.value);
    winrt::com_ptr<IDWriteTextLayout> layout;
    winrt::check_hresult(dw->CreateTextLayout(value.c_str(), static_cast<UINT32>(value.size()), formatOf(dw, text).get(),
                                              100000.0f, 1000.0f, layout.put()));
    DWRITE_TEXT_METRICS metrics{};
    winrt::check_hresult(layout->GetMetrics(&metrics));
    return std::ceil(metrics.widthIncludingTrailingWhitespace);
}

void paintMarquee(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Text& text, float width, float lead, float gap, float height)
{
    Text start = text;
    start.alignEnd = false;
    start.centred = false;
    auto f = formatOf(dw, start);
    std::wstring value = widen(text.value);
    Color color = lookOf(text.style).color;
    // A pixel of slack, so rounding never trips the "…" trimming.
    for (float x : { lead, lead + width + gap }) drawString(dc, f.get(), value, D2D1::RectF(x, 0, x + width + 1, height), color);
}

void paintGlyph(ID2D1DeviceContext* dc, IDWriteFactory* dw, Glyph glyph, float fontSize, Vec2 size, const Color& color)
{
    auto f = format(dw, iconFamily(dw), fontSize, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER); // icons: as designed
    // PinnedFill (U+E841) is only a fill layer, with no needle: it goes under the Pinned outline (decision 046).
    if (glyph == Glyph::Pinned) drawString(dc, f.get(), L"\uE841", size, color);
    drawString(dc, f.get(), codepoint(glyph), size, color);
}

void paintToggleDot(ID2D1DeviceContext* dc, Vec2 size, float glyphSize)
{
    auto brush = brushOf(dc, tokens().buttonColor);
    float r = std::max(1.0f, glyphSize / 10), y = std::min(size.y / 2 + glyphSize / 2 + 2 * r + 1, size.y - r);
    dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(size.x / 2, y), r, r), brush.get());
}

void paintSteps(ID2D1DeviceContext* dc, const Steps& steps, Vec2 size, const Color& accent)
{
    const Tokens& t = tokens();
    float r = t.stepDotSize / 2, y = size.y / 2;
    for (int i = 0; i < steps.count; ++i) {
        auto brush = brushOf(dc, i < steps.done ? t.stepDoneColor : i == steps.done ? accent : t.stepTodoColor);
        float x = r + static_cast<float>(i) * (t.stepDotSize + t.stepDotGap);
        dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), r, r), brush.get());
    }
}

void paintSwatch(ID2D1DeviceContext* dc, const Swatch& swatch, Vec2 size)
{
    float r = std::min(size.x, size.y) / 2, ring = std::max(1.0f, r / 9); // a hairline at any scale
    auto centre = D2D1::Point2F(size.x / 2, size.y / 2);
    dc->FillEllipse(D2D1::Ellipse(centre, r, r), brushOf(dc, swatch.fill).get());
    dc->DrawEllipse(D2D1::Ellipse(centre, r - ring / 2, r - ring / 2), brushOf(dc, tokens().trackColor).get(), ring);
    float dot = r * 0.4f; // the accent: a dot, like a theme's live colour on its island
    dc->FillEllipse(D2D1::Ellipse(centre, dot, dot), brushOf(dc, swatch.mark).get());
}

void paintTag(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Tag& tag, Vec2 size)
{
    const Tokens& t = tokens();
    float r = size.y / 2;
    dc->FillRoundedRectangle(D2D1_ROUNDED_RECT{ D2D1::RectF(0, 0, size.x, size.y), r, r }, brushOf(dc, t.tagColor).get());
    float fontSize = std::min(t.captionFontSize, size.y * 0.75f);
    auto f = format(dw, textFamily(dw, fontSize), fontSize, t.titleFontWeight, DWRITE_TEXT_ALIGNMENT_CENTER);
    drawString(dc, f.get(), widen(tag.text), size, t.tagTextColor);
}

void paintImage(ID2D1DeviceContext* dc, const ImageData* data, Vec2 size, float cornerRadius)
{
    D2D1_ROUNDED_RECT shape{ D2D1::RectF(0, 0, size.x, size.y), cornerRadius, cornerRadius };
    if (!data || data->width <= 0 || data->height <= 0 ||
        data->bgra.size() < static_cast<size_t>(data->width) * static_cast<size_t>(data->height) * 4) {
        dc->FillRoundedRectangle(shape, brushOf(dc, tokens().imagePlaceholderColor).get());
        return;
    }

    winrt::com_ptr<ID2D1Bitmap1> bitmap;
    auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,
                                         D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    winrt::check_hresult(dc->CreateBitmap(D2D1::SizeU(static_cast<UINT32>(data->width), static_cast<UINT32>(data->height)),
                                          data->bgra.data(), static_cast<UINT32>(data->width) * 4, &props, bitmap.put()));

    // Scale to cover the shape (crop the overflow), centred.
    float scale = std::max(size.x / static_cast<float>(data->width), size.y / static_cast<float>(data->height));
    float dx = (size.x - static_cast<float>(data->width) * scale) / 2;
    float dy = (size.y - static_cast<float>(data->height) * scale) / 2;

    winrt::com_ptr<ID2D1BitmapBrush1> brush;
    auto brushProps = D2D1::BitmapBrushProperties1(D2D1_EXTEND_MODE_CLAMP, D2D1_EXTEND_MODE_CLAMP,
                                                   D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
    winrt::check_hresult(dc->CreateBitmapBrush(bitmap.get(), &brushProps, nullptr, brush.put()));
    brush->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) * D2D1::Matrix3x2F::Translation(dx, dy));
    dc->FillRoundedRectangle(shape, brush.get());
}

} // namespace cup::platform
