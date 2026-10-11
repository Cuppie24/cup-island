# 011: Ship a plain installer first, MSIX later

Status: Proposed (TR §10 still says MSIX; update TR if accepted)

**Context.** TR §10 asks for a signed MSIX. That needs a manifest, package
identity and a code-signing certificate before anyone can install it
(an unsigned MSIX won't install at all). We want to hand out builds now.

**Decision.** Release builds are a GUI-subsystem exe (no console; quit from
the tray icon) with the C++ runtime linked statically, so the exe has no
dependencies besides Windows. `installer/cup-island.iss` (Inno Setup) wraps
it with `tokens.json` into a per-user setup exe: no admin prompt, Start menu
shortcut, uninstaller. `.\build.ps1 -Installer` builds it into `dist\`.

**Consequences.** No VC++ Redistributable to bundle or detect. Unsigned, so
SmartScreen warns until we sign the setup exe and `CupIsland.exe`. Launch at
sign-in is a "Start with Windows" checkbox in the tray menu, written as an
HKCU Run value. TR §9 asks for the MSIX `StartupTask`, but its real rule is
"user opt-in, no silent autorun", and an unticked-by-default checkbox meets
it. The uninstaller removes the value; MSIX will swap in `StartupTask`. Static linking adds a few hundred KB to the exe.
