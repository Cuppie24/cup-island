# 082: Recently opened files rank higher in search

Status: Accepted (builds decision 075's "Planned: recently opened first").

**Context.** Search ranked files by how well the name matched, then shorter
paths (decision 075). The file you open every day sat among a dozen of the
same name. Apps already ranked by past launches (decision 077,
LaunchHistory); files didn't.

**Decision.**
- **What's remembered:** files and folders opened from the island's search,
  in the island's store (`Host::save`, key `opened`), a LaunchHistory of
  the newest 300: how often, and when last. Never in the service: what
  someone opens is theirs, and the SYSTEM process has no business keeping
  it. Windows' Recent Items aren't read (maybe later).
- **Worked in on the island's side** (`recent_files`, pure), no protocol
  change. Each row's match class is the service's (`indexer::matchScore`:
  whole name, prefix, word start, anywhere). A recent row moves up past rows
  that match no better and were opened less recently; it never passes a
  better match (`report` typed exactly still beats a recent
  `q3-report.pdf`). Otherwise the service's order stands.
- **Weight:** LaunchHistory's, as for apps: the count, fading to nothing
  over a month.
- **Not in the answer:** a recent file the service's top rows left out is
  still shown, if the query is plain words that all match its name. With a
  filter (`ext:`, `path:`, a path word, a drive) only the service can
  judge, so only what it sent is re-ranked. These show without the service
  too (not set up, or starting).
- **Gone files:** before showing one the service didn't send, the app
  checks it's still there (`SearchHooks::exists`, `GetFileAttributes`);
  gone, it's dropped from the history. Paths come from the index, so local
  NTFS only: the check doesn't wait on a network.

**Consequences.** A row's place now depends on the user's history, so the
same query can order differently on two PCs. The store grows by at most
300 lines.
