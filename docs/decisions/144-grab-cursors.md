# 144: Grab cursors: an open hand, a closed one while carrying

Status: Accepted (amends 143). The look on Windows needs a run.

**Context.** Decision 143 showed Windows' move cursor (four arrows,
`IDC_SIZEALL`) over what drags a card. The user wanted a hand, as browsers
show over what can be grabbed. Windows has no such cursor: `IDC_HAND` is the
pointing finger links use.

**Decision.** The app brings Chromium's, as cursor resources in the exe
(`cup_island.rc`: 2 is `grab.cur`, an open hand; 3 is `grabbing.cur`, a
closed one): Chrome's and Edge's on Windows (`ui/resources/cursors/hand_grab.cur`
and `hand_grabbing.cur`), unchanged, 32 px, shaded white with a black edge.
Their BSD licence is in `THIRD-PARTY-NOTICES.txt`, which the installer puts
beside the exe. The open hand shows over a card's header and handle, the
closed one while a card is carried (`ProviderWindow::Grab`). If they can't be
loaded, the move arrows show.

**Not taken.** Drawing our own (a first try did, at 32, 48 and 64 px): the
user preferred ready-made ones, the hands people know from their browser.
Firefox's (MPL-2.0) are one-bit and blockier.
