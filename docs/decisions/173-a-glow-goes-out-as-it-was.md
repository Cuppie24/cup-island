# 173: A glow goes out as it was

Status: Accepted; built, not yet run on Windows (amends 090, 166; follows 171)

**Context.** When music stops, the cover's glow along the island's bottom
edge fades out with the other colours. Its start still felt sharp after
decision 171 gave recolours a gentle curve. The user saw why: the fade
didn't start from the glow as it was shown. Two things changed at once, the
moment the cover went:
- **Its shape.** An open card's glow has its own reach and curve (decision
  166), and `glowLook` falls back to the pill's when there's no card glow
  colour. The cover gone, the reach (`GlowHeight`, set at once, not faded)
  snapped from the card's to the pill's.
- **Its colour's goal.** No glow is `islandGlowColor` clear: clear *black*.
  Fading straight from an orange to clear black darkens the colour as it
  thins it, and a glow's light is colour times opacity: it lost most of
  its light in the first part of the fade.

**Decision.** A glow going out keeps what was shown, only clear
(`glowTowards`, core): the same colour with no opacity, the same reach, the
same curve. The Blob remembers the last glow that had any colour. A glow
coming in, or one cover's glow giving way to another's, is as before. Going
out, a card opening or closing no longer glides the reach either: there's
nothing to glide to.

**Consequences.** The fade-out is the shown glow thinning evenly, along
recolorEase (decision 171), and nothing else. The reach it kept is still
there, unseen, until the next glow sets its own. Tested in
`tests/tokens_tests.cpp`.
