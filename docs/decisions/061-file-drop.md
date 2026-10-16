# 061: File drop: any provider can take a drop; the shelf links or copies

Status: Accepted, partly built (amends PS §3.1, §3.4, §7.2). Built: the
zones, the Shelf, Explorer's files, links and text. Not yet: virtual files,
drag out of the shelf, the wider hot zone, the elevation alert (only logged).
See "As built" at the end.

**Context.** Dragging files onto the island should open the file drop
(the Shelf, PS §10) whatever the island is showing. Four questions came
with it: is accepting drops a shelf-only feature or open to every
provider; does the shelf keep the user's files or copies of them; where
does the island catch a drag when the resting pill is tiny; and what the
island does after a drop.

**Decision.**

*Drops are a capability, not a shelf feature.*
- Any provider can accept drops (PS §1 rule 3: built-ins use the public
  API). The Shelf is the first to do so. Third-party providers that accept
  drops are off by default (PS §9), and a drop goes only to the zone it
  landed on.
- A provider brings one or more **zones**, not one zone. At `DragEnter`
  (once, never on every `DragOver`) the host builds a summary of the drag:
  its kinds, item count, sizes, folders and free disk space. It hands the
  summary to each accepting provider, which returns its zones:
  `{id, label, reason, glyph, enabled}`. For built-ins this is an
  in-process call to a pure function. Tier 2 plugins declare fixed zones in
  their manifest and are never asked during a drag: a pipe round-trip inside
  the source app's drag loop would stall it.
- Core only sees the kinds of data being dragged, never paths. It decides
  Receptive and lays out the zones in the `Scene`, so `hitTest` picks the
  zone. Paths go from the platform straight to the registry and the chosen
  provider, as plain structs.
- With one accepting provider (today, only the Shelf), Receptive *is* that
  provider's card with its zones over it. With several, each brings its zones.

*The Shelf: two big zones, Link and Copy.*
- **Link** keeps a reference to the original: its path plus the volume
  serial and file ID, so a moved file can often be found again. **Copy**
  makes a copy in the host's file store (below).
- Both zones fill the card, as big as Expanded allows, so they're easy to
  land on. They sit one above the other when the card is short. Their sizes
  and the gap come from tokens.
- A zone is **greyed** only when it would do nothing at all, and it always
  shows why on a second, smaller line:

  | Dropped | Link | Copy |
  | --- | --- | --- |
  | Local, USB or network file under the copy limit | ✓ | ✓ |
  | File over the copy limit | ✓ | greyed: "Too large to copy (4.2 GB, limit 1 GB)" |
  | Not enough free disk space, or the shelf's store full | ✓ | greyed, with that reason |
  | Folder | ✓ | greyed: "Folders can only be linked" (no tree walk during a drag) |
  | Outlook attachment, file inside a zip, phone (MTP) | greyed: "No file on disk to link" | ✓ |
  | Text, URL | greyed: "Nothing to link" | ✓ saved as `.txt` / `.url` |
  | Control Panel item, "This PC", printers | not offered: the island doesn't open | |

- **Mixed drops: each zone does all it can and says how.** A drop of two
  small files and a 4 GB video shows Copy as "Copy 2 · 1 too large, will be
  linked". The other way round: "Link 2 · 1 has no file, will be copied".
  An item neither zone can take (a 3 GB file inside a zip, text over the
  limit) is skipped, and the label says so: "Copy 2 · 1 skipped (too large)".
  The shelf's total storage fills in drop order: once it's full, the rest are
  linked. The label uses the same calculation, so it says what will happen.
  All of this is one pure function in `src/providers/shelf/`:
  `dropZones(summary, limits) → {link, copy}`, each zone with its label,
  reason and a plan for every item. It is tested on Linux.
- **Sizes are read off the UI thread.** `GetFileAttributesEx` can freeze on
  a network share or a sleeping USB drive, so a worker thread started at
  `DragEnter` reads the sizes. Until it answers, Copy reads "Checking…", and
  a drop on it waits for the answer. Only the first 200 items are checked;
  past that the label reads "Large drop: link only". Virtual files take their
  size from their descriptor (`FD_FILESIZE`). When it's missing, the limit
  applies during the copy instead.
- Limits (`shelfCopyMaxBytes` per file, `shelfStoreMaxBytes` for the store)
  are tokens.

*What a drag can carry, and the order it's read in.*
`CF_HDROP` (file paths) → `CFSTR_SHELLIDLIST` (shell items, some with no
path: phones, libraries) → `FileGroupDescriptorW` + `FileContents` (virtual
files the source makes on request: Outlook, zips, Remote Desktop) →
`UniformResourceLocatorW` (a link, saved as an `[InternetShortcut]` `.url`,
named after the page's title from the HTML format, else its domain) →
`CF_UNICODETEXT` (saved as UTF-8 `.txt` with a BOM, which Notepad on 1809
needs, named after its first words). Generated names drop
`\ / : * ? " < > |`, Windows' reserved names (`CON`, `NUL`, `COM1`…) and
trailing dots and spaces, and are cut to a sane length. Shell items that
aren't files are refused.

*Copies live in the host's store, not the provider's files.*
- Decision 028 says a provider keeps only text, through `Host::load/save`.
  Copies need bytes, so the host gets a file store:
  `Host::storeFile(stream) → handle`, `removeFile(handle)`, with a size cap
  per provider. It lives in `%LOCALAPPDATA%\CupIsland\shelf\`, not
  `%TEMP%`, which Storage Sense empties. Each drop's items go in their own
  folder there, so two `report.pdf` from different places keep their names.
- Copying runs in the background with progress and cancel. The drop itself
  is accepted at once, so Explorer finishes its drag. `CopyFile2` is used
  for paths: it copies alternate data streams, so the Mark of the Web
  (`Zone.Identifier`) comes along. Streams (virtual files, MTP) get one
  written by us when they came from the internet, so opening them from the
  shelf still goes through SmartScreen.
- Half-finished copies are deleted at the next start. Clear and Remove
  delete copies. Removing a *link* never touches the original: the
  wording is "Remove from shelf", and Clear's confirmation says "originals
  are kept".

*Shelf items show what they are.*
New glyphs `Link` and a missing/offline state (`Warning`, or a new
`Missing`). A copy shows the existing `Copy` glyph, with a progress bar
while it's being copied. A link shows `Link`; it's greyed with a warning
when the original is gone or its drive is offline. Links are checked lazily
when the shelf is shown, never at startup, so a dead network drive can't
slow the start. A copy that fails after the drop (the network dropped,
OneDrive offline) becomes an error row with **Keep as link** when the source
has a path, else Remove.

*Windows' side of the drag.*
- **Never `DROPEFFECT_MOVE`.** Explorer suggests a move on the same
  volume, and on a move the *source deletes the file*. Over an enabled zone
  the effect is `COPY` (Copy) or `LINK` (Link), whatever keys are held.
  Over a greyed zone, the gap between zones or outside them it's `NONE`, so
  Windows shows its "no" cursor. Dragging out of the shelf offers only
  `COPY`, so nothing moves the user's original or empties our store.
- A drag the island started itself (dragging out of the shelf and back)
  doesn't open Receptive.
- The UI thread calls `OleInitialize` (an STA alone isn't enough for
  `RegisterDragDrop`). `DragOver` gives physical screen pixels; they're
  converted like all other input before `hitTest`.
- **The hot zone without blocking clicks.** OLE finds the target with
  `WindowFromPoint`, which skips the `WS_EX_TRANSPARENT` render layer, so
  only the interaction layer can take a drop, and at rest that's just the
  pill. A permanent invisible strip around it would steal clicks from what's
  below (browser tabs at the top of a maximised window). Instead a
  low-level mouse hook (`WH_MOUSE_LL`) notes "left button held and moving
  near the island" and the interaction layer grows over the hot zone until
  the button is released. While a button is held, the window under the
  press keeps the mouse capture, so the grown window never takes a click,
  only a real drag's `DragEnter`. The hook only posts a message and returns,
  because Windows drops a hook that is slow (`LowLevelHooksTimeout`), and
  it is off while the island is Hidden. The grow/shrink rule and a short
  grace after `DragLeave` (the window resizes under the cursor as Receptive
  opens, which can fire `DragLeave`/`DragEnter` in a row) are core rules
  with scenarios.
- **Elevation.** OLE drag and drop doesn't cross integrity levels. An
  elevated island gets no drops from a normal Explorer. Decision 062 keeps
  the installer from starting it elevated. As a safety net, at start the
  island checks its own token (`GetTokenInformation(TokenElevation)`). When
  elevated, it logs it and says once that drag and drop is off until
  Cup Island is started normally.

*States.*
- Receptive wins over Expanded, Alert and Resting (PS §3.3), and loses to
  Hidden. On `DragLeave` or Esc the island goes back to exactly where it
  was: the same card, pin (decision 046) and field draft (decision 050).
- **After a drop the island doesn't collapse.** It stays Expanded on the
  Shelf's card, showing the new items. The usual idle timeout applies from
  then on. This replaces PS §3.4's "then island collapses".

**Consequences.**
- PS §3.1, §3.4 and §7.2 are updated to match. Decision 028 keeps its
  rules: the copy store is the Shelf's (As built). `IslandState::dragging` and the `DragEntered`/`DragLeft`/`Dropped`
  events already exist; `DragEntered` carries the zones offered instead of
  `accepted`.
- New scenario lines: a drag opens Receptive from every resting and overlay
  state except Hidden; a drag leaving goes back to the previous card and
  draft; a drop stays Expanded on the Shelf; the grace after `DragLeave`;
  the hot zone growing only while a button is held.
- New unit tests for `dropZones`: every row of the table above, the mixed
  labels, the store filling in drop order, the 200-item cap, and generated
  names.
- The Windows side (OLE target, hook, worker, copies) can only be checked
  on Windows: drags from Explorer, a USB drive, a network share, OneDrive
  online and offline, Outlook, a zip, a browser's link and text, and an
  elevated island.

**As built (first version).**
- **The copy store is the Shelf's, not the host's.** `Host::storeFile` waits
  until a second provider needs a store: the Shelf's hooks
  (`providers/shelf/shelf_files.cpp`) copy into
  `%LOCALAPPDATA%\CupIsland\shelf\<item id>\`, so decision 028 is unchanged.
- **Zones sit side by side**, the drag's whole card split between them
  (`receptiveSize`, `dropZoneGap`), drawn like Home's widgets.
- **Limits are in megabytes** (`shelfCopyMaxMB`, `shelfStoreMaxMB`): the
  tokens reader parses numbers as floats, exact only to 2^24.
- **A drop before the sizes are read** reads them then, once. A file whose
  size is still unknown is planned as fitting; the copy stops if it turns
  out larger than the limit (`CopyJob::maxBytes`) and the item offers
  "Keep as link".
- **A critical alert during a drag** waits for the drag to end, then closes
  Expanded as it always would (`IslandState::criticalHeld`).
- Copies aren't cancelled when their item is removed mid-copy: the copy
  finishes and is deleted.
- Tests: `tests/scenarios/drop.scenario`, `tests/drop_tests.cpp`,
  `tests/shelf_tests.cpp`.
- The Shelf's card was redone in decision 063: a Large card, cards or a
  list, thumbnails, rename, Show in folder, double-click to open.
