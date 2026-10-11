#pragma once
// Owns the providers and connects them to the island: gives each one a Host
// that stamps its id on everything it posts, and routes clicked actions back.

#include "core/provider.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cup {

class ProviderRegistry {
public:
    using Dispatch = std::function<void(const Event& event, const char* reason)>;
    using Clock = std::function<Ms()>;

    ProviderRegistry(Dispatch dispatch, Clock clock);
    ~ProviderRegistry(); // stops everything still running

    void add(std::unique_ptr<Provider> provider);
    void start(const std::string& providerId);
    void stop(const std::string& providerId);
    bool isRunning(const std::string& providerId) const;

    // A click on one of a provider's buttons. Unknown or stopped providers are ignored.
    void route(const Action& action);

private:
    class Handle; // the Host a provider sees
    struct Entry {
        std::unique_ptr<Provider> provider;
        std::unique_ptr<Handle> host;
        bool running = false;
    };
    Entry* find(const std::string& providerId);
    const Entry* find(const std::string& providerId) const;

    Dispatch dispatch_;
    Clock clock_;
    std::vector<Entry> entries_;
};

} // namespace cup
