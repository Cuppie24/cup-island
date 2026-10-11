#include "providers/system/bluetooth_provider.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace cup {
namespace {

namespace wde = winrt::Windows::Devices::Enumeration;
namespace wf = winrt::Windows::Foundation;

// Paired Bluetooth devices: Classic and LE protocol ids.
constexpr wchar_t kPairedAqs[] =
    L"(System.Devices.Aep.ProtocolId:=\"{e0cbf06c-cd8b-4647-bb8a-263b43f0f974}\" OR "
    L"System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\") AND "
    L"System.Devices.Aep.IsPaired:=System.StructuredQueryType.Boolean#True";
constexpr wchar_t kIsConnected[] = L"System.Devices.Aep.IsConnected";
constexpr wchar_t kContainerId[] = L"System.Devices.Aep.ContainerId";
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
    auto status = watcher.Status();
    if (status == wde::DeviceWatcherStatus::Started || status == wde::DeviceWatcherStatus::EnumerationCompleted) watcher.Stop();
}

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
    try {
        endpoints_ = Info::CreateWatcher(kPairedAqs, { kIsConnected, kContainerId }, wde::DeviceInformationKind::AssociationEndpoint);
    } catch (winrt::hresult_error const& e) {
        std::printf("[cup-island] bluetooth: can't watch devices (0x%08x)\n", static_cast<unsigned>(e.code()));
        return;
    }
    endpointAdded_ = endpoints_.Added(winrt::auto_revoke, [this](auto&&, Info const& info) {
        post([this, info] {
            std::string name = winrt::to_string(info.Name());
            endpointChanged(std::wstring(info.Id()), info.Properties(), &name);
        });
    });
    endpointUpdated_ = endpoints_.Updated(winrt::auto_revoke, [this](auto&&, Update const& update) {
        post([this, update] { endpointChanged(std::wstring(update.Id()), update.Properties(), nullptr); });
    });
    endpointRemoved_ = endpoints_.Removed(winrt::auto_revoke, [this](auto&&, Update const& update) {
        post([this, update] {
            devices_.erase(std::wstring(update.Id()));
            watchBatteries();
            publish();
        });
    });
    endpoints_.Start();
}

void BluetoothProvider::stop()
{
    *alive_ = false;
    endpointAdded_.revoke();
    endpointUpdated_.revoke();
    endpointRemoved_.revoke();
    if (endpoints_) stopWatcher(endpoints_);
    endpoints_ = nullptr;
    for (auto& [container, watch] : batteries_) stopWatcher(watch.watcher);
    batteries_.clear();
    devices_.clear();
    presenter_->show({});
    presenter_.reset();
}

// Added (with a name) or updated: only the properties that changed are present.
void BluetoothProvider::endpointChanged(const std::wstring& id, const Properties& properties, const std::string* name)
{
    Endpoint& device = devices_[id];
    if (name) device.name = *name;
    if (auto container = property<winrt::guid>(properties, kContainerId)) device.container = std::wstring(winrt::to_hstring(*container));
    if (auto connected = property<bool>(properties, kIsConnected)) {
        if (*connected && !device.connected) device.connectedOrder = nextOrder_++;
        device.connected = *connected;
    }
    watchBatteries();
    publish();
}

void BluetoothProvider::watchBatteries()
{
    std::vector<std::wstring> wanted;
    for (const auto& [id, device] : devices_)
        if (device.connected && !device.container.empty()) wanted.push_back(device.container);

    for (auto it = batteries_.begin(); it != batteries_.end();) {
        if (std::find(wanted.begin(), wanted.end(), it->first) != wanted.end()) {
            ++it;
            continue;
        }
        stopWatcher(it->second.watcher); // disconnected
        it = batteries_.erase(it);
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
                publish();
            });
        });
        watch.watcher.Start();
    }
}

void BluetoothProvider::publish()
{
    std::vector<std::pair<std::uint64_t, BluetoothDevice>> connected;
    for (const auto& [id, device] : devices_) {
        if (!device.connected) continue;
        BluetoothDevice d{ winrt::to_string(id), device.name, std::nullopt };
        if (auto watch = batteries_.find(device.container); watch != batteries_.end() && !watch->second.levels.empty()) {
            int best = 0;
            for (const auto& [node, value] : watch->second.levels) best = std::max(best, value);
            d.battery = best;
        }
        connected.push_back({ device.connectedOrder, d });
    }
    std::sort(connected.begin(), connected.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<BluetoothDevice> devices;
    for (auto& [order, d] : connected) devices.push_back(std::move(d));
    presenter_->show(devices);
}

} // namespace cup
