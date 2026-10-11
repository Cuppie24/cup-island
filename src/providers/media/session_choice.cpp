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
