#pragma once
// Owns the providers and connects them to the island: gives each one a Host
// that stamps its id on everything it posts, and routes clicked actions back.
// Also keeps their windows (decision 104), which aren't the island's state.

#include "core/provider.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cup {

class ProviderRegistry {
public:
    using Dispatch = std::function<void(const Event& event, const char* reason)>;
    using Clock = std::function<Ms()>;

    // Where providers' saved text lives (Host::load/save, decision 028). Unset
    // parts fall back to memory and to `clock` (tests).
    struct Store {
        std::function<std::string(const std::string& providerId, const std::string& key)> load;
        std::function<void(const std::string& providerId, const std::string& key, const std::string& text)> save;
        Clock wallClock;
        Clock utcOffset; // Host::utcOffset; unset: UTC
    };

    ProviderRegistry(Dispatch dispatch, Clock clock, Store store = {});
    ~ProviderRegistry(); // stops everything still running

    void add(std::unique_ptr<Provider> provider);

    // Cup Island's service (decision 078): who carries calls (ServiceHost),
    // and which modules each provider may call. A call to a module the
    // provider wasn't granted is refused here, before it goes anywhere.
    using ServiceCaller = std::function<void(const std::string& method, const std::string& params,
                                             std::function<void(ServiceReply)> done)>;
    void setServiceCaller(ServiceCaller caller) { serviceCaller_ = std::move(caller); }
    void grant(const std::string& providerId, const std::string& module);
    // Not a service module but the host's (decision 160): Host::listCommands and runCommand.
    static constexpr const char* kCommandsGrant = "commands";
    void start(const std::string& providerId);
    void stop(const std::string& providerId);
    bool isRunning(const std::string& providerId) const;

    // A click on one of a provider's buttons. Unknown or stopped providers are ignored.
    void route(const Action& action);
    // The pointer came onto or left what would send `what` (Provider::onHover). Not the user acting: opens no window.
    void hover(const Action& what, bool over);

    // A provider's card stopped being shown (shownCard changed): tells it (Provider::onCardClosed).
    void cardClosed(const ShownCard& card);
    // ...and started being shown (Provider::onCardShown).
    void cardShown(const ShownCard& card);

    // Ctrl+W on an activity's card: asks its provider to close it (Provider::onCloseRequested).
    void closeRequested(const ShownCard& card);

    // Providers' windows (decision 104): what's open, for the app to show.
    // `changed` runs after anything in it changes (opened, raised, new
    // content, a page, closed), never during a provider's own call returning.
    const Windows& windows() const { return windows_; }
    void onWindowsChanged(std::function<void()> changed) { windowsChanged_ = std::move(changed); }
    // The user closed one (✕, Alt+F4, Esc): gone, and its provider told (onWindowClosed).
    void windowClosed(const WindowKey& key);
    // The user picked a page: shown, and its provider told (Action{kWindowPage, text = page}).
    void windowPageShown(const WindowKey& key, const std::string& pageId);
    // Windows' toasts whose time is up go (decision 121): the windows' own timers call it.
    void expireWindowToasts(Ms now);

    // What providers published with Host::publishCommands (decision 160).
    const Commands& commands() const { return commands_; }

    // Wake-ups providers asked for (Host::wakeAt): the earliest, for the app's
    // one timer, and calling onWake() on every provider whose time has come.
    std::optional<Ms> nextWake() const;
    void wakeDue(Ms now);
    // `changed` runs whenever a provider asks for a different wake-up, so the
    // app re-arms its timer. A wake asked from posted work (the media
    // reader's "session gone") changes nothing on the island, so no dispatch
    // re-armed it: it waited for the next click (decision 163).
    void onWakesChanged(std::function<void()> changed) { wakesChanged_ = std::move(changed); }

private:
    class Handle; // the Host a provider sees
    struct Entry {
        std::unique_ptr<Provider> provider;
        std::unique_ptr<Handle> host;
        bool running = false;
        std::optional<Ms> wake;
    };
    Entry* find(const std::string& providerId);
    const Entry* find(const std::string& providerId) const;
    bool granted(const std::string& providerId, const std::string& module) const;

    Dispatch dispatch_;
    Clock clock_;
    Store store_;
    std::map<std::string, std::string> memory_; // "<provider>/<key>", without a store
    ServiceCaller serviceCaller_;
    std::map<std::string, std::vector<std::string>> grants_; // provider -> modules
    std::vector<Entry> entries_;
    Windows windows_;
    Commands commands_;
    std::function<void()> windowsChanged_;
    std::function<void()> wakesChanged_;
    void windowsChanged() const
    {
        if (windowsChanged_) windowsChanged_();
    }
};

} // namespace cup
