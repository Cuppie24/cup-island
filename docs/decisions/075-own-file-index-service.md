# 075: Our own file index, in a small Windows service

Status: Proposed. Partly superseded by 076: search is installed with the app
and updates itself, with no "Turn on file search" and no separate update row.

**Context.** We want instant file search on the island, like Everything:
every file name on the PC, found while you type. Windows Search
(`SystemIndex` over OLE DB) is slow and only covers indexed folders.
Everything's IPC would make Cup Island depend on another app. The fast way
is the one Everything uses: read the NTFS file table with
`FSCTL_ENUM_USN_DATA` (every file's reference number, parent and name), then
keep the index current from the USN change journal (`FSCTL_READ_USN_JOURNAL`).
Both need a handle to the volume (`\\.\C:`), which only an administrator can
open. The island runs as the user, installed per user without a UAC prompt
(decision 011).

**Decision.**
- **Two processes.** `CupIslandIndex.exe` is a Windows service (LocalSystem)
  that reads volumes and answers queries. The island stays as it is, with the
  user's rights, and draws results and opens files itself (`ShellExecute`, as
  the user). The service never opens, starts or deletes a file.
  - Not the whole island elevated: an elevated topmost window loses drops
    from normal apps (UIPI), the HKCU Run entry can't start it elevated, and
    every UI bug would run as admin.
  - Not an elevated helper started by a scheduled task: the Service Control
    Manager already restarts crashed services, and a service starts before
    sign-in, so the index is warm.
- **Logic is pure, in `src/indexer/`** (`cup_indexer`, built and tested on
  Linux like `cup_providers`): USN record parsing from raw bytes, the index,
  applying journal changes, resume-or-rescan, queries and ranking, and the
  snapshot format. `src/service/` (Windows only) only translates: volume
  handles, the service entry point, the pipe.
- **The index** is direct-addressed by MFT record number (the low 48 bits of
  an NTFS file reference; the high 16 are the sequence number, kept to catch
  reused records). An entry is 16 bytes (parent, name offset, name length,
  sequence, flags) and names are UTF-8 in one buffer: **37 MB per million
  files** measured (1.2 M records, names ~18 bytes), once `shrink()` has
  given back what growing left; 67 MB before. That's in the service. TR §8's 60 MB is the island's and doesn't
  change. Paths are built by walking parents, only for results shown.
  ReFS (128-bit references, Dev Drive) needs a hashed mode: later.
- **Snapshot:** `volume-<serial>.bin` in the service's folder, SYSTEM's own
  LocalAppData (`C:\Windows\System32\config\systemprofile\AppData\Local\
  CupIsland\index-service`, with its log). Not ProgramData: any user may
  create folders there, so one could make ours first and own it. The file
  holds the journal id and the next USN.
  On start: load it and replay the journal from that USN; a different
  journal id, or a USN the journal no longer holds, means a full rescan.
  Written to a side file then renamed over the old one (as decision 028).
- **Queries, v1:** words (all must match), `"quoted phrases"`, `ext:`,
  `path:`, `file:`, `folder:`; case-insensitive for English and Russian
  (А–Я, not Ё): both fold within the same UTF-8 length, so matching stays
  byte by byte. Full Unicode folding (tables, NFC) was weighed and left out
  for speed and size. Ranked: whole
  name (or a file's name before its extension: "notepad" is notepad.exe's
  whole name), then name prefix, word start, anywhere; folders and shorter
  names (in letters, not UTF-8 bytes) first on a tie. Anything inside a
  noise folder (WinSxS, WindowsApps, node_modules, __pycache__, .git, .vs,
  $Recycle.Bin)
  comes after everything else, unless a `path:` filter names one; noise is
  only worked out for the hits near the top, since it means walking up
  folders. These three came from the first run on a real PC: notepad.exe
  ranked below "NotepadXamlUI", WinSxS's copies crowded the list, and
  Cyrillic names lost ties as if twice as long.
  A linear scan of a million names measured 30–45 ms; an
  n-gram index only if real drives say it's too slow. A snapshot of a
  million files is 31 MB and saves and loads in about a quarter second. No sizes or dates
  in v1: `ENUM_USN_DATA` has neither, they need raw `$MFT` parsing (later).
- **The service is a privilege boundary.**
  - The pipe accepts local interactive users only
    (`PIPE_REJECT_REMOTE_CLIENTS`, NETWORK denied).
  - **A user only sees names they could list themselves:** the service
    impersonates the client (`ImpersonateNamedPipeClient`) and checks list
    access on each result's folder, cached per folder. Everything shows all
    users' files; we don't.
  - Interactive users get `FILE_GENERIC_READ | FILE_WRITE_DATA` on the pipe,
    not `GENERIC_WRITE`: that includes `FILE_CREATE_PIPE_INSTANCE`, which
    would let any user serve the pipe beside us. The first instance is made
    with `FILE_FLAG_FIRST_PIPE_INSTANCE`, so a name taken before the service
    starts stops it rather than sharing. A client still checks the server is
    the service (`GetNamedPipeServerProcessId` against the SCM's process id),
    as `cupindex` does and the island must.
  - Read-only protocol (query, status) with caps on message size, query
    length and result count; the parsers are fuzzed on Linux. No cancel: a
    search takes tens of milliseconds, and a client ignores an answer whose
    id it no longer wants.
  - If the thread can't stop impersonating (`RevertToSelf` fails), the
    process ends at once rather than carry on with a client's rights.
  - Its own log; minidumps (decision 013) and SCM recovery actions come with
    the installer and hardening (milestones 4, 6).
- **Its sizes and timings** live in `src/service/service_limits.h`, not
  tokens.json: that file is the island's, user-writable and live-reloaded,
  and a SYSTEM process must not take settings from a file any user can edit.
- **Install and update.** The island's installer stays per user. The service
  has its own elevated installer, `CupIsland-Index-Setup-<v>.exe`
  (Inno Setup, `PrivilegesRequired=admin`), run from the island's "Turn on
  file search" (one UAC prompt) and removed from Settings or Apps & Features.
  Decision 036's silent updates can't cross UAC: a new service version is
  offered on its own row and is never mandatory (decision 041 doesn't apply).
  The protocol is versioned; the island works with the service's current and
  previous protocol.
- **Windows 10 1809 (decision 015):** the USN controls, services, pipes and
  impersonation are all XP/Vista-era. Nothing to look up at runtime.

**Milestones.**
1. This record, and a Windows spike timing `ENUM_USN_DATA` on a real drive.
2. `src/indexer/`, pure and tested: records, index, journal, snapshot, query.
3. The service: volumes, scan, journal, snapshots, the pipe protocol
   (JSON-RPC 2.0 like PS §8, length-prefixed), access checks, a debug CLI.
4. The index installer, the opt-in flow, both installers in releases.
5. The island: a live text field (an action per edit, not just Enter), the
   search panel, an "Open search" shortcut (decision 032), the setup card.
6. Hardening: drives coming and going, journal resets, sleep and resume.
7. Later: sizes and dates from `$MFT`, ReFS, removable and FAT drives.

**Planned (after milestone 5): recently opened first.** Wanted by the owner,
not built yet. Files you opened recently rank higher in search.
- **The signal:** files opened from the island's search (milestone 5),
  which the island knows exactly. Maybe later also Windows' Recent Items
  (`%APPDATA%\Microsoft\Windows\Recent`, a shortcut per file opened
  anywhere), if Explorer's record proves good enough.
- **Kept per user, in the island's store** (`Host::save`, decision 028):
  the last few hundred paths with when each was opened. Never in the
  service: what someone opens is theirs, and the SYSTEM process has no
  business keeping it.
- **Ranked on the island's side, no protocol change.** The island matches
  the query against its own short recent list and puts those hits first
  among results of the same match class (whole name, prefix, word start,
  anywhere), never above a better match: `report` typed exactly still
  beats a recent `q3-report.pdf`. More recent ranks higher; older than
  about a month counts for nothing. A recent file the service's answer cut
  off still shows, because the island's own list found it. Files gone from
  disk are dropped from the list.

**Trying it** (milestone 3, before the installer): build on Windows, then
in an administrator's terminal run `build\windows\CupIslandIndex.exe
--console` (Ctrl+C stops it, saving the index). In any terminal:
`build\windows\cupindex.exe status`, then `cupindex.exe query report
ext:pdf`. As a real service (admin; the exe must sit where users can't
write, such as `C:\Program Files\CupIsland\`):
`sc create CupIslandIndex binPath= "C:\Program Files\CupIsland\CupIslandIndex.exe" start= auto`,
then `sc start CupIslandIndex`; `sc stop` and `sc delete` undo it.
Checked so far: MSVC and MinGW builds; under Wine the pipe round trip
(status, query, errors), the volume retries and the log. Not yet: a real
NTFS drive's scan and journal, the access checks, running as a service.

**Consequences.** A second binary, a second installer and the app's first
UAC prompt, all opt-in. The island gains no rights. A packaged (MSIX) build
would need `desktop6:Service`, Windows 10 2004+ and an admin install: decide
when MSIX comes (TR §10).

**Open.** Default drives (all fixed NTFS, or a list in Settings); the
default search shortcut (Alt+Space is PowerToys Run's and the window menu's).
