# 146: CI keeps compiles in a cache (sccache)

Status: Accepted (amends 036). Needs a few runs to show its speed.

**Context.** A release took about nine minutes, eight and a half of them in
the build: every run is a fresh Windows machine, so the whole app (heavy
C++/WinRT headers, optimised) is compiled from nothing each time. The user
asked for build caching in the workflows.

**Decision.** sccache (`mozilla-actions/sccache-action`), in both workflows,
with GitHub's Actions cache as its store (`SCCACHE_GHA_ENABLED`). It keeps
each compile's output by the source, its headers and the flags, so a file
that hasn't changed since a cached build is fetched, not compiled.
`build.ps1` uses it only when the workflow asks (`CUP_SCCACHE`) and sccache
is on PATH, as the C++ compiler launcher; local builds are as before. It
prints sccache's hits and misses after the build. Release builds already put
debug info in each .obj (`/Z7`, decision 013), which is what lets sccache
cache them; a shared .pdb (`/Zi`) wouldn't. The version is only in a
generated header (cup_version.h), so a new version recompiles the few files
that include it, not all of them.

GitHub's cache is per branch: main's runs (the Build workflow on every merge,
and releases) fill and read main's; a pull request reads main's and keeps its
own, which main never reads.

**Not taken.** Caching the build folder (actions/cache): a fresh checkout
gives every source file a new time, so Ninja would compile everything
anyway. Precompiled headers for C++/WinRT: worth trying if the cache isn't
enough, but they change every build, local ones too.
