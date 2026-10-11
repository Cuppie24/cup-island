# 003: Win32 + Composition for the island, not WinUI 3

Status: Proposed (TR §3–4 still say WinUI 3; update TR if accepted)

**Context.** TR picks WinUI 3. But the island draws everything itself from
declarative content (PS §5), so it needs no XAML controls. WinUI 3 with C++
brings XAML, IDL/MIDL code generation and heavy MSBuild projects. That's the
most verbose C++ on Windows, and it doesn't fit CMake. The prototype already
proves raw Win32 + `Windows.UI.Composition` delivers the morphs, the
two-layer windows and zero idle cost.

**Decision.** Keep the island on Win32 + Composition, built with CMake. If a
settings window ever needs real controls, build it separately (possibly C#)
and have it talk to the host over the PS §8 pipe.

**Consequences.** Smaller, readable codebase and a portable `core/`. We do
our own text rendering (DirectWrite into a Composition surface) when
content slots land. MSIX packaging (TR §10) still works for a plain Win32 exe.
