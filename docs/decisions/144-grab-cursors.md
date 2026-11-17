# 144: Grab cursors: an open hand, a closed one while carrying

Status: Accepted (amends 143). The look on Windows needs a run.

**Context.** Decision 143 showed Windows' move cursor (four arrows,
`IDC_SIZEALL`) over what drags a card. The user wanted a hand, as browsers
show over what can be grabbed. Windows has no such cursor: `IDC_HAND` is the
pointing finger links use.

**Decision.** The app brings its own, as cursor resources in the exe
(`cup_island.rc`: 2 is `grab.cur`, an open hand; 3 is `grabbing.cur`, a
closed one). `tools/make_cursors.py` draws them: white with a black outline
and lines between the fingers, like Windows' own, at 32, 48 and 64 px, so
Windows picks one for the cursor size and display scale instead of
stretching one; the hotspot is the palm's middle. The open hand shows over a
card's header and handle, the closed one while a card is carried
(`ProviderWindow::Grab`). If they can't be loaded, the move arrows show.

**Not taken.** Drawing them at run time (a bitmap made into a cursor): more
code than two small files, for the same pixels.
