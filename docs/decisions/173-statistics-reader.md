# 173: The statistics reader: documented, unprivileged sources only; one-shot Bluetooth

Status: Accepted (completes 171 and 172; under 042's condition). Windows-only
code, written on Linux: it compiles in CI, and **nothing here has run on a
real PC yet**. The Bluetooth part must not ship before 042's test: music
over Bluetooth headphones, the card open for a few minutes, no stutter.

**Context.** 171 and 172 left the Windows half open: something has to fill
`StatsReading`. Much of what the pages show has no clean source on Windows,
and three things had to hold: it runs on Windows 10 1809 (decision 015),
without admin rights, a driver or a vendor's SDK; nothing is read on the UI
thread (decision 144); and nothing is watched (decision 042).

**Decision.**
- **`StatsReader` owns one thread** that sleeps on a condition variable.
  `SystemStatisticsProvider` (the pure provider plus the reader) hands it
  each request with the open page and posts the answer back to the UI
  thread, guarded by an `alive` flag like media's. A request during a read
  is skipped. Closed, the provider stops asking and the thread sleeps;
  after `idleClose` (30 s) it also closes its counters. `stop()` joins:
  every look is a local call that takes milliseconds, and exit has its 3 s
  safety net. Every look runs inside a guard (decision 158): one that
  throws or fails leaves its values out, and the page shows its empty state.
- **Rates need two samples.** With no recent sample (the card just opened,
  or the last is older than `staleAfter`), the reader samples, waits
  `firstSpan` (250 ms) and reads: the first card shows numbers at once, and
  never an average over the hour it was closed.
- **Sources**, all present since Windows 7 unless noted:

  | Reading | Source |
  | --- | --- |
  | CPU load, per core | PDH `\Processor Information(*)\% Processor Utility` (Windows 8+, Task Manager's number), else `% Processor Time`, else `\Processor(*)`; the total falls back to `GetSystemTimes` where the counters are broken |
  | CPU name | registry, `HARDWARE\DESCRIPTION\System\CentralProcessor\0` |
  | Memory | `GlobalMemoryStatusEx`; cached = the three `\Memory\Standby Cache … Bytes` counters, else `GetPerformanceInfo`'s system cache |
  | Drives | `GetLogicalDrives`, fixed ones only (a dead network drive hangs), `GetVolumeInformation`, `GetDiskFreeSpaceEx`; activity: `\PhysicalDisk(_Total)\Disk Read/Write Bytes/sec` |
  | Network | `GetIfTable2`, hardware interfaces only (filter and virtual layers repeat their adapter's bytes) |
  | GPU load, engines, processes | PDH `\GPU Engine(*)\Utilization Percentage` (Windows 10 1709+) |
  | GPU memory, name | PDH `\GPU Adapter Memory(*)\Dedicated Usage`; DXGI `EnumAdapters1` for the total and the name |
  | Processes | PDH `\Process(*)\% Processor Time`, `Working Set - Private`, `ID Process`: no process is opened; only on the CPU, GPU and memory pages |
  | CPU temperature | PDH `\Thermal Zone Information(*)\High Precision Temperature`, the hottest zone |
  | Bluetooth | SetupAPI, the nodes under `BTHENUM`, `BTHLE`, `BTHLEDEVICE` |

  PDH counters are added by their English names (`PdhAddEnglishCounter`).
  A counter a PC doesn't have isn't added, and its values stay empty.
- **What the counters mean is pure and tested** (`stats_counters`,
  `stats_network`, `stats_bluetooth`): instance names taken apart, a GPU's
  load (an engine's processes added, the adapter's busiest engine, the
  busiest hardware adapter), processes gathered by name, adapters' deltas,
  the adapter in use, devices from nodes. The Windows files only fetch.
- **Bluetooth is one listing, of what Windows already keeps.** Each
  `bluetoothNodes()` call builds a device-information set of present nodes,
  reads six properties per node from the PnP manager (name, container,
  the battery level `{104EA319-6EE2-4701-BD47-8DDBF425BBE5} 2`, class of
  device, connected), and destroys the set. No `DeviceWatcher`, no
  notification, no `Windows.Devices.Bluetooth`, no device opened, nothing
  sent over the radio. Every `bluetoothEvery` (5) readings, and at once
  when its page opens. A device Windows marks disconnected is left out
  (its level is the last one heard).
- **"Today" for the network** is bytes since local midnight, kept between
  runs (`Host::save`, key `network-day`, written when the card closes):
  adapters' counters go on while the card is closed, so each look adds what
  went through since the last one, a reboot included. The first look of a
  day starts from everything since boot if the PC booted today, else from
  0: what passed between midnight and that look can't be known without
  reading at midnight, which would mean running with the card closed.

**Deliberately not read, and what shows instead.**
- **Temperatures.** Windows has no documented, unprivileged API for the
  CPU's or the GPU's sensors: every tool that shows them loads a kernel
  driver or a vendor SDK, and WMI's thermal zone class needs admin. The one
  source taken is the ACPI thermal zone counter, which needs neither. On
  many laptops it is the CPU's zone; most desktops have none, and some
  boards report a constant made-up value. So: the CPU page shows that
  zone's temperature where there is one (labelled as the CPU's, which is
  an approximation), nothing where there isn't; per-core and GPU
  temperatures are always empty.
- **Earbuds' parts** (left, right, case): Windows keeps one level a device.
- **The Wi-Fi network's name.** Asking for it counts as using location on
  Windows 11 24H2 (a prompt or a refusal, and the location icon). The
  adapter's connection name shows instead ("Wi-Fi", "Ethernet 2").
- **Apps' display names.** Lists show process names ("chrome", "dwm"):
  a display name means opening each process and its file's version info.
- **Bluetooth LE devices' levels** show only where Windows itself put one
  on the node; the reader never asks a device (that is radio traffic).

**Not taken.** `NtQuerySystemInformation` for processes: faster, but its
structures aren't documented. WMI: slow, and a COM server. A WinRT
`DeviceInformation` query for Bluetooth: it goes through the service that
042's watcher did. Reader timings as tokens: they'd be the first read off
the UI thread, and this change stayed out of `core/`; they are
`StatsReaderSettings`' defaults for now.

**Consequences.** Four more system libraries (pdh, iphlpapi, setupapi,
psapi), all in every Windows since 7. The first reading takes `firstSpan`
longer than the rest. To check on Windows, besides 042's test: the log's
`statistics: counters opened (…)` line says which counters the PC has;
numbers against Task Manager (CPU, GPU, memory, disk, network); a PC with
two GPUs; a language other than English; closing the card and seeing the
thread idle; the connected property (`{83DA6326-…} 15`), which is not
documented: a paired but switched-off headset must not be listed.
