#pragma once
// Providers' commands (decision 160): things a provider can do from
// anywhere, published to the host and run by another provider (Search)
// for the user. The host keeps them beside the island's state, as it keeps
// windows. Pure: who published what, and whether a run may go ahead.

#include "core/content.h"
#include "core/open_windows.h"

#include <map>
#include <string>
#include <vector>

namespace cup {

// One command, as its provider publishes it. Running it sends the provider
// Action{activityId, actionId = id}: the action its own button sends, so it
// needs no second code path.
struct CommandSpec {
    std::string id;                    // letters, digits, '-', '_', '.' (validWindowId's rule)
    std::string title;                 // what the user reads and types: "Open the Pomodoro timer"
    std::vector<std::string> keywords; // more to match, lower case: "focus", "tasks"
    Glyph glyph = Glyph::OpenApp;
    std::string activityId;            // the card its button is on, carried in the Action
    bool operator==(const CommandSpec&) const = default;
};

// A published command, with whose it is.
struct PublishedCommand {
    std::string providerId;
    CommandSpec spec;
    bool operator==(const PublishedCommand&) const = default;
};

struct Commands {
    std::map<std::string, std::vector<CommandSpec>> byProvider;
    // The user action (Windows::actedAt) each provider last ran a command
    // with: one gesture, one command.
    std::map<std::string, Ms> spent;
    bool operator==(const Commands&) const = default;
};

// Replaces the provider's list. Bad ids, empty titles and repeated ids are
// dropped; past commandsPerProvider, the rest are.
void publishCommands(Commands& commands, const std::string& providerId, std::vector<CommandSpec> list);
// A provider that stopped: its commands go.
void removeCommands(Commands& commands, const std::string& providerId);

// Every command but `except`'s own, by provider id, in each one's order.
std::vector<PublishedCommand> listCommands(const Commands& commands, const std::string& except);
const CommandSpec* findCommand(const Commands& commands, const std::string& providerId, const std::string& commandId);

enum class CommandRun {
    Ran,
    NotNow,     // the caller isn't just after a user action, or that action already ran one
    Unknown,    // no such command (its provider removed it, or stopped)
    OwnCommand, // a provider doesn't run its own through the host
    NotAllowed, // the caller wasn't granted commands (the registry decides)
};

// `caller` runs another provider's command for the user, now. Ran: the
// caller's action is spent, and the user counts as having acted on the
// owner (so it may open a window, decision 104); the caller routes the action.
CommandRun runCommand(Commands& commands, Windows& windows, const std::string& caller, const std::string& providerId,
                      const std::string& commandId, Ms now);

} // namespace cup
