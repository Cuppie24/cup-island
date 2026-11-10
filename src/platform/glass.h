#pragma once
// Glass (decision 089): what Windows offers for it. Windows 11 only, looked
// up at runtime (decision 015); on Windows 10 the island stays solid.

#include "core/tokens.h"
#include "platform/win.h"

namespace cup::platform {

// Lets `window`'s visuals show the desktop behind it, blurred by Windows (a
// host backdrop brush). False where Windows can't: Windows 10, where a Win32
// window has no way to, and nothing is changed.
bool allowBackdrop(HWND window);

// The desktop behind the window, blurred by Windows, at `luminosity`'s
// lightness (its alpha is how strongly), cut to `mask`'s alpha: what the
// glass shows through the island. A backdrop can't be a mask brush's source,
// so it's effects (Flood and Blend, then Composite source-in). If Windows
// refuses the brightness step, the plain blur (even false); if it refuses
// that too, no brush: the island stays solid. Each try is logged.
struct Glass {
    wuc::CompositionEffectBrush brush{ nullptr };
    bool even = false;     // at one brightness (luminosity applies)
    bool animated = false; // ...that fadeLuminosity can change
};
Glass maskedBackdrop(wuc::Compositor const& compositor, wuc::CompositionBrush const& mask, Color luminosity);

// Fades the brightness to `color` (decision 089); nothing unless it's animated.
void fadeLuminosity(Glass const& glass, Color color, Ms duration);

// Windows' "Transparency effects" (Personalisation > Colours). Off, glass is
// drawn solid, as Windows draws its own; on where it can't be read.
bool transparencyOn();

} // namespace cup::platform
