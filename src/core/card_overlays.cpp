#include "core/card_overlays.h"

#include "core/text_edit.h"

namespace cup {
namespace {

bool shown(const IslandState& s, const std::string& activityId) { return !s.homeOpen && s.expandedId == activityId; }

} // namespace

void focusInput(IslandState& s, const InputFocused& e, Ms now)
{
    if (!shown(s, e.activityId) || s.dialog) return;
    if (!s.draft || s.draft->actionId != e.actionId) {
        s.draft = Draft{ e.providerId, e.activityId, e.actionId, e.text, e.editing };
        s.draft->maxLength = e.maxLength;
        placeCaret(*s.draft, e.text.size(), false); // at the end, unless it was clicked somewhere
    }
    if (e.caret) placeCaret(*s.draft, *e.caret, false);
    s.lastPointerAt = now;
}

void submitText(IslandState& s, Ms now)
{
    if (!s.draft) return;
    if (s.draft->editing) s.draft.reset(); // an edit is done once sent
    else *s.draft = Draft{ s.draft->providerId, s.draft->activityId, s.draft->actionId, {}, false, 0, 0, s.draft->maxLength }; // a field keeps focus: add the next one
    s.lastPointerAt = now;
}

void showDialog(IslandState& s, const DialogShown& e, Ms now)
{
    if (!shown(s, e.dialog.activityId)) return; // a question needs its card in front of the user
    s.dialog = e.dialog;
    s.draft.reset(); // the dialog covers the field
    s.lastPointerAt = now;
}

void dropStaleOverlays(IslandState& s)
{
    if (s.draft && !shown(s, s.draft->activityId)) s.draft.reset();
    if (s.dialog && !shown(s, s.dialog->activityId)) s.dialog.reset();
    if (s.listDrag && (!shown(s, s.listDrag->activityId) || s.dialog)) s.listDrag.reset();
}

} // namespace cup
