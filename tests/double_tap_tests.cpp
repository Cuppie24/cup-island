// Double-tap shortcuts (decision 079): what counts as Ctrl pressed twice,
// and what doesn't, so ordinary Ctrl shortcuts never open search.

#include "core/double_tap.h"
#include "core/shortcuts.h"
#include "expect.h"

namespace cup::test {
namespace {

// Feeds "key down at t, up at t + held" for each tap; true if the last event fired.
bool taps(DoubleTap& d, std::initializer_list<std::pair<std::uint32_t, std::uint32_t>> pressHeld, int key = kCtrl)
{
    bool fired = false;
    for (auto [at, held] : pressHeld) {
        fired = d.feed(key, true, at) || fired;
        fired = d.feed(key, false, at + held) || fired;
    }
    return fired;
}

} // namespace

void runDoubleTapTests()
{
    {
        DoubleTap d(kCtrl);
        expect(taps(d, { { 0, 60 }, { 200, 60 } }), "Ctrl, Ctrl: fires");
    }
    {
        DoubleTap d(kCtrl);
        expect(!taps(d, { { 0, 60 }, { 60 + 351, 60 } }), "too slow between the taps: nothing");
        expect(taps(d, { { 1000, 60 }, { 1100, 60 } }), "and a quick pair after that works");
    }
    {
        DoubleTap d(kCtrl);
        expect(!taps(d, { { 0, 301 }, { 400, 60 } }), "a first press held long (Ctrl held for a shortcut): nothing");
    }
    {
        DoubleTap d(kCtrl);
        d.feed(kCtrl, true, 0);
        d.feed('C', true, 30);
        d.feed('C', false, 50);
        d.feed(kCtrl, false, 60);
        expect(!taps(d, { { 150, 60 } }), "Ctrl+C, then Ctrl: nothing");
        DoubleTap e(kCtrl);
        e.feed(kCtrl, true, 0);
        e.feed(kCtrl, false, 50);
        e.feed('V', true, 80);
        expect(!e.feed(kCtrl, true, 120) && !e.feed(kCtrl, false, 160), "a key between the taps: nothing");
        DoubleTap f(kCtrl);
        f.feed(kCtrl, true, 0);
        f.feed(kCtrl, false, 50);
        expect(!f.feed(kCtrl, true, 120), "the second press alone doesn't fire: its release does");
        f.feed('C', true, 150);
        expect(!f.feed(kCtrl, false, 200), "Ctrl, then Ctrl+C typed fast: nothing");
    }
    {
        DoubleTap d(kCtrl);
        d.feed(kCtrl, true, 0);
        d.feed(kCtrl, true, 30); // auto-repeat while held
        d.feed(kCtrl, true, 60);
        d.feed(kCtrl, false, 90);
        d.feed(kCtrl, true, 200);
        expect(d.feed(kCtrl, false, 250), "a held key's repeats aren't taps");
        DoubleTap g(kCtrl);
        expect(!taps(g, { { 0, 50 }, { 100, 301 } }), "a second press held long: nothing");
    }
    {
        DoubleTap d(kCtrl);
        expect(taps(d, { { 0, 50 }, { 100, 50 } }) && !taps(d, { { 200, 50 } }), "a third tap doesn't fire again");
        expect(taps(d, { { 300, 50 } }), "...it starts the next pair");
    }
    {
        DoubleTap d(kCtrl);
        expect(!taps(d, { { 0, 50 }, { 100, 50 } }, kShift), "another modifier's taps aren't ours");
        DoubleTap wrap(kCtrl);
        expect(taps(wrap, { { 0xFFFFFF00u, 50 }, { 0xFFFFFF00u + 200, 50 } }), "across the clock's wrap");
    }
    // The chord itself.
    Chord doubleCtrl{ .key = kCtrl, .twice = true };
    expect(chordText(doubleCtrl) == "Double Ctrl" && parseChord("Double Ctrl") == doubleCtrl &&
           parseChord(" double shift ") == Chord{ .key = kShift, .twice = true } && !parseChord("Double Q"),
           "Double Ctrl as text, and back");
    expect(chordOf(defaultBindings(), Command::OpenSearch) == doubleCtrl, "search's default is Double Ctrl");
    expect(loadBindings("home=Ctrl+Q\n") == defaultBindings(), "an old saved file gets search's default");
    expect(chordOf(loadBindings("search=Ctrl+Space\n"), Command::OpenSearch) == Chord{ .key = kSpace, .ctrl = true },
           "and it can be rebound to an ordinary chord");
    Bindings b = defaultBindings();
    expect(bindingProblem(b, Command::OpenSearch, doubleCtrl).empty(), "a double tap is a valid binding");
    expect(!bindingProblem(b, Command::OpenHome, doubleCtrl).empty(), "...but not two commands' at once");
    expect(!bindingProblem(b, Command::OpenSearch, Chord{ .key = 'Q', .twice = true }).empty(), "only modifiers are tapped twice");
}

} // namespace cup::test
