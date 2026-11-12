#include "platform/glass.h"

#include "platform/effects.h"
#include "platform/log.h"

#include <d2d1_1.h>
#include <d2d1effects.h>
#include <dwmapi.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Windows.UI.ViewManagement.h>

#include <array>
#include <vector>

namespace cup::platform {
namespace {

using winrt::Windows::Graphics::Effects::IGraphicsEffect;
using winrt::Windows::Graphics::Effects::IGraphicsEffectSource;

using winrt::Windows::Foundation::PropertyValue;
using wuc::CompositionEffectSourceParameter;

IGraphicsEffect composite(winrt::hstring const& name, UINT32 mode, IGraphicsEffectSource const& destination, IGraphicsEffectSource const& source)
{
    return composite(name, mode, std::vector<IGraphicsEffectSource>{ destination, source });
}

IGraphicsEffect blend(UINT32 mode, IGraphicsEffectSource const& destination, IGraphicsEffectSource const& source, winrt::hstring const& name = L"Even")
{
    return makeEffect(name, CLSID_D2D1Blend, { destination, source }, { PropertyValue::CreateUInt32(mode) });
}

// Every colour channel scaled by `scale` and lifted by `lift`, alpha kept:
// the whole range squeezed into [lift, scale + lift] (decision 100).
IGraphicsEffect darken(IGraphicsEffectSource const& source, float scale, float lift)
{
    // Direct2D's 5x4 matrix, row-major: rows are the input red, green, blue,
    // alpha and a constant 1; columns the output red, green, blue, alpha.
    std::array<float, 20> m{};
    m[0 * 4 + 0] = m[1 * 4 + 1] = m[2 * 4 + 2] = scale;
    m[3 * 4 + 3] = 1;
    m[4 * 4 + 0] = m[4 * 4 + 1] = m[4 * 4 + 2] = lift;
    return makeEffect(L"Darken", CLSID_D2D1ColorMatrix, { source },
                      { PropertyValue::CreateSingleArray(winrt::array_view<float const>(m.data(), m.data() + m.size())),
                        PropertyValue::CreateUInt32(D2D1_COLORMATRIX_ALPHA_MODE_PREMULTIPLIED), PropertyValue::CreateBoolean(false) });
}

// Cut to the island's shape: source-in keeps `inner` where the mask has alpha.
IGraphicsEffect cut(IGraphicsEffectSource const& inner)
{
    return composite(L"Glass", D2D1_COMPOSITE_MODE_SOURCE_IN, CompositionEffectSourceParameter(L"Mask"), inner);
}

// Whether Windows takes an effect at all, logged once (decision 100): each
// tests one guess about why it refuses every effect but the cut.
void probe(wuc::Compositor const& compositor, const char* what, IGraphicsEffect const& effect)
{
    try {
        compositor.CreateEffectFactory(effect);
        logLine("glass: probe: %s: yes", what);
    } catch (winrt::hresult_error const& e) {
        logLine("glass: probe: %s: no (0x%08lx)", what, static_cast<unsigned long>(e.code()));
    }
}

void probeOnce(wuc::Compositor const& compositor)
{
    static bool probed = false;
    if (probed) return;
    probed = true;
    auto backdrop = CompositionEffectSourceParameter(L"Backdrop");
    auto luminosity = CompositionEffectSourceParameter(L"Luminosity");
    // Nested: is it putting one effect in another? The cut works alone.
    probe(compositor, "a composite in the cut (nesting)", cut(composite(L"Over", D2D1_COMPOSITE_MODE_SOURCE_OVER, backdrop, luminosity)));
    // Alone: is it the blend, or the blend inside the cut?
    probe(compositor, "a Multiply blend alone", blend(D2D1_BLEND_MODE_MULTIPLY, backdrop, luminosity));
    probe(compositor, "the acrylic blend alone", blend(D2D1_BLEND_MODE_COLOR, backdrop, luminosity));
    // No properties at all: is it how a setting is passed?
    probe(compositor, "an effect with no settings (luminance to alpha)", makeEffect(L"Plain", CLSID_D2D1LuminanceToAlpha, { backdrop }, {}));
    probe(compositor, "the colour matrix alone", darken(backdrop, 0.35f, 0.08f));
    // The first run's answers: every effect alone yes, nested no. Why nesting:
    // an inner effect with no settings (is it its settings?), and one with no
    // name (is it the name?).
    probe(compositor, "a no-settings effect in the cut", cut(makeEffect(L"Plain", CLSID_D2D1LuminanceToAlpha, { backdrop }, {})));
    probe(compositor, "an unnamed blend in the cut", cut(blend(D2D1_BLEND_MODE_MULTIPLY, backdrop, luminosity, L"")));
}

} // namespace

bool allowBackdrop(HWND window)
{
    // Universal API contract 14 is Windows 11 (build 22000): the first that
    // lets a window that isn't UWP use a host backdrop brush. Windows 10's DWM
    // doesn't know the attribute, so it isn't even asked there.
    using winrt::Windows::Foundation::Metadata::ApiInformation;
    if (!ApiInformation::IsApiContractPresent(L"Windows.Foundation.UniversalApiContract", 14)) return false;
    constexpr DWORD kUseHostBackdropBrush = 17; // DWMWA_USE_HOSTBACKDROPBRUSH; older SDKs don't name it
    BOOL on = TRUE;
    HRESULT result = DwmSetWindowAttribute(window, kUseHostBackdropBrush, &on, sizeof on);
    if (FAILED(result)) logLine("glass: Windows refused a backdrop for a window (0x%08lx)", static_cast<unsigned long>(result));
    return SUCCEEDED(result);
}

Glass maskedBackdrop(wuc::Compositor const& compositor, wuc::CompositionBrush const& mask, wuc::CompositionBrush const& luminosity,
                     Darken darkening)
{
    probeOnce(compositor);
    using wuc::CompositionEffectBrush;
    const char* step = "";
    // Windows takes every effect alone but none inside another (the probes,
    // decision 100). So the brightness step is a brush of its own, over the
    // backdrop, and the cut takes that whole brush as its input ("Inner"):
    // two brushes chained, no effect nested. Each step named, so a refusal in
    // the log says which one Windows turned down.
    auto chained = [&](IGraphicsEffect const& inner, bool usesLuminosity) {
        step = "inner factory";
        CompositionEffectBrush innerBrush = compositor.CreateEffectFactory(inner).CreateBrush();
        step = "backdrop";
        innerBrush.SetSourceParameter(L"Backdrop", compositor.CreateHostBackdropBrush()); // already blurred by Windows
        if (usesLuminosity) {
            step = "luminosity";
            innerBrush.SetSourceParameter(L"Luminosity", luminosity);
        }
        step = "factory";
        CompositionEffectBrush brush = compositor.CreateEffectFactory(cut(CompositionEffectSourceParameter(L"Inner"))).CreateBrush();
        step = "mask";
        brush.SetSourceParameter(L"Mask", mask);
        step = "inner";
        brush.SetSourceParameter(L"Inner", innerBrush);
        return brush;
    };
    // The backdrop at the luminosity colour's lightness, keeping its own hue:
    // Windows' acrylic recipe (WinUI's AcrylicBrush). Direct2D's Colour and
    // Luminosity blend modes are swapped against their names, so "Colour" is
    // the one that takes the top layer's lightness, as WinUI notes and uses.
    try {
        auto brush = chained(blend(D2D1_BLEND_MODE_COLOR, CompositionEffectSourceParameter(L"Backdrop"),
                                   CompositionEffectSourceParameter(L"Luminosity")),
                             true);
        logLine("glass: backdrop ready, at one brightness");
        return { brush, Look::Even };
    } catch (winrt::hresult_error const& e) {
        logLine("glass: no even brightness (%s: 0x%08lx)", step, static_cast<unsigned long>(e.code()));
    }
    // Refused: the backdrop's brightness squeezed into a darker range
    // instead, so a white window behind isn't a white island (decision 100).
    try {
        auto brush = chained(darken(CompositionEffectSourceParameter(L"Backdrop"), darkening.scale, darkening.lift), false);
        logLine("glass: backdrop ready, darkened");
        return { brush, Look::Darkened };
    } catch (winrt::hresult_error const& e) {
        logLine("glass: no darkening (%s: 0x%08lx)", step, static_cast<unsigned long>(e.code()));
    }
    try {
        step = "factory";
        CompositionEffectBrush brush = compositor.CreateEffectFactory(cut(CompositionEffectSourceParameter(L"Backdrop"))).CreateBrush();
        step = "mask";
        brush.SetSourceParameter(L"Mask", mask);
        step = "backdrop";
        brush.SetSourceParameter(L"Backdrop", compositor.CreateHostBackdropBrush());
        logLine("glass: backdrop ready, plain blur");
        return { brush, Look::Plain };
    } catch (winrt::hresult_error const& e) {
        logLine("glass: no backdrop brush (%s: 0x%08lx): the island stays solid", step, static_cast<unsigned long>(e.code()));
        return {};
    }
}

bool transparencyOn()
{
    try {
        return winrt::Windows::UI::ViewManagement::UISettings().AdvancedEffectsEnabled();
    } catch (winrt::hresult_error const&) {
        return true; // can't tell: the user's pick decides alone
    }
}

} // namespace cup::platform
