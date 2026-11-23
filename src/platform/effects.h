#pragma once
// Direct2D effects described the way Composition reads them, without Win2D
// (decisions 089, 100). Windows takes each effect alone, never one nested in
// another: build a graph of one effect, chaining brushes where it needs more.

#include "platform/win.h"

#include <d2d1_1.h>
#include <d2d1effects.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Effects.h>

#include <vector>

namespace cup::platform {

using winrt::Windows::Graphics::Effects::IGraphicsEffect;
using winrt::Windows::Graphics::Effects::IGraphicsEffectSource;

// The effect with Direct2D id `id`: its inputs in order (for two, 0 is the
// destination, Win2D's Background, 1 the source) and its properties by
// Direct2D's index, boxed as Composition expects (PropertyValue). Named, as
// Composition finds animated properties by "Name.Property".
IGraphicsEffect makeEffect(winrt::hstring const& name, GUID id, std::vector<IGraphicsEffectSource> sources,
                           std::vector<winrt::Windows::Foundation::IInspectable> properties);

// Direct2D's Composite in `mode` over `layers`: each composited onto the
// result so far, in order (0 the first destination).
IGraphicsEffect composite(winrt::hstring const& name, UINT32 mode, std::vector<IGraphicsEffectSource> layers);

} // namespace cup::platform
