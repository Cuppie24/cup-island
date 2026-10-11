# 008: Handing a visual between animations in Composition

Status: Accepted

**Context.** Each blob's Size/Offset morph through implicit animations
(`this.FinalValue`), but a dragged bubble's Offset is driven by an
expression that keeps it centred on the cursor. Handing Offset back and
forth between the two broke on a real build: after `StopAnimation(L"Offset")`
and setting `Offset` to its home, the bubble stayed at the drop point. You
can't tell whether that happens by reading the code, and Linux sessions
can't run the app to check.

**Decision.**
- **Leaving a cursor/expression animation:** start an explicit keyframe
  animation with *both* ends (from the last known position, which the app
  tracks itself, to the target). Set the resting value first with
  `ImplicitAnimations(nullptr)` so it doesn't start a morph of its own, then
  re-attach the collection (`Blob::follow`).
- **Never re-set an unchanged Size/Offset:** it restarts the implicit
  animation and stalls a morph in flight (`Blob::place` compares first).
- **While an expression owns a property, don't set that property:** keep
  the target on the side (`offset_`) and apply it on hand-back.
- **Hidden shapes wait on the top edge** (`Scene::bubbleTuck`): a shape that
  grows from a point mid-pill rises into place and shows a gap above it.
  Anything that appears or disappears should do so from the top edge.
- Composition never reports animated values back (decision 001), so any
  "where is it now" the app needs, it must track itself.

**Consequences.** `Blob` holds its implicit collection (`implicit_`) and the
last cursor centre. Platform code is only verified by running it on
Windows. In Linux sessions, a syntax check against a stub `platform/win.h`
(every WinRT method taking anything) catches C++ mistakes like missing
includes, but says nothing about how Composition behaves.
