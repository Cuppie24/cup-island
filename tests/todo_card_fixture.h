#pragma once
// A to-do list on a card, as Pomodoro's card had before its window
// (decision 106): the field, the list, the Done row, the clear buttons; a
// clear asks with a dialog over the card, a delete leaves a toast with Undo.
// The island's own card machinery (host lists, dragging, editing in place,
// toasts, dialogs, kept text) is tested on it.

#include "core/provider.h"
#include "providers/pomodoro/todo_list.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>

namespace cup::test::fixture {

inline constexpr const char* kTodoProvider = "todos";
inline constexpr const char* kTodoCard = "todos.card";
inline constexpr const char* kGoLive = "live"; // the card becomes an activity's (tabs, the tallest card)

// The card: wide, kept open, its rows the list's (appendTodoRows).
inline Content todoCard(const TodoList& list)
{
    Content c{ .label = "To-dos", .card = CardLayout::Centred, .keepOpen = true, .noIdleTimeout = true, .wideCard = true,
               .tile = { row({ Text{ "To-dos" } }) } };
    appendTodoRows(c.expandedCenter, list);
    return c;
}

inline Panel todoPanel(const TodoList& list = {}) { return { .id = kTodoCard, .content = todoCard(list), .wide = true }; }

class TodoCardProvider : public Provider {
public:
    const char* id() const override { return kTodoProvider; }
    void start(Host& host) override
    {
        host_ = &host;
        todos_ = TodoList::load(host_->load("todos"));
        show();
    }
    void stop() override
    {
        if (host_) host_->removePanel(kTodoCard);
        host_ = nullptr;
    }
    void onAction(const Action& action) override
    {
        namespace todo = todo_action;
        const std::string& id = action.actionId;
        if (!host_ || action.activityId != kTodoCard) return;
        if (id == kGoLive) {
            live_ = true;
            return show();
        }
        if (id == todo::kClearDone || id == todo::kClearAll) return host_->ask(clearQuestion(id, todos_, kTodoCard));
        if (id == todo::kUndoDelete) {
            if (deleted_) todos_.restore(*deleted_);
            deleted_.reset();
            host_->save("todos", todos_.save());
            return show();
        }
        if (id.rfind(todo::kDelete, 0) == 0) {
            std::uint64_t todoId = std::strtoull(id.c_str() + std::char_traits<char>::length(todo::kDelete), nullptr, 10);
            auto it = std::find_if(todos_.items().begin(), todos_.items().end(), [&](const Todo& t) { return t.id == todoId; });
            if (it != todos_.items().end()) {
                deleted_ = *it;
                host_->toast({ .activityId = kTodoCard, .text = "To-do deleted", .actionLabel = "Undo", .actionId = todo::kUndoDelete });
            }
        }
        if (!todos_.apply(id, action.text, action.value)) return;
        host_->save("todos", todos_.save());
        show();
        if (id == todo::kAdd) { // the newest open one: keep it in view (decision 029)
            auto open = std::count_if(todos_.items().begin(), todos_.items().end(), [](const Todo& t) { return !t.done; });
            if (open > 0) host_->reveal(kTodoCard, todo::kList, static_cast<std::size_t>(open - 1));
        }
    }

private:
    void show()
    {
        host_->publishPanel(todoPanel(todos_));
        if (live_) host_->startActivity({ .id = kTodoCard, .relevance = 0.4f, .content = todoCard(todos_) });
    }

    Host* host_ = nullptr;
    TodoList todos_;
    std::optional<Todo> deleted_;
    bool live_ = false;
};

} // namespace cup::test::fixture
