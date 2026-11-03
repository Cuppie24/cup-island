#pragma once
// Installing, stopping and removing the service (decision 076), for its
// installer: CupIslandIndex.exe --install | --stop | --uninstall, run as
// administrator. Done here rather than with sc.exe, so the service's path is
// always quoted (an unquoted path with spaces lets C:\Program.exe run as
// SYSTEM) and its recovery settings are set in one place.

namespace cup::service {

// Creates the service for this exe, or updates its settings if it exists,
// then starts it: automatic (delayed) start, LocalSystem, restarted after a
// crash. 0 on success, else a Win32 error code, which Inno Setup logs.
int installService();
// Stops it and waits until its process has exited, so its exe can be
// replaced. Not installed or already stopped counts as done.
int stopService();
// Stops it, then removes it. Not installed counts as done.
int uninstallService();

} // namespace cup::service
