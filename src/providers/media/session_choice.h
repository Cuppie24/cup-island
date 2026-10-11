#pragma once
// Which media session the island shows (decision 005), on plain data so the
// rules are tested on Linux. Sessions are matched by object first, then by
// app id: Windows can hand out a new object for the same session, and an
// identity-only match made the source button lose its pick (first click
// appeared to do nothing).

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace cup {

struct SessionView {
    std::string app;         // SourceAppUserModelId
    bool remembered = false; // the very object we stored
};

// Where a remembered session is now: the same object, else the first session
// of its app. Nothing if its app is gone.
std::optional<std::size_t> findSession(const std::vector<SessionView>& sessions, const std::string& app);

// Did Windows move its main media to another app since `lastApp`? Having no
// main media for a moment ("") is not a move.
bool mainMediaMoved(const std::string& lastApp, const std::string& currentApp);

// The source button's next session after `shown`, wrapping around.
std::size_t nextSession(std::size_t count, std::optional<std::size_t> shown);

} // namespace cup
