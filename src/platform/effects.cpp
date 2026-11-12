// Direct2D effects for Composition without Win2D (decisions 089, 100).

#include "platform/effects.h"

#include <windows.graphics.effects.interop.h>

#include <cwchar>
#include <utility>

namespace cup::platform {
namespace {

namespace abi = ABI::Windows::Graphics::Effects;

// A Direct2D effect, described the way Composition reads effects (what
// Win2D's effect classes do, without Win2D): its id, its inputs in order
// (for two, 0 is the destination, Win2D's Background, and 1 the source), and
// its properties by Direct2D's index, boxed as Composition expects them.
struct Effect : winrt::implements<Effect, IGraphicsEffect, abi::IGraphicsEffectD2D1Interop> {
    GUID id{};
    std::vector<IGraphicsEffectSource> sources;
    std::vector<winrt::Windows::Foundation::IInspectable> properties;
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
        // Composite's and Blend's mode, as before; nothing here is animated by name.
        if (std::wcscmp(property, L"Mode") != 0 || properties.empty()) return E_INVALIDARG;
        *index = 0;
        *mapping = abi::GRAPHICS_EFFECT_PROPERTY_MAPPING_DIRECT;
        return S_OK;
    }
    IFACEMETHODIMP GetPropertyCount(UINT* count) override
    {
        *count = static_cast<UINT>(properties.size());
        return S_OK;
    }
    IFACEMETHODIMP GetProperty(UINT index, ABI::Windows::Foundation::IPropertyValue** value) override
    {
        if (index >= properties.size()) return E_INVALIDARG;
        try { // nothing may throw back into the compositor
            *value = properties[index].as<ABI::Windows::Foundation::IPropertyValue>().detach();
            return S_OK;
        } catch (...) {
            return winrt::to_hresult();
        }
    }
    IFACEMETHODIMP GetSource(UINT index, abi::IGraphicsEffectSource** result) override
    {
        if (index >= sources.size()) return E_INVALIDARG;
        try {
            *result = sources[index].as<abi::IGraphicsEffectSource>().detach();
            return S_OK;
        } catch (...) {
            return winrt::to_hresult();
        }
    }
    IFACEMETHODIMP GetSourceCount(UINT* count) override
    {
        *count = static_cast<UINT>(sources.size());
        return S_OK;
    }
};

} // namespace

IGraphicsEffect makeEffect(winrt::hstring const& name, GUID id, std::vector<IGraphicsEffectSource> sources,
                           std::vector<winrt::Windows::Foundation::IInspectable> properties)
{
    auto effect = winrt::make_self<Effect>();
    effect->name = name; // every effect named: Composition finds animated properties by "Name.Property"
    effect->id = id;
    effect->sources = std::move(sources);
    effect->properties = std::move(properties);
    return effect.as<IGraphicsEffect>();
}

IGraphicsEffect composite(winrt::hstring const& name, UINT32 mode, std::vector<IGraphicsEffectSource> layers)
{
    return makeEffect(name, CLSID_D2D1Composite, std::move(layers), { winrt::Windows::Foundation::PropertyValue::CreateUInt32(mode) });
}

} // namespace cup::platform
