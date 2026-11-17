#include "core/commands.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {

void publishCommands(Commands& c, const std::string& providerId, std::vector<CommandSpec> list)
{
    std::vector<CommandSpec> kept;
    for (CommandSpec& spec : list) {
        if (kept.size() >= tokens().commandsPerProvider) break;
        bool repeated = std::any_of(kept.begin(), kept.end(), [&](const CommandSpec& k) { return k.id == spec.id; });
        if (!validWindowId(spec.id) || spec.title.empty() || repeated) continue;
        kept.push_back(std::move(spec));
    }
    if (kept.empty()) c.byProvider.erase(providerId);
    else c.byProvider[providerId] = std::move(kept);
}

void removeCommands(Commands& c, const std::string& providerId)
{
    c.byProvider.erase(providerId);
    c.spent.erase(providerId);
}

std::vector<PublishedCommand> listCommands(const Commands& c, const std::string& except)
{
    std::vector<PublishedCommand> out;
    for (const auto& [providerId, list] : c.byProvider)
        if (providerId != except)
            for (const CommandSpec& spec : list) out.push_back({ providerId, spec });
    return out;
}

const CommandSpec* findCommand(const Commands& c, const std::string& providerId, const std::string& commandId)
{
    auto it = c.byProvider.find(providerId);
    if (it == c.byProvider.end()) return nullptr;
    for (const CommandSpec& spec : it->second)
        if (spec.id == commandId) return &spec;
    return nullptr;
}

CommandRun runCommand(Commands& c, Windows& windows, const std::string& caller, const std::string& providerId,
                      const std::string& commandId, Ms now)
{
    if (caller == providerId) return CommandRun::OwnCommand; // it needs no host to do its own
    if (!findCommand(c, providerId, commandId)) return CommandRun::Unknown;
    // Only for the user, right after they acted on the caller (decision 104's
    // guard): never from a wake-up, a timer or a reply. And once per action,
    // so one click can't open a chain of windows. Two clicks within the same
    // millisecond count as one.
    std::optional<Ms> acted = justActed(windows, caller, now);
    if (!acted) return CommandRun::NotNow;
    if (auto spent = c.spent.find(caller); spent != c.spent.end() && spent->second == *acted) return CommandRun::NotNow;
    c.spent[caller] = *acted;
    userActed(windows, providerId, now); // the gesture passes to the owner: it may open its window
    return CommandRun::Ran;
}

} // namespace cup
