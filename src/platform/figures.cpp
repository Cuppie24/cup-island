#include "platform/figures.h"

#include <string>

namespace cup::platform {

void useTabularFigures(IDWriteFactory* dw, IDWriteTextLayout* layout, UINT32 length)
{
    winrt::com_ptr<IDWriteTypography> typography;
    if (FAILED(dw->CreateTypography(typography.put()))) return;
    if (FAILED(typography->AddFontFeature(DWRITE_FONT_FEATURE{ DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES, 1 }))) return;
    layout->SetTypography(typography.get(), DWRITE_TEXT_RANGE{ 0, length });
}

float figureNudge(IDWriteFactory* dw, IDWriteTextLayout* layout)
{
    DWRITE_LINE_METRICS line{};
    UINT32 lines = 0;
    if (FAILED(layout->GetLineMetrics(&line, 1, &lines)) || lines != 1) return 0;

    // The font the layout uses at its start: its family, in its collection (the system's if none was given).
    winrt::com_ptr<IDWriteFontCollection> fonts;
    if (FAILED(layout->GetFontCollection(0, fonts.put())) || !fonts) {
        fonts = nullptr;
        if (FAILED(dw->GetSystemFontCollection(fonts.put()))) return 0;
    }
    UINT32 nameLength = 0;
    if (FAILED(layout->GetFontFamilyNameLength(0, &nameLength))) return 0;
    std::wstring name(nameLength + 1, L'\0');
    if (FAILED(layout->GetFontFamilyName(0, name.data(), nameLength + 1))) return 0;
    UINT32 index = 0;
    BOOL exists = FALSE;
    if (FAILED(fonts->FindFamilyName(name.c_str(), &index, &exists)) || !exists) return 0;
    DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
    layout->GetFontWeight(0, &weight);
    winrt::com_ptr<IDWriteFontFamily> family;
    winrt::com_ptr<IDWriteFont> font;
    if (FAILED(fonts->GetFontFamily(index, family.put())) ||
        FAILED(family->GetFirstMatchingFont(weight, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, font.put())))
        return 0;
    DWRITE_FONT_METRICS metrics{};
    font->GetMetrics(&metrics);
    float size = 0;
    if (FAILED(layout->GetFontSize(0, &size)) || metrics.designUnitsPerEm == 0) return 0;

    float cap = size * static_cast<float>(metrics.capHeight) / static_cast<float>(metrics.designUnitsPerEm);
    float box = layout->GetMaxHeight();
    float top = (box - line.height) / 2; // the line, centred in the box (DWRITE_PARAGRAPH_ALIGNMENT_CENTER)
    return box / 2 - (top + line.baseline - cap / 2);
}

} // namespace cup::platform
