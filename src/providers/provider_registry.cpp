#include "providers/provider_registry.h"

#include "providers/file_store.h"

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
    Ms utcOffset() const override { return registry_.store_.utcOffset ? registry_.store_.utcOffset() : Ms{ 0 }; }

    void publishPanel(Panel panel) override
    {
        panel.providerId = providerId_;
        registry_.dispatch_(PanelPublished{ std::move(panel) }, providerId_.c_str());
    }

    void removePanel(const std::string& panelId) override
    {
        registry_.dispatch_(PanelRemoved{ panelId }, providerId_.c_str());
    }

    void ask(Dialog dialog) override
    {
        dialog.providerId = providerId_;
        registry_.dispatch_(DialogShown{ std::move(dialog) }, providerId_.c_str());
    }

    void toast(Toast toast) override
    {
        toast.providerId = providerId_;
        registry_.dispatch_(ToastShown{ std::move(toast) }, providerId_.c_str());
    }

    void reveal(const std::string& activityId, const std::string& list, std::size_t row) override
    {
        registry_.dispatch_(ListRevealed{ activityId, list, row }, providerId_.c_str());
    }

    void wakeAt(std::optional<Ms> at) override
    {
        if (Entry* e = registry_.find(providerId_)) e->wake = at;
    }

    std::string load(const std::string& key) const override
    {
        if (!validStoreKey(key)) return {};
        if (registry_.store_.load) return registry_.store_.load(providerId_, key);
        auto it = registry_.memory_.find(providerId_ + "/" + key);
        return it == registry_.memory_.end() ? std::string() : it->second;
    }

    void save(const std::string& key, const std::string& text) override
    {
        if (!validStoreKey(key)) return;
        if (registry_.store_.save) registry_.store_.save(providerId_, key, text);
        else registry_.memory_[providerId_ + "/" + key] = text;
    }

    Ms wallClock() const override { return registry_.store_.wallClock ? registry_.store_.wallClock() : registry_.clock_(); }

private:
    ProviderRegistry& registry_;
    std::string providerId_;
};

ProviderRegistry::ProviderRegistry(Dispatch dispatch, Clock clock, Store store)
    : dispatch_(std::move(dispatch)), clock_(std::move(clock)), store_(std::move(store))
{
}

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

void ProviderRegistry::cardClosed(const ShownCard& card)
{
    if (Entry* e = find(card.providerId); e && e->running) e->provider->onCardClosed(card.activityId);
}

void ProviderRegistry::cardShown(const ShownCard& card)
{
    if (Entry* e = find(card.providerId); e && e->running) e->provider->onCardShown(card.activityId);
}

void ProviderRegistry::closeRequested(const ShownCard& card)
{
    if (Entry* e = find(card.providerId); e && e->running) e->provider->onCloseRequested(card.activityId);
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
