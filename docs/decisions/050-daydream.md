# 050: The daydream: holding Home's shortcut

Status: Accepted

**Context.** A bit of play: holding Home's system-wide shortcut (decision
032) should make the island give up opening and float off, spinning and
wobbling like a drop of liquid, instead of doing nothing (the shortcut is
registered with `MOD_NOREPEAT`, so holding it fires once). Three things
make it less simple than it sounds. Windows sends `WM_HOTKEY` when the keys
go down and never when they come up. The island's pill, bubble and neck
are each driven by implicit and expression animations that don't tolerate
a second owner (decision 008). And a truly liquid outline (an animated
path) would mean replacing the rounded rectangle, its ears and its shadow
mask for the duration.

**Decision.**
- **The press still opens Home at once.** No delay is added to a normal
  press to tell it from a hold. The app reports `HomeShortcutHeld` after
  running the command; held for `daydreamHold`, core closes Expanded and
  sets `daydream` (`core/daydream.h`). That reads as "it tried to open,
  then gave up". `HomeShortcutReleased` ends it. Hiding ends it too.
- **The release is polled.** While the key is down, a one-shot timer
  (`shortcutPoll`) checks `GetAsyncKeyState` on the shortcut's key (not its
  modifiers). Only while held: an idle island still wakes once a minute.
- **In the air it takes no clicks** (clicks, Home, a lift, a bubble drag):
  the interaction window stays where the island rests (decision 001), not
  where it floats. A lift in progress lands when it starts.
- **One container moves.** The pill, the bubble and their neck live in one
  `ContainerVisual`; only it is animated (`platform/daydream`), through
  expressions over two properties, `Float` (0 at rest, 1 in the air) and
  `Phase` (radians). The turn, the drift, the jelly wobble and the
  droplet's buds are all whole multiples of `Phase`, so a loop of it wraps
  seamlessly. The pill's own morphs (an alert arriving) carry on inside it.
- **Liquid, faked:** the jelly is a non-uniform scale in the pill's own
  axes, and a droplet buds off the pill's edge and merges back through a
  second `LiquidBridge`. No animated paths.
- **It lands upright.** Let go, `Phase` runs forward to the next whole turn
  (never unwinding) while `Float` returns to 0. Composition doesn't report
  where `Phase` is (decision 001), so the app tracks it from the time each
  stage started, with a margin. A hold during the landing carries on from
  that turn. Each run's batch-completed handler checks it's still current.
- **Content fades out** while it floats: spinning text reads as a glitch.
  The flares fold in on take-off and come back as it touches down.
- **It plays even when Windows' "Show animations" is off.** It's only ever
  asked for, by holding the keys on purpose.

**Consequences.** Eight scenarios in `daydream.scenario` cover the rules;
the show itself needs a Windows run to judge and tune (`daydream*` tokens,
live). It relies on a keyframe animation without a 0 key starting from a
property's current *animated* value (the Forever loop's, mid-turn); if a
real build shows a jump there, the fix is to start the landing from the
tracked phase explicitly. `LiquidBridge` now takes two visuals instead of
two Blobs.
