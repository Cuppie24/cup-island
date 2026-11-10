#include "platform/glass.h"

#include "platform/log.h"

#include <d2d1_1.h>
#include <d2d1effects.h>
#include <dwmapi.h>
#include <windows.graphics.effects.interop.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Windows.UI.ViewManagement.h>

#include <cwchar>

namespace cup::platform {
namespace {

namespace abi = ABI::Windows::Graphics::Effects;
using winrt::Windows::Graphics::Effects::IGraphicsEffect;
using winrt::Windows::Graphics::Effects::IGraphicsEffectSource;

// Direct2D's Composite effect, described the way Composition reads effects
// (what Win2D's CompositeEffect does, without Win2D). Input 0 is the
// destination, 1 the source; source-in keeps the source where the
// destination has alpha: the backdrop, cut to the island's shape.
struct SourceIn : winrt::implements<SourceIn, IGraphicsEffect, abi::IGraphicsEffectD2D1Interop> {
    IGraphicsEffectSource destination, source;
    winrt::hstring name;

    winrt::hstring Name() { return name; }
    void Name(winrt::hstring const& value) { name = value; }

    IFACEMETHODIMP GetEffectId(GUID* id) override
    {
        *id = CLSID_D2D1Composite;
        return S_OK;
    }
    IFACEMETHODIMP GetNamedPropertyMapping(LPCWSTR property, UINT* index, abi::GRAPHICS_EFFECT_PROPERTY_MAPPING* mapping) override
    {
        if (std::wcscmp(property, L"Mode") != 0) return E_INVALIDARG; // nothing here is animated by name
        *index = D2D1_COMPOSITE_PROP_MODE;
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
        if (index != D2D1_COMPOSITE_PROP_MODE) return E_INVALIDARG;
        try { // nothing may throw back into the compositor
            auto mode = winrt::Windows::Foundation::PropertyValue::CreateUInt32(D2D1_COMPOSITE_MODE_SOURCE_IN);
            *value = mode.as<ABI::Windows::Foundation::IPropertyValue>().detach();
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

wuc::CompositionBrush maskedBackdrop(wuc::Compositor const& compositor, wuc::CompositionBrush const& mask)
{
    try {
        auto effect = winrt::make_self<SourceIn>();
        effect->name = L"Glass";
        effect->destination = wuc::CompositionEffectSourceParameter(L"Mask");
        effect->source = wuc::CompositionEffectSourceParameter(L"Backdrop");
        auto brush = compositor.CreateEffectFactory(effect.as<IGraphicsEffect>()).CreateBrush();
        brush.SetSourceParameter(L"Mask", mask);
        brush.SetSourceParameter(L"Backdrop", compositor.CreateHostBackdropBrush()); // already blurred by Windows
        return brush;
    } catch (winrt::hresult_error const& e) {
        logLine("glass: no backdrop brush (0x%08lx): the island stays solid", static_cast<unsigned long>(e.code()));
        return nullptr;
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
