#include "core/card_overlays.h"

#include "core/text_edit.h"

namespace cup {
namespace {

bool shown(const IslandState& s, const std::string& activityId) { return !s.homeOpen && s.expandedId == activityId; }

} // namespace

std::string keptKey(const std::string& activityId, const std::string& actionId) { return activityId + "/" + actionId; }

void dropDraft(IslandState& s)
{
    if (!s.draft) return;
    if (!s.draft->editing) {
        std::string key = keptKey(s.draft->activityId, s.draft->actionId);
        if (s.draft->text.empty()) s.keptText.erase(key);
        else s.keptText[key] = s.draft->text;
    }
    s.draft.reset();
}

void focusInput(IslandState& s, const InputFocused& e, Ms now)
{
    if (!shown(s, e.activityId) || s.dialog) return;
    if (!s.draft || s.draft->actionId != e.actionId) {
        dropDraft(s); // the field it leaves keeps its text
        std::string text = e.text;
        if (!e.editing) { // a field: back to what it had (decision 050)
            auto kept = s.keptText.find(keptKey(e.activityId, e.actionId));
            if (kept != s.keptText.end()) text = std::move(kept->second), s.keptText.erase(kept);
        }
        s.draft = Draft{ e.providerId, e.activityId, e.actionId, text, e.editing };
        s.draft->maxLength = e.maxLength;
        placeCaret(*s.draft, text.size(), false); // at the end, unless it was clicked somewhere
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
    dropDraft(s); // the dialog covers the field; its text waits
    s.lastPointerAt = now;
}

void openMenu(IslandState& s, const MenuOpened& e, Ms now)
{
    if (!shown(s, e.menu.activityId) || s.dialog || e.menu.items.empty()) return;
    s.menu = e.menu;
    s.lastPointerAt = now;
}

void dropStaleOverlays(IslandState& s)
{
    if (s.draft && !shown(s, s.draft->activityId)) dropDraft(s);
    if (s.dialog && !shown(s, s.dialog->activityId)) s.dialog.reset();
    if (s.listDrag && (!shown(s, s.listDrag->activityId) || s.dialog)) s.listDrag.reset();
    if (s.focusRequest && !shown(s, s.focusRequest->activityId)) s.focusRequest.reset();
    if (s.menu && (!shown(s, s.menu->activityId) || s.dialog)) s.menu.reset(); // a menu goes with its card, and under a question
}

} // namespace cup
