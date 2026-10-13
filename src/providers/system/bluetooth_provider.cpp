#include "providers/system/bluetooth_provider.h"
#include "platform/log.h"

#include "core/tokens.h"

#include <algorithm>
#include <vector>

namespace cup {
namespace {

namespace wde = winrt::Windows::Devices::Enumeration;
namespace wdb = winrt::Windows::Devices::Bluetooth;
namespace wdr = winrt::Windows::Devices::Radios;
namespace wf = winrt::Windows::Foundation;

// Paired Bluetooth devices: Classic and LE protocol ids.
constexpr winrt::guid kClassicProtocol{ 0xe0cbf06c, 0xcd8b, 0x4647, { 0xbb, 0x8a, 0x26, 0x3b, 0x43, 0xf0, 0xf9, 0x74 } };
constexpr winrt::guid kLeProtocol{ 0xbb7bb05e, 0x5972, 0x42b5, { 0x94, 0xfc, 0x76, 0xea, 0xa7, 0x08, 0x4d, 0x49 } };
constexpr wchar_t kPairedAqs[] =
    L"(System.Devices.Aep.ProtocolId:=\"{e0cbf06c-cd8b-4647-bb8a-263b43f0f974}\" OR "
    L"System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\") AND "
    L"System.Devices.Aep.IsPaired:=System.StructuredQueryType.Boolean#True";
constexpr wchar_t kIsConnected[] = L"System.Devices.Aep.IsConnected";
constexpr wchar_t kContainerId[] = L"System.Devices.Aep.ContainerId";
constexpr wchar_t kProtocolId[] = L"System.Devices.Aep.ProtocolId";
// DEVPKEY_Bluetooth_Battery: the level Windows reads from the device and shows in Settings.
constexpr wchar_t kBattery[] = L"{104EA319-6EE2-4701-BD47-8DDBF425BBE5} 2";

template <typename T> std::optional<T> property(const wf::Collections::IMapView<winrt::hstring, wf::IInspectable>& properties, const wchar_t* key)
{
    if (!properties || !properties.HasKey(key)) return std::nullopt;
    auto value = properties.Lookup(key);
    if (auto ref = value ? value.try_as<wf::IReference<T>>() : nullptr) return ref.Value();
    return std::nullopt;
}

void stopWatcher(const wde::DeviceWatcher& watcher)
{
    if (!watcher) return;
    auto status = watcher.Status();
    if (status == wde::DeviceWatcherStatus::Started || status == wde::DeviceWatcherStatus::EnumerationCompleted) watcher.Stop();
}

std::string key(const std::wstring& id) { return winrt::to_string(id); }

} // namespace

void BluetoothProvider::post(std::function<void()> work)
{
    post_([alive = alive_, work = std::move(work)] {
        if (*alive) work();
    });
}

void BluetoothProvider::start(Host& host)
{
    presenter_ = std::make_unique<BluetoothPresenter>(host);
    alive_ = std::make_shared<bool>(true);
    state_ = {};
    followRadio(alive_);
    listPaired();
    relistEvery(tokens().bluetoothRelistInterval, alive_);
}

// A one-off listing (decision 039): the watcher is stopped once it has listed
// the paired devices. Left running, a Bluetooth device watcher keeps the radio
// busy, and music on Bluetooth headphones stutters while it does.
void BluetoothProvider::listPaired()
{
    if (endpoints_) return; // one is under way
    try {
        endpoints_ = Info::CreateWatcher(kPairedAqs, { kIsConnected, kContainerId, kProtocolId }, wde::DeviceInformationKind::AssociationEndpoint);
    } catch (winrt::hresult_error const& e) {
        platform::logLine("bluetooth: can't list devices (0x%08x)", static_cast<unsigned>(e.code()));
        return;
    }
    seen_.clear();
    endpointAdded_ = endpoints_.Added(winrt::auto_revoke, [this](auto&&, Info const& info) {
        post([this, info] {
            seen_.insert(std::wstring(info.Id()));
            endpointChanged(std::wstring(info.Id()), info.Properties(), winrt::to_string(info.Name()));
        });
    });
    endpointUpdated_ = endpoints_.Updated(winrt::auto_revoke, [this](auto&&, Update const& update) {
        post([this, update] { endpointChanged(std::wstring(update.Id()), update.Properties(), std::nullopt); });
    });
    endpointRemoved_ = endpoints_.Removed(winrt::auto_revoke, [this](auto&&, Update const& update) {
        post([this, id = std::wstring(update.Id())] { seen_.erase(id); });
    });
    endpointsListed_ = endpoints_.EnumerationCompleted(winrt::auto_revoke, [this](auto&&, auto&&) { post([this] { finishListing(true); }); });
    endpointsStopped_ = endpoints_.Stopped(winrt::auto_revoke, [this](auto&&, auto&&) { post([this] { finishListing(false); }); });
    endpoints_.Start();
}

void BluetoothProvider::finishListing(bool complete)
{
    if (!endpoints_) return; // already finished (our own Stop() also raises Stopped)
    endpointAdded_.revoke();
    endpointUpdated_.revoke();
    endpointRemoved_.revoke();
    endpointsListed_.revoke();
    endpointsStopped_.revoke();
    stopWatcher(endpoints_);
    endpoints_ = nullptr;
    if (!complete) {
        platform::logLine("bluetooth: listing aborted; trying again later");
        return; // a partial list can't tell what was unpaired
    }

    // Known devices this listing didn't report were unpaired meanwhile.
    for (auto it = paired_.begin(); it != paired_.end();) {
        if (seen_.count(it->first)) {
            ++it;
            continue;
        }
        state_.unpaired(key(it->first));
        it = paired_.erase(it); // revokes its status handler
    }
    platform::logLine("bluetooth: %zu paired device(s) listed; watcher stopped", paired_.size());
    watchBatteries();
    publish();
}

// New pairings and removals are picked up by listing again now and then
// (the devices' own events and the radio cover connections in between).
winrt::fire_and_forget BluetoothProvider::relistEvery(Ms interval, std::shared_ptr<bool> alive)
{
    auto toUi = post_; // `this` is only touched on the UI thread, after the alive check
    interval = std::max(interval, Ms{ 60000 });
    for (;;) {
        co_await winrt::resume_after(interval);
        if (!*alive) co_return;
        toUi([this, alive] {
            if (*alive) listPaired();
        });
    }
}

void BluetoothProvider::stop()
{
    *alive_ = false;
    endpointAdded_.revoke();
    endpointUpdated_.revoke();
    endpointRemoved_.revoke();
    endpointsListed_.revoke();
    endpointsStopped_.revoke();
    stopWatcher(endpoints_);
    endpoints_ = nullptr;
    radioChanged_.revoke();
    radio_ = nullptr;
    for (auto& [container, watch] : batteries_) stopWatcher(watch.watcher);
    batteries_.clear();
    paired_.clear();
    state_ = {};
    presenter_->show({});
    presenter_.reset();
}

// Added (with a name) or updated: only the properties that changed are present.
void BluetoothProvider::endpointChanged(const std::wstring& id, const Properties& properties, const std::optional<std::string>& name)
{
    Paired& device = paired_[id];
    if (auto container = property<winrt::guid>(properties, kContainerId)) device.container = std::wstring(winrt::to_hstring(*container));
    state_.listed(key(id), name, property<bool>(properties, kIsConnected));
    if (!device.watched) {
        if (auto protocol = property<winrt::guid>(properties, kProtocolId); protocol && (*protocol == kClassicProtocol || *protocol == kLeProtocol)) {
            device.watched = true;
            followDevice(id, *protocol == kLeProtocol, alive_);
        }
    }
    watchBatteries();
    publish();
}

// The device's own connection status: raised when it connects or drops,
// which the paired list's IsConnected often misses (decision 039). Opening
// the device object doesn't connect to it.
winrt::fire_and_forget BluetoothProvider::followDevice(std::wstring id, bool lowEnergy, std::shared_ptr<bool> alive)
{
    try {
        if (lowEnergy) {
            Le device = co_await Le::FromIdAsync(id);
            if (!device || !*alive) co_return; // stopped meanwhile: `this` may be gone
            post([this, id, device] {
                auto it = paired_.find(id);
                if (it == paired_.end()) return; // unpaired meanwhile
                it->second.le = device;
                it->second.leChanged = device.ConnectionStatusChanged(winrt::auto_revoke, [this, id](Le const& d, auto&&) {
                    bool connected = d.ConnectionStatus() == wdb::BluetoothConnectionStatus::Connected;
                    post([this, id, connected] { state_.status(key(id), connected); watchBatteries(); publish(); });
                });
                state_.status(key(id), device.ConnectionStatus() == wdb::BluetoothConnectionStatus::Connected);
                watchBatteries();
                publish();
            });
        } else {
            Classic device = co_await Classic::FromIdAsync(id);
            if (!device || !*alive) co_return;
            post([this, id, device] {
                auto it = paired_.find(id);
                if (it == paired_.end()) return;
                it->second.classic = device;
                it->second.classicChanged = device.ConnectionStatusChanged(winrt::auto_revoke, [this, id](Classic const& d, auto&&) {
                    bool connected = d.ConnectionStatus() == wdb::BluetoothConnectionStatus::Connected;
                    post([this, id, connected] { state_.status(key(id), connected); watchBatteries(); publish(); });
                });
                state_.status(key(id), device.ConnectionStatus() == wdb::BluetoothConnectionStatus::Connected);
                watchBatteries();
                publish();
            });
        }
    } catch (winrt::hresult_error const& e) {
        // No device object: the paired list's word is all there is for this one.
        if (*alive) platform::logLine("bluetooth: can't follow a device (0x%08x)", static_cast<unsigned>(e.code()));
    }
}

// The radio switched off: nothing is connected, whatever the lists still say.
// No Bluetooth radio found: the devices' own word stands.
winrt::fire_and_forget BluetoothProvider::followRadio(std::shared_ptr<bool> alive)
{
    try {
        auto radios = co_await Radio::GetRadiosAsync();
        if (!*alive) co_return;
        for (Radio const& r : radios) {
            if (r.Kind() != wdr::RadioKind::Bluetooth) continue;
            post([this, r] {
                radio_ = r;
                auto onNow = [](Radio const& radio) { return radio.State() != wdr::RadioState::Off && radio.State() != wdr::RadioState::Disabled; };
                radioChanged_ = radio_.StateChanged(winrt::auto_revoke, [this, onNow](Radio const& radio, auto&&) {
                    bool on = onNow(radio);
                    post([this, on] {
                        state_.radio(on);
                        if (on) listPaired(); // anything paired while it was off
                        watchBatteries();
                        publish();
                    });
                });
                state_.radio(onNow(radio_));
                watchBatteries();
                publish();
            });
            co_return;
        }
    } catch (winrt::hresult_error const& e) {
        if (*alive) platform::logLine("bluetooth: can't read the radio (0x%08x)", static_cast<unsigned>(e.code()));
    }
}

void BluetoothProvider::watchBatteries()
{
    std::vector<std::wstring> wanted;
    for (const auto& [id, device] : paired_)
        if (state_.connected(key(id)) && !device.container.empty()) wanted.push_back(device.container);

    for (auto it = batteries_.begin(); it != batteries_.end();) {
        if (std::find(wanted.begin(), wanted.end(), it->first) != wanted.end()) {
            ++it;
            continue;
        }
        stopWatcher(it->second.watcher); // disconnected
        std::wstring container = it->first;
        it = batteries_.erase(it);
        batteryChanged(container); // its level goes with it
    }

    for (const std::wstring& container : wanted) {
        if (batteries_.count(container)) continue;
        std::wstring aqs = L"System.Devices.ContainerId:=\"" + container + L"\"";
        BatteryWatch& watch = batteries_[container];
        try {
            watch.watcher = Info::CreateWatcher(aqs, { kBattery }, wde::DeviceInformationKind::Device);
        } catch (winrt::hresult_error const&) {
            continue; // no battery for this one; it still shows as connected
        }
        auto level = [this, container](std::wstring node, std::optional<std::uint8_t> value) {
            post([this, container, node, value] {
                auto watch = batteries_.find(container);
                if (watch == batteries_.end() || !value) return;
                watch->second.levels[node] = *value;
                batteryChanged(container);
                publish();
            });
        };
        watch.added = watch.watcher.Added(winrt::auto_revoke, [level](auto&&, Info const& info) {
            level(std::wstring(info.Id()), property<std::uint8_t>(info.Properties(), kBattery));
        });
        watch.updated = watch.watcher.Updated(winrt::auto_revoke, [level](auto&&, Update const& update) {
            level(std::wstring(update.Id()), property<std::uint8_t>(update.Properties(), kBattery));
        });
        watch.removed = watch.watcher.Removed(winrt::auto_revoke, [this, container](auto&&, Update const& update) {
            post([this, container, node = std::wstring(update.Id())] {
                if (auto watch = batteries_.find(container); watch != batteries_.end()) watch->second.levels.erase(node);
                batteryChanged(container);
                publish();
            });
        });
        watch.watcher.Start();
    }
}

// The best level any of the device's nodes reports, for each paired endpoint in that group.
void BluetoothProvider::batteryChanged(const std::wstring& container)
{
    std::optional<int> best;
    if (auto watch = batteries_.find(container); watch != batteries_.end())
        for (const auto& [node, value] : watch->second.levels) best = std::max(best.value_or(0), value);
    for (const auto& [id, device] : paired_)
        if (device.container == container) state_.battery(key(id), best);
}

void BluetoothProvider::publish() { presenter_->show(state_.shown()); }

} // namespace cup
