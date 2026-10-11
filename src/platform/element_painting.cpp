#include "platform/element_painting.h"

#include "core/tokens.h"

#include <algorithm>
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
    case Glyph::Source: return L"";
    case Glyph::Music: return L"";
    case Glyph::Warning: return L"";
    case Glyph::NextActivity: return L"";
    case Glyph::Pin: return L"";
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

void drawString(ID2D1DeviceContext* dc, IDWriteTextFormat* f, const std::wstring& text, Vec2 size, float alpha)
{
    winrt::com_ptr<ID2D1SolidColorBrush> brush;
    winrt::check_hresult(dc->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, alpha), brush.put()));
    dc->DrawText(text.c_str(), static_cast<UINT32>(text.size()), f, D2D1::RectF(0, 0, size.x, size.y), brush.get(),
                 D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

} // namespace

void paintText(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Text& text, Vec2 size)
{
    const Tokens& t = tokens();
    float fontSize = t.bodyFontSize, alpha = 0.7f;
    DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
    switch (text.style) {
    case TextStyle::Title: fontSize = t.titleFontSize; alpha = 1.0f; weight = DWRITE_FONT_WEIGHT_SEMI_BOLD; break;
    case TextStyle::Body: break;
    case TextStyle::Caption:
    case TextStyle::Digits: fontSize = t.captionFontSize; alpha = 0.55f; break;
    }
    auto f = format(dw, textFamily(dw), fontSize, weight, text.alignEnd ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
    drawString(dc, f.get(), widen(text.value), size, alpha);
}

void paintGlyph(ID2D1DeviceContext* dc, IDWriteFactory* dw, Glyph glyph, float fontSize, Vec2 size, float alpha)
{
    auto f = format(dw, iconFamily(dw), fontSize, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
    drawString(dc, f.get(), codepoint(glyph), size, alpha);
}

void paintImage(ID2D1DeviceContext* dc, const ImageData* data, Vec2 size, float cornerRadius)
{
    D2D1_ROUNDED_RECT shape{ D2D1::RectF(0, 0, size.x, size.y), cornerRadius, cornerRadius };
    if (!data || data->width <= 0 || data->height <= 0 ||
        data->bgra.size() < static_cast<size_t>(data->width) * static_cast<size_t>(data->height) * 4) {
        winrt::com_ptr<ID2D1SolidColorBrush> tile;
        winrt::check_hresult(dc->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 0.12f), tile.put()));
        dc->FillRoundedRectangle(shape, tile.get());
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
