#pragma once
// Glass (decision 089): what Windows offers for it. Windows 11 only, looked
// up at runtime (decision 015); on Windows 10 the island stays solid.

#include "platform/win.h"

namespace cup::platform {

// Lets `window`'s visuals show the desktop behind it, blurred by Windows (a
// host backdrop brush). False where Windows can't: Windows 10, where a Win32
// window has no way to, and nothing is changed.
bool allowBackdrop(HWND window);

// The desktop behind the window, blurred by Windows, cut to `mask`'s alpha:
// what the glass shows through the island. A backdrop can't be a mask
// brush's source, so it's an effect (Composite, source-in). Null where the
// compositor can't make it: no glass, the island stays solid.
wuc::CompositionBrush maskedBackdrop(wuc::Compositor const& compositor, wuc::CompositionBrush const& mask);

// Windows' "Transparency effects" (Personalisation > Colours). Off, glass is
// drawn solid, as Windows draws its own; on where it can't be read.
bool transparencyOn();

} // namespace cup::platform
