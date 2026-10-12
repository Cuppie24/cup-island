#pragma once
// "Start with Windows": a value under HKCU\...\CurrentVersion\Run naming this
// exe. Set from the Settings card: on by default the first time the app runs,
// then as the user leaves it (decision 030). The MSIX StartupTask replaces it
// once we ship MSIX (decision 011).

namespace cup::platform {

// True if the Run value points at this exe. A stale path (the app moved)
// counts as off, so ticking the item again repairs it.
bool startsWithWindows();

// Writes or removes the Run value.
void setStartsWithWindows(bool on);

} // namespace cup::platform
