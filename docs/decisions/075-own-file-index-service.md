# 075: Our own file index, in a small Windows service

Status: Proposed (changes decision 011's "no admin prompt" for one opt-in
part, and TR §10's packaging note; update both if accepted)

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
- **Snapshot:** `%ProgramData%\CupIsland\index\<volume serial>.bin`, readable
  by SYSTEM and Administrators only, with the journal id and the next USN.
  On start: load it and replay the journal from that USN; a different
  journal id, or a USN the journal no longer holds, means a full rescan.
  Written to a side file then renamed over the old one (as decision 028).
- **Queries, v1:** words (all must match), `"quoted phrases"`, `ext:`,
  `path:`, `file:`, `folder:`; case-insensitive for ASCII. Ranked: whole
  name, then name prefix, word start, anywhere; folders and shorter names
  first on a tie. A linear scan of a million names measured 30–45 ms; an
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
  - Read-only protocol (query, status, cancel) with caps on message size,
    query length and result count; the parsers are fuzzed on Linux.
  - Its own log and minidumps (decision 013), SCM recovery actions to restart.
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

**Consequences.** A second binary, a second installer and the app's first
UAC prompt, all opt-in. The island gains no rights. A packaged (MSIX) build
would need `desktop6:Service`, Windows 10 2004+ and an admin install: decide
when MSIX comes (TR §10).

**Open.** Default drives (all fixed NTFS, or a list in Settings); the
default search shortcut (Alt+Space is PowerToys Run's and the window menu's).
