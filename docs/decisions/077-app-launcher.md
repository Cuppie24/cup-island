# 077: An app launcher in the island's search

Status: Proposed

**Context.** The owner wants to start apps from the island, the way Listary,
PowerToys Run and the Start menu's search do: type a few letters, press
Enter. File search (decisions 075, 076) finds files; apps, Settings pages
and Control Panel items are something else. They aren't files to open by
path (a Store app has no exe to run), and they need their names and icons
as Windows shows them, in the system language. Windows has no single "all
launchable things" API: apps and the Control Panel have shell folders that
list them, the Settings app has none.

**Decision.**
- **It runs in the island, as the user.** Apps, Settings and the Control
  Panel are per user and read through the user's shell. No service, no admin
  prompt, nothing to install: the launcher works with or without file search.
- **Three sources:**
  - **Apps: the Applications folder** (`shell:AppsFolder`,
    `FOLDERID_AppsFolder`), the Start menu's "All apps": desktop programs and
    Store apps alike, with display name, AppUserModelID and icon. Read with
    `SHGetKnownFolderItem` and `BHID_EnumItems`; names from `GetDisplayName`,
    the ID from `System.AppUserModel.ID`, icons from `IShellItemImageFactory`.
    Not the Start menu's `.lnk` folders (no Store apps) or `PackageManager`
    (only Store apps): this folder already merges both.
  - **Control Panel items: the "All Control Panel Items" shell folder**
    (`shell:::{26EE0668-A00A-44D7-9371-BEB064C98683}`), read the same way:
    Power Options, Device Manager, Credential Manager...
  - **Settings pages: our own list**, since Windows publishes none (its own
    search uses an undocumented index that changes between versions;
    PowerToys Run ships a hand-made list too). Each entry: its
    `ms-settings:` link, English and Russian names, search words ("wifi",
    "вайфай", "интернет" for `ms-settings:network-wifi`), and the Windows
    builds that have the page, since an unknown link just opens Settings'
    home (Windows 10 and 11 differ; decision 015). It starts with the pages
    people actually look for, about 60, and grows. It's a C++ table in the
    pure code, tested: no file to parse or keep beside the exe.
  - Not available from any public API, so not offered: single switches
    ("Turn on dark mode"); the most we can do is open the page they're on.
- **Clean-up, pure and tested** (`providers/launcher/`): entries nobody
  launches are dropped, by rules that go into tests: names starting with
  "Uninstall" / "Удалить", help files and web links (`.chm`, `.url`, `.html`,
  `.pdf`, `.txt` targets), and duplicates of one AppUserModelID.
- **Kept in memory, refreshed in the background.** Reading a few hundred
  apps with their icons takes ~0.1-0.5 s, so it's done once, off the UI
  thread (its own COM apartment), and searched from memory after that. It's
  read again when the search opens if the last read is older than
  `launcherRefreshMs` (tokens.json); what's shown is the old list until the
  new one is in. Icons are decoded once, at the size the island draws them,
  into the content model's `Image` (BGRA): the host draws them like media art.
- **Matching and ranking reuse file search's** (`indexer::matchScore`:
  whole name, name start, word start, anywhere; case-insensitive for English
  and Russian, decision 075), plus two things only names of apps need:
  - **initials**: `vsc` finds Visual Studio Code, `вк` ВКонтакте, scored
    like a word start;
  - **a Settings page's search words** count like its name.
  Apps launched more often and more recently rank higher, kept per user in
  the island's store: the same history as "recently opened first"
  (decision 075's plan), shared by apps and files.
- **Launching:** Enter starts the top result; arrows pick another
  (decisions 031, 035). Apps and Control Panel items start through the shell
  (`ShellExecuteEx` on the item, `shell:AppsFolder\<ID>`), which handles
  desktop and Store apps alike and keeps a shortcut's arguments and working
  folder. Settings pages open their `ms-settings:` link. Ctrl+Shift+Enter runs
  a desktop app as administrator (the `runas` verb, as the Start menu does);
  Store apps can't be. The island comes to the front with its shortcut
  (decision 032), so the app it starts gets the focus.
- **One search box for everything** (milestone 5): results in groups, apps
  first, then Settings and Control Panel, then files; each group shows its
  best few, so a hundred file matches never push the app you meant off the
  list. Without file search installed, the box still finds apps and settings.

**Order.** The launcher needs neither the service nor an installer, so it
comes first: milestone 5 becomes the search panel with apps, Settings and
Control Panel items; files join it in the next step through the service.

**Consequences.** A list of Settings pages to keep up as Windows changes
them: a new or renamed page is a line in a table and a test. COM and the
shell in the island's process, on a background thread: a misbehaving shell
extension could slow the read down, never the island's UI. Nothing leaves
the PC (TR §10).

**Open.** The search's own shortcut (Alt+Space is PowerToys Run's and the
window menu's). Whether to add later: switching to open windows, a
calculator, web search (a network call TR §10 doesn't cover).
