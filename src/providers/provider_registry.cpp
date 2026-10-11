#include "providers/provider_registry.h"

namespace cup {

class ProviderRegistry::Handle : public Host {
public:
    Handle(ProviderRegistry& registry, std::string providerId) : registry_(registry), providerId_(std::move(providerId)) {}

    void startActivity(Activity activity) override
    {
        activity.providerId = providerId_;
        registry_.dispatch_(ActivityStarted{ std::move(activity) }, providerId_.c_str());
    }

    void endActivity(const std::string& activityId) override
    {
        registry_.dispatch_(ActivityEnded{ activityId }, providerId_.c_str());
    }

    void postAlert(Alert alert) override
    {
        alert.providerId = providerId_;
        registry_.dispatch_(AlertPosted{ std::move(alert) }, providerId_.c_str());
    }

    Ms now() const override { return registry_.clock_(); }

    void publishPanel(Panel panel) override
    {
        panel.providerId = providerId_;
        registry_.dispatch_(PanelPublished{ std::move(panel) }, providerId_.c_str());
    }

    void removePanel(const std::string& panelId) override
    {
        registry_.dispatch_(PanelRemoved{ panelId }, providerId_.c_str());
    }

    void wakeAt(std::optional<Ms> at) override
    {
        if (Entry* e = registry_.find(providerId_)) e->wake = at;
    }

private:
    ProviderRegistry& registry_;
    std::string providerId_;
};

ProviderRegistry::ProviderRegistry(Dispatch dispatch, Clock clock) : dispatch_(std::move(dispatch)), clock_(std::move(clock)) {}

ProviderRegistry::~ProviderRegistry()
{
    for (auto& entry : entries_)
        if (entry.running) entry.provider->stop();
}

void ProviderRegistry::add(std::unique_ptr<Provider> provider)
{
    auto host = std::make_unique<Handle>(*this, provider->id());
    entries_.push_back({ std::move(provider), std::move(host), false });
}

void ProviderRegistry::start(const std::string& providerId)
{
    if (Entry* e = find(providerId); e && !e->running) {
        e->running = true;
        e->provider->start(*e->host);
    }
}

void ProviderRegistry::stop(const std::string& providerId)
{
    if (Entry* e = find(providerId); e && e->running) {
        e->running = false;
        e->wake.reset();
        e->provider->stop();
    }
}

bool ProviderRegistry::isRunning(const std::string& providerId) const
{
    const Entry* e = find(providerId);
    return e && e->running;
}

void ProviderRegistry::route(const Action& action)
{
    if (Entry* e = find(action.providerId); e && e->running) e->provider->onAction(action);
}

std::optional<Ms> ProviderRegistry::nextWake() const
{
    std::optional<Ms> next;
    for (const auto& e : entries_)
        if (e.running && e.wake && (!next || *e.wake < *next)) next = e.wake;
    return next;
}

void ProviderRegistry::wakeDue(Ms now)
{
    for (auto& e : entries_) {
        if (!e.running || !e.wake || *e.wake > now) continue;
        e.wake.reset(); // before onWake(), which may ask for the next one
        e.provider->onWake();
    }
}

ProviderRegistry::Entry* ProviderRegistry::find(const std::string& providerId)
{
    for (auto& e : entries_)
        if (providerId == e.provider->id()) return &e;
    return nullptr;
}

const ProviderRegistry::Entry* ProviderRegistry::find(const std::string& providerId) const
{
    for (const auto& e : entries_)
        if (providerId == e.provider->id()) return &e;
    return nullptr;
}

} // namespace cup
