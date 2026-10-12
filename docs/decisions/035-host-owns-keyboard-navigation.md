# 035: The host owns keyboard navigation; providers give hints, never keys

Status: Accepted

**Context.** With the keyboard able to reach every card, dialog and list
(031, 033, 034), the question came up whether a provider should handle
its own buttons' tab order and keys. Toolkits that let each part of the
UI do this (key handlers, tab indexes, moving focus in code) end up with
cards that each behave a little differently, some not at all.

**Decision.**
- **Providers never see keys or manage focus.** There is no `onKey`, no
  tab index, and no way to move or take focus. The host decides what is
  a stop, the order, which keys do what, and how focus looks.
- **The order is the layout's.** Focus follows reading order on screen
  (`core/keyboard`), so where a provider puts its buttons is its tab
  order, and it can't drift from what's drawn. A provider that wants a
  different order lays its content out differently.
- **When the host can't work something out from the layout, the provider
  gives a hint**: a declarative fact about its content, with the keys
  left to the host. So far:
  - `Content::backAction`: the card has a step back. Esc sends it (033).
  - `Provider::onCloseRequested`: the user wants this activity closed. It's
    a request with a meaning, not a key: the provider never learns it was
    Ctrl+W (034).
- **New hints are added when a real card needs one**, not ahead of it.
  Likely candidates: which item gets focus first, if a card needs
  something other than its first; leaving a clickable item out of the
  stops (decorative art). Each is a field on the content, read by
  `core/keyboard`.

**Why.**
- It's the split everything else follows: providers describe, the host
  draws (PS §1 rule 1); providers decide what goes where, the host how
  it works and looks (decision 014).
- Every card answers to Tab, the arrows, Esc and Enter the same way,
  whoever wrote its provider, and keeps doing so as the keys change.
- A provider can't break Esc, trap focus or swallow the shortcuts.
  Built-ins use the public API (PS §1 rule 3), so a key hook for one would
  be a hook for every third party. Out of process, sending keystrokes to
  plugins would cost latency and be a security risk.
- Screen readers (UI Automation) need the host to know what's on a card
  anyway; focus works from the same knowledge.

**Consequences.** A request for a provider to "handle a key" is turned
into a hint: what does the key *mean* for this card? The host maps keys
to that meaning. Dialogs (020) already work this way: the host owns their
buttons and decides that focus starts on Cancel.
