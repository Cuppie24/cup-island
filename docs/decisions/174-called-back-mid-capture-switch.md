# 174: The island is called back while the capture switches (a crash under Snipping Tool)

Status: Accepted; built, not yet run on Windows (amends 165)

**Symptom.** 0.13.1, liquid glass on. Snipping Tool's overlay came and went
twice within a second; the second time the log read

    fullscreen app detected: hiding (SnippingTool.exe ...)
    Compact -> Hidden (fullscreen)
    fullscreen app gone: showing          <- 2 ms later, no "Hidden -> Compact"
    CRASH: exception 0xe06d7363 at ...    <- an uncaught C++ exception

**Cause** (read from the log and the code; the dump would confirm it).
Hiding places the glass's shapes, and `LiquidGlass::render()` then switches
the capture with `lock_` held. Here the island had been shown 4 ms before,
its capture not yet started, so `syncCapture()` started one for a shape
already on its way out. Starting (and closing) a capture calls into another
process, and while the UI thread waits Windows runs its messages. The
fullscreen watcher's hook answered one: the overlay had gone. So, from
inside `capture_.start()`:

    dispatch(HiddenChanged) -> PillRenderer::apply -> LiquidGlass::place
        -> lock_guard(lock_)   // this thread already holds it

A `std::mutex` locked again by its owner throws `std::system_error`, which
`render()` (catching `winrt::hresult_error`) let through. The missing
"Hidden -> Compact" line is the same thing seen from `dispatch()`: the
inner one compared against a scene the outer one hadn't stored yet.

**Decision.**
- **`LiquidCapture::switching()`** is true inside its `start()` and
  `stop()`. While it is, on the UI thread, `LiquidGlass::place()`, `setActive()`,
  `refresh()` and `render()` don't lock (their caller up the stack holds
  the lock): they record what's wanted, set `again_`, and leave.
  `syncCapture()` loops while `again_`, so a capture started for a shape
  that has since gone is paused before it returns, and the frame after it
  draws the shapes as last placed.
- **A frame arriving meanwhile** isn't taken (`take()` locks too): it stays
  in the pool and `catchUp()` takes it once `render()` has let go.
- **`render()` catches `std::exception` as well**: called by Windows, it
  throws nothing back (decision 158), whatever the kind.
- **`dispatch()` isn't entered while a scene is being applied.** Between
  `reduce` and `g_island.scene = next` (`g_applying`), a dispatch is queued
  to run after, as one from another thread is (decision 060). Carried on
  inline, the outer dispatch finished by storing its older scene over the
  newer one: here, an island left hidden with no fullscreen app. The
  dispatches made on purpose from inside one (a recolour's
  `tokensChanged`, Settings republishing) come after that point and still
  run at once.

**Not covered.** `LiquidGlass::stop()` or `start()` called from inside a
switch (a glass setting or the display changing in that same moment) would
still lock twice. Nothing seen does that; it needs the switch itself moved
out from under the lock.

**Rule.** Holding a lock on the UI thread across a call into another
process (capture, COM out of process, `SetWindowDisplayAffinity`) means
the app's own handlers can run with it held. Either let go first, or make
every UI-thread way in check for it.
