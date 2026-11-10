#include "platform/glass.h"

#include "platform/log.h"

#include <d2d1_1.h>
#include <d2d1effects.h>
#include <dwmapi.h>
#include <windows.graphics.effects.interop.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Windows.UI.ViewManagement.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <vector>

namespace cup::platform {
namespace {

namespace abi = ABI::Windows::Graphics::Effects;
using winrt::Windows::Graphics::Effects::IGraphicsEffect;
using winrt::Windows::Graphics::Effects::IGraphicsEffectSource;

// A Direct2D effect with two inputs and a mode, described the way
// Composition reads effects (what Win2D's CompositeEffect and BlendEffect
// do, without Win2D). Input 0 is the destination (Win2D's Background), 1 the
// source (Foreground).
struct TwoInputEffect : winrt::implements<TwoInputEffect, IGraphicsEffect, abi::IGraphicsEffectD2D1Interop> {
    GUID id{};
    UINT modeIndex = 0; // the effect's mode property
    UINT32 mode = 0;
    IGraphicsEffectSource destination, source;
    winrt::hstring name;

    winrt::hstring Name() { return name; }
    void Name(winrt::hstring const& value) { name = value; }

    IFACEMETHODIMP GetEffectId(GUID* result) override
    {
        *result = id;
        return S_OK;
    }
    IFACEMETHODIMP GetNamedPropertyMapping(LPCWSTR property, UINT* index, abi::GRAPHICS_EFFECT_PROPERTY_MAPPING* mapping) override
    {
        if (std::wcscmp(property, L"Mode") != 0) return E_INVALIDARG; // nothing here is animated by name
        *index = modeIndex;
        *mapping = abi::GRAPHICS_EFFECT_PROPERTY_MAPPING_DIRECT;
        return S_OK;
    }
    IFACEMETHODIMP GetPropertyCount(UINT* count) override
    {
        *count = 1;
        return S_OK;
    }
    IFACEMETHODIMP GetProperty(UINT index, ABI::Windows::Foundation::IPropertyValue** value) override
    {
        if (index != modeIndex) return E_INVALIDARG;
        try { // nothing may throw back into the compositor
            *value = winrt::Windows::Foundation::PropertyValue::CreateUInt32(mode).as<ABI::Windows::Foundation::IPropertyValue>().detach();
            return S_OK;
        } catch (...) {
            return winrt::to_hresult();
        }
    }
    IFACEMETHODIMP GetSource(UINT index, abi::IGraphicsEffectSource** result) override
    {
        if (index > 1) return E_INVALIDARG;
        try {
            *result = (index == 0 ? destination : source).as<abi::IGraphicsEffectSource>().detach();
            return S_OK;
        } catch (...) {
            return winrt::to_hresult();
        }
    }
    IFACEMETHODIMP GetSourceCount(UINT* count) override
    {
        *count = 2;
        return S_OK;
    }
};

IGraphicsEffect twoInputs(winrt::hstring const& name, GUID id, UINT modeIndex, UINT32 mode, IGraphicsEffectSource const& destination,
                          IGraphicsEffectSource const& source)
{
    auto effect = winrt::make_self<TwoInputEffect>();
    effect->name = name; // every effect named: Composition finds animated properties by "Name.Property"
    effect->id = id;
    effect->modeIndex = modeIndex;
    effect->mode = mode;
    effect->destination = destination;
    effect->source = source;
    return effect.as<IGraphicsEffect>();
}

// Direct2D's Flood: one colour everywhere, with no input (Win2D's
// ColorSourceEffect). Composition takes a colour into an effect only this
// way; a colour brush as an effect's source is refused (0x80070057). Its
// Color can be animated by name (fadeLuminosity), as a Vector4 underneath.
struct Flood : winrt::implements<Flood, IGraphicsEffect, abi::IGraphicsEffectD2D1Interop> {
    std::array<float, 4> color{}; // straight r, g, b, a, 0..1
    winrt::hstring name;

    winrt::hstring Name() { return name; }
    void Name(winrt::hstring const& value) { name = value; }

    IFACEMETHODIMP GetEffectId(GUID* result) override
    {
        *result = CLSID_D2D1Flood;
        return S_OK;
    }
    IFACEMETHODIMP GetNamedPropertyMapping(LPCWSTR property, UINT* index, abi::GRAPHICS_EFFECT_PROPERTY_MAPPING* mapping) override
    {
        if (std::wcscmp(property, L"Color") != 0) return E_INVALIDARG;
        *index = D2D1_FLOOD_PROP_COLOR;
        *mapping = abi::GRAPHICS_EFFECT_PROPERTY_MAPPING_COLOR_TO_VECTOR4;
        return S_OK;
    }
    IFACEMETHODIMP GetPropertyCount(UINT* count) override
    {
        *count = 1;
        return S_OK;
    }
    IFACEMETHODIMP GetProperty(UINT index, ABI::Windows::Foundation::IPropertyValue** value) override
    {
        if (index != D2D1_FLOOD_PROP_COLOR) return E_INVALIDARG;
        try {
            *value = winrt::Windows::Foundation::PropertyValue::CreateSingleArray(winrt::array_view<float const>(color.data(), color.data() + color.size())).as<ABI::Windows::Foundation::IPropertyValue>().detach();
            return S_OK;
        } catch (...) {
            return winrt::to_hresult();
        }
    }
    IFACEMETHODIMP GetSource(UINT, abi::IGraphicsEffectSource**) override { return E_INVALIDARG; }
    IFACEMETHODIMP GetSourceCount(UINT* count) override
    {
        *count = 0;
        return S_OK;
    }
};

IGraphicsEffect flood(winrt::hstring const& name, Color c)
{
    auto effect = winrt::make_self<Flood>();
    effect->name = name;
    effect->color = { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f };
    return effect.as<IGraphicsEffect>();
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

Glass maskedBackdrop(wuc::Compositor const& compositor, wuc::CompositionBrush const& mask, Color luminosity)
{
    using wuc::CompositionEffectSourceParameter;
    // Cut to the island's shape: source-in keeps `inner` where the mask has alpha.
    auto cut = [](IGraphicsEffectSource const& inner) {
        return twoInputs(L"Glass", CLSID_D2D1Composite, D2D1_COMPOSITE_PROP_MODE, D2D1_COMPOSITE_MODE_SOURCE_IN,
                         CompositionEffectSourceParameter(L"Mask"), inner);
    };
    // The backdrop at the luminosity colour's lightness, keeping its own hue:
    // Windows' acrylic recipe (WinUI's AcrylicBrush). Direct2D's Colour and
    // Luminosity blend modes are swapped against their names, so "Colour" is
    // the one that takes the top layer's lightness, as WinUI notes and uses.
    auto evenOut = [&](UINT32 mode) {
        return twoInputs(L"Even", CLSID_D2D1Blend, D2D1_BLEND_PROP_MODE, mode, CompositionEffectSourceParameter(L"Backdrop"),
                         flood(L"Luminosity", luminosity));
    };
    const char* step = "";
    // Each step named, so a refusal in the log says which one Windows turned down.
    auto make = [&](IGraphicsEffect const& effect, bool animated) {
        step = "factory";
        // An animated brightness (fadeLuminosity) is declared up front.
        auto factory = animated ? compositor.CreateEffectFactory(effect, std::vector<winrt::hstring>{ L"Luminosity.Color" })
                                : compositor.CreateEffectFactory(effect);
        step = "brush";
        auto brush = factory.CreateBrush();
        step = "mask";
        brush.SetSourceParameter(L"Mask", mask);
        step = "backdrop";
        brush.SetSourceParameter(L"Backdrop", compositor.CreateHostBackdropBrush()); // already blurred by Windows
        return brush;
    };
    // The evened-out blur, with its brightness animated, then fixed; Windows
    // refused the first on the first runs (factory: 0x80070057), so each try is
    // logged until one is taken.
    struct Try {
        const char* what;
        bool animated;
    };
    for (Try t : { Try{ "animated", true }, Try{ "fixed", false } }) {
        try {
            auto brush = make(cut(evenOut(D2D1_BLEND_MODE_COLOR)), t.animated);
            logLine("glass: backdrop ready, at one brightness (%s)", t.what);
            return { brush, true, t.animated };
        } catch (winrt::hresult_error const& e) {
            logLine("glass: no even brightness, %s (%s: 0x%08lx)", t.what, step, static_cast<unsigned long>(e.code()));
        }
    }
    // What Windows objects to, asked once: a blend in a mode it surely has
    // (Multiply), and the flood colour alone. Only logged, never drawn.
    static bool probed = false;
    if (!probed) {
        probed = true;
        auto probe = [&](const char* what, IGraphicsEffect const& effect) {
            try {
                compositor.CreateEffectFactory(effect);
                logLine("glass: probe: Windows accepts %s", what);
            } catch (winrt::hresult_error const& e) {
                logLine("glass: probe: Windows refuses %s (0x%08lx)", what, static_cast<unsigned long>(e.code()));
            }
        };
        probe("a Multiply blend", cut(evenOut(D2D1_BLEND_MODE_MULTIPLY)));
        probe("a Luminosity blend", cut(evenOut(D2D1_BLEND_MODE_LUMINOSITY)));
        probe("a flood colour", cut(flood(L"Luminosity", luminosity)));
    }
    try {
        auto brush = make(cut(CompositionEffectSourceParameter(L"Backdrop")), false);
        logLine("glass: backdrop ready, plain blur");
        return { brush, false, false };
    } catch (winrt::hresult_error const& e) {
        logLine("glass: no backdrop brush (%s: 0x%08lx): the island stays solid", step, static_cast<unsigned long>(e.code()));
        return {};
    }
}

void fadeLuminosity(Glass const& glass, Color color, Ms duration)
{
    if (!glass.brush || !glass.animated) return; // fixed: a new brightness shows when the glass is next built
    auto anim = glass.brush.Compositor().CreateColorKeyFrameAnimation();
    anim.InsertKeyFrame(1.0f, toUiColor(color));
    anim.Duration(std::max(duration, Ms{ 1 }));
    anim.InterpolationColorSpace(wuc::CompositionColorSpace::Rgb); // straight across, as fadeColor does
    glass.brush.StartAnimation(L"Luminosity.Color", anim);
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
