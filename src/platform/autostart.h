#pragma once
// "Start with Windows": a value under HKCU\...\CurrentVersion\Run naming this
// exe. Only ever set from the tray menu, so it's the user's opt-in (TR §9);
// the MSIX StartupTask replaces it once we ship MSIX (decision 011).

namespace cup::platform {

// True if the Run value points at this exe. A stale path (the app moved)
// counts as off, so ticking the item again repairs it.
bool startsWithWindows();

// Writes or removes the Run value.
void setStartsWithWindows(bool on);

} // namespace cup::platform
