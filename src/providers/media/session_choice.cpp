#include "providers/media/session_choice.h"

namespace cup {

std::optional<std::size_t> findSession(const std::vector<SessionView>& sessions, const std::string& app)
{
    for (std::size_t i = 0; i < sessions.size(); ++i)
        if (sessions[i].remembered) return i;
    for (std::size_t i = 0; i < sessions.size(); ++i)
        if (!app.empty() && sessions[i].app == app) return i;
    return std::nullopt;
}

std::optional<std::size_t> watchedSession(const std::vector<SessionView>& watched, const std::string& app)
{
    for (std::size_t i = 0; i < watched.size(); ++i)
        if (watched[i].remembered) return i;
    std::optional<std::size_t> only;
    for (std::size_t i = 0; i < watched.size(); ++i) {
        if (app.empty() || watched[i].app != app) continue;
        if (only) return std::nullopt; // two of its app: either could be it
        only = i;
    }
    return only;
}

bool mainMediaMoved(const std::string& lastApp, const std::string& currentApp)
{
    return !currentApp.empty() && currentApp != lastApp;
}

std::size_t nextSession(std::size_t count, std::optional<std::size_t> shown)
{
    if (count == 0) return 0;
    return shown ? (*shown + 1) % count : 0;
}

} // namespace cup
