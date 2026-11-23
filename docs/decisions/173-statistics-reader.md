# 173: The statistics reader: unprivileged sources only, opened per request; one-shot Bluetooth

Status: Accepted (completes 171; rewritten for 174's contract; under 042's
condition). Windows-only code, written on Linux: it compiles in CI, and
**nothing here has run on a real PC yet**. The Bluetooth part must not ship
before 042's test: music over Bluetooth headphones, the card open for a few
minutes and then a device picked for the pill, no stutter.

**Context.** Something has to fill `StatsReading`. Much of what a system
monitor shows has no clean source on Windows, and four things had to hold:
it runs on Windows 10 1809 (decision 015), without admin rights, a driver
or a vendor's SDK; nothing is read on the UI thread (decision 144); nothing
is watched (decision 042); and since 174 a reading is asked for every few
seconds for as long as the app runs when the pill shows one, so a request
must cost only what it asks for.

**Decision.**
- **`StatsReader` owns one thread** that sleeps on a condition variable.
  `SystemStatisticsProvider` (the pure provider plus the reader) hands it
  each `StatsRequest` with the reader's timings and posts the reading back
  to the UI thread, guarded by an `alive` flag like media's. A request
  that comes during a read is added to what's waiting and read right after
  (a Bluetooth listing due once a minute is never dropped). When the
  provider stops asking, the thread sleeps. `stop()` joins: every look is a
  local call that takes milliseconds, and exit has its 3 s safety net.
  Every look runs inside a guard (decision 158): one that throws or fails
  leaves its values out, and the tile shows its empty state.
- **A source is opened when a request first asks for it, and closed when
  none has for `statsIdleClose`** (30 s). Each has its own PDH query, so
  collecting one never touches another:

  | Asked | Opened and read | Rate |
  | --- | --- | --- |
  | `cpuLoad` | a query with one counter, `\Processor Information(_Total)\% Processor Utility` (Windows 8+, Task Manager's number), else `% Processor Time`, else `\Processor(_Total)`; `GetSystemTimes` as the fallback where the counters are broken; `GetActiveProcessorCount` for `cpuCores` | yes |
  | `cpuTemperature` | a query with `\Thermal Zone Information(*)\High Precision Temperature` and `Temperature`: the hottest zone | no |
  | `gpuLoad` | a query with `\GPU Engine(*)\Utilization Percentage` (Windows 10 1709+): the costly one, an instance per process and engine; the adapter list | yes |
  | `gpuTemperature` | the adapter list, and `D3DKMTQueryAdapterInfo` on the chosen one (below) | no |
  | `memory` | `GlobalMemoryStatusEx`: nothing opened | no |
  | `disk` | `GetDiskFreeSpaceEx` on the system drive: nothing opened | no |
  | `network` | `GetIfTable2`, hardware interfaces only (filter and virtual layers repeat their adapter's bytes): nothing stays open | yes |
  | `bluetooth` | SetupAPI, the nodes under `BTHENUM`, `BTHLE`, `BTHLEDEVICE`: nothing stays open | no |

  So a pill of memory alone never opens a counter, and one of the CPU's
  load keeps a single counter open. With something picked, requests come
  every `statsPillRefresh` (5 s), sooner than the idle close: what the pill
  reads stays open, which is the cheapest (a query opened again would need
  a first sample each time); what only the card read (the GPU's engines)
  closes 30 s after the card does.
- **Rates need two samples, each source its own.** A rate asked for with
  no sample of its own newer than `statsStaleAfter` gets one, and the
  reading waits `statsFirstSpan` (250 ms): the first card shows numbers at
  once, and never an average over the hour it was closed. A request with
  no such source (memory, the disk, Bluetooth, or rates read 5 s ago)
  doesn't wait.
- **The GPU is the busiest hardware adapter**, the one with the most
  dedicated memory among equals (idle, or when only its temperature is
  asked: the discrete card). DXGI (`EnumAdapters1`) is kept for exactly
  this: it says which adapters are hardware (the counters also list the
  software renderer, which would give a PC without a GPU a GPU tile at 0 %)
  and their memory. Listed once per use, not per reading.
- **GPU temperature is the adapter's own report to Windows**, the number
  Task Manager shows: `D3DKMTQueryAdapterInfo` with
  `KMTQAITYPE_ADAPTERPERFDATA` (`D3DKMT_ADAPTER_PERFDATA::Temperature`,
  tenths of a degree Celsius; Windows 10 1803+, WDDM 2.4+ drivers), on the
  adapter opened by its luid and closed again at once. No admin rights, no
  vendor SDK. The three functions are found in gdi32 by name at runtime
  (decision 015), the structures are the Windows SDK's own header's
  (`d3dkmthk.h`, used only if the kit has it: `__has_include`), and a
  failure of any step, or a 0, is no reading. **Unverified**: Microsoft's
  documentation couldn't be reached while writing it. The function names,
  the three call structures and the query's number (62) match Wine's
  header; the performance structure and its unit are from memory, which is
  why it is never declared here. Whether a normal user's process gets an
  answer, and from which drivers, only a real PC can say.
- **What the counters mean is pure and tested** (`stats_counters`,
  `stats_network`, `stats_bluetooth`): a GPU's load (an engine's processes
  added, the adapter's busiest engine), which adapter is the reading's,
  adapters' byte deltas, devices from nodes. The Windows files only fetch.
  PDH counters are added by their English names (`PdhAddEnglishCounter`);
  one a PC doesn't have isn't added, and its value stays empty.
- **Bluetooth is one listing, only when the request asks, of what Windows
  already keeps.** Each `bluetoothNodes()` call builds a device-information
  set of present nodes, reads six properties per node from the PnP manager
  (name, container, the battery level
  `{104EA319-6EE2-4701-BD47-8DDBF425BBE5} 2`, class of device, connected),
  and destroys the set. No `DeviceWatcher`, no notification, no
  `Windows.Devices.Bluetooth`, no device opened, nothing sent over the
  radio. How often is the provider's (174: every 10 s with the card shown,
  once a minute for the pill). A device Windows marks disconnected is left
  out (its level is the last one heard). A reading that wasn't asked for
  the listing leaves `bluetooth` unset, and the provider keeps the last.

**Deliberately not read, and what shows instead.**
- **The CPU's temperature has no proper source.** Windows has no
  documented, unprivileged API for its sensors: every tool that shows them
  loads a kernel driver or a vendor SDK, and WMI's thermal zone class needs
  admin. What is read is the ACPI thermal zone counter, which needs
  neither. On many laptops it is the CPU's zone; most desktops have none,
  and some boards report a constant made-up value. So the CPU tile shows
  that zone's temperature where there is one (an approximation), and the
  number of cores where there isn't.
- **Bluetooth LE devices' levels** show only where Windows itself put one
  on the node; the reader never asks a device (that is radio traffic).
- **Everything 174 took off the card**: per-core loads, the CPU's and
  GPU's names, GPU memory and engines, process lists, cached memory, the
  link's speed and name, other drives, disk activity, network totals
  "today" (exact only on a PC booted that day). Their sources are deleted,
  not switched off.

**Not taken.** WMI: slow, and a COM server. A WinRT `DeviceInformation`
query for Bluetooth: it goes through the service that 042's watcher did.
One PDH query for everything: a collect would read the GPU's hundreds of
instances for a pill that shows the CPU. Declaring the D3DKMT structures
ourselves so the temperature builds with any kit: a layout typed from
memory, passed to the kernel.

**Consequences.** Three system libraries (pdh, iphlpapi, setupapi), in
every Windows since 7. Three tokens (`statsFirstSpanMs`,
`statsStaleAfterMs`, `statsIdleCloseMs`): the provider reads them on the UI
thread and hands them over with each request (decision 144). To check on
Windows, besides 042's test: the log's `statistics: … counter opened (…)`
lines say what the PC has; the numbers against Task Manager (CPU, GPU and
its temperature, memory, the system drive, network); a PC with two GPUs; a
language other than English; a pill of one reading, and the thread's CPU
time with it; the connected property (`{83DA6326-…} 15`), which is not
documented: a paired but switched-off headset must not be listed.
