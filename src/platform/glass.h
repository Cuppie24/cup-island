#pragma once
// Glass (decision 089): what Windows offers for it. Windows 11 only, looked
// up at runtime (decision 015); on Windows 10 the island stays solid.

#include "platform/win.h"

namespace cup::platform {

// Lets `window`'s visuals show the desktop behind it, blurred by Windows (a
// host backdrop brush). False where Windows can't: Windows 10, where a Win32
// window has no way to, and nothing is changed.
bool allowBackdrop(HWND window);

// How the blurred desktop's brightness is kept in hand: Even (acrylic's
// blend: one brightness, glassLuminosityColor's), Darkened (squeezed into a
// darker range, decision 100), or Plain (not at all).
enum class Look { Plain, Even, Darkened };

// Darkened's range: each channel times `scale`, plus `lift` (glassDarken*).
struct Darken {
    float scale = 1;
    float lift = 0;
};

// The desktop behind the window, blurred by Windows, cut to `mask`'s alpha:
// what the glass shows through the island. First at `luminosity`'s
// lightness (a surface brush painted in glassLuminosityColor: Windows
// refused that colour as a flood effect); if Windows refuses that blend,
// darkened by `darkening`; then plain. A backdrop can't be a mask brush's
// source, so it's effects, cut by Composite source-in. If Windows refuses
// even the plain cut, no brush: the island stays solid. Each outcome is
// logged, and once, probes of which effects Windows takes (decision 100).
struct Glass {
    wuc::CompositionEffectBrush brush{ nullptr };
    Look look = Look::Plain;
};
Glass maskedBackdrop(wuc::Compositor const& compositor, wuc::CompositionBrush const& mask, wuc::CompositionBrush const& luminosity,
                     Darken darkening);

// Windows' "Transparency effects" (Personalisation > Colours). Off, glass is
// drawn solid, as Windows draws its own; on where it can't be read.
bool transparencyOn();

} // namespace cup::platform
