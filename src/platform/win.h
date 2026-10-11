#pragma once
// The one place Windows/WinRT headers are included. Only platform/ and app/
// may include this; core/ never does.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <windowsx.h>
#include <dcomp.h>
#include <dispatcherqueue.h>
#include <windows.ui.composition.interop.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.Graphics.DirectX.h>

#include "core/content.h"
#include "core/geometry.h"

namespace cup::platform {

namespace wf = winrt::Windows::Foundation;
namespace wfn = winrt::Windows::Foundation::Numerics;
namespace wuc = winrt::Windows::UI::Composition;

inline wfn::float2 toFloat2(Vec2 v) { return { v.x, v.y }; }
inline wfn::float3 toFloat3(Vec2 v) { return { v.x, v.y, 0.0f }; }
inline winrt::Windows::UI::Color toUiColor(const Color& c) { return { c.a, c.r, c.g, c.b }; }

} // namespace cup::platform
