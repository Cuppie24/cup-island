#include "platform/element_painting.h"

#include "core/tokens.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace cup::platform {
namespace {

std::wstring widen(const std::string& utf8)
{
    if (utf8.empty()) return {};
    int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), length);
    return out;
}

bool hasFont(IDWriteFactory* dw, const wchar_t* family)
{
    winrt::com_ptr<IDWriteFontCollection> fonts;
    UINT32 index = 0;
    BOOL exists = FALSE;
    if (SUCCEEDED(dw->GetSystemFontCollection(fonts.put())))
        fonts->FindFamilyName(family, &index, &exists);
    return exists;
}

// Windows 11 fonts first, Windows 10 equivalents as fallback.
const wchar_t* textFamily(IDWriteFactory* dw)
{
    static const wchar_t* family = hasFont(dw, L"Segoe UI Variable Text") ? L"Segoe UI Variable Text" : L"Segoe UI";
    return family;
}

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
    case Glyph::Battery: return L"\uE83F";
    case Glyph::Charging: return L"\uE945"; // lightning bolt
    case Glyph::Bluetooth: return L"\uE702";
    case Glyph::Sun: return L"\uE706";  // Brightness
    case Glyph::Moon: return L"\uE708"; // QuietHours (crescent)
    case Glyph::Copy: return L"\uE8C8";
    }
    return L"?";
}

winrt::com_ptr<IDWriteTextFormat> format(IDWriteFactory* dw, const wchar_t* family, float size,
                                         DWRITE_FONT_WEIGHT weight, DWRITE_TEXT_ALIGNMENT align)
{
    winrt::com_ptr<IDWriteTextFormat> f;
    winrt::check_hresult(dw->CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                              DWRITE_FONT_STRETCH_NORMAL, size, L"", f.put()));
    f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    f->SetTextAlignment(align);
    f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    // Long titles end in "…" instead of being cut mid-letter.
    DWRITE_TRIMMING trimming{ DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    winrt::com_ptr<IDWriteInlineObject> ellipsis;
    if (SUCCEEDED(dw->CreateEllipsisTrimmingSign(f.get(), ellipsis.put()))) f->SetTrimming(&trimming, ellipsis.get());
    return f;
}

D2D1::ColorF toD2d(const Color& c) { return D2D1::ColorF(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f); }

winrt::com_ptr<ID2D1SolidColorBrush> brushOf(ID2D1DeviceContext* dc, const Color& color)
{
    winrt::com_ptr<ID2D1SolidColorBrush> brush;
    winrt::check_hresult(dc->CreateSolidColorBrush(toD2d(color), brush.put()));
    return brush;
}

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
    DWRITE_FONT_WEIGHT weight;
};

TextLook lookOf(TextStyle style)
{
    const Tokens& t = tokens();
    switch (style) {
    case TextStyle::Title: return { t.titleFontSize, t.titleTextColor, DWRITE_FONT_WEIGHT_SEMI_BOLD };
    case TextStyle::Body: break;
    case TextStyle::Caption:
    case TextStyle::Digits: return { t.captionFontSize, t.captionTextColor, DWRITE_FONT_WEIGHT_NORMAL };
    case TextStyle::Display: return { t.displayFontSize, t.titleTextColor, DWRITE_FONT_WEIGHT_SEMI_LIGHT }; // big and light, like a clock face
    }
    return { t.bodyFontSize, t.bodyTextColor, DWRITE_FONT_WEIGHT_NORMAL };
}

winrt::com_ptr<IDWriteTextFormat> formatOf(IDWriteFactory* dw, const Text& text)
{
    TextLook look = lookOf(text.style);
    return format(dw, textFamily(dw), look.fontSize, look.weight,
                  text.centred ? DWRITE_TEXT_ALIGNMENT_CENTER
                  : text.alignEnd ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
}

} // namespace

void paintText(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Text& text, Vec2 size)
{
    drawString(dc, formatOf(dw, text).get(), widen(text.value), size, lookOf(text.style).color);
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
    auto f = format(dw, iconFamily(dw), fontSize, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
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
