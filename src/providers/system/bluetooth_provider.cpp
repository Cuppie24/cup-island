#include "providers/system/bluetooth_provider.h"
#include "platform/log.h"

#include <cwctype>
#include <optional>

namespace cup {
namespace {

namespace wde = winrt::Windows::Devices::Enumeration;
namespace wdr = winrt::Windows::Devices::Radios;
namespace wf = winrt::Windows::Foundation;
using Properties = wf::Collections::IMapView<winrt::hstring, wf::IInspectable>;

// Bluetooth's device nodes: Classic devices and their services (BTHENUM),
// Low Energy ones (BTHLE). Plug and Play records, read without the radio.
constexpr wchar_t kBluetoothNodes[] = L"System.Devices.EnumeratorName:=\"BTHENUM\" OR System.Devices.EnumeratorName:=\"BTHLE\"";
constexpr wchar_t kContainerId[] = L"System.Devices.ContainerId";
// The node's "is connected" record (the one Settings' device list follows).
constexpr wchar_t kConnected[] = L"{83DA6326-97A6-4088-9453-A1923F573B29} 15";
// DEVPKEY_Bluetooth_Battery: the level Windows reads from the device and shows in Settings.
constexpr wchar_t kBattery[] = L"{104EA319-6EE2-4701-BD47-8DDBF425BBE5} 2";

template <typename T> std::optional<T> property(const Properties& properties, const wchar_t* key)
{
    if (!properties || !properties.HasKey(key)) return std::nullopt;
    auto value = properties.Lookup(key);
    if (auto ref = value ? value.try_as<wf::IReference<T>>() : nullptr) return ref.Value();
    return std::nullopt;
}

std::wstring upper(std::wstring s)
{
    for (wchar_t& c : s) c = static_cast<wchar_t>(std::towupper(c));
    return s;
}

// The device's own node (BTHENUM\DEV_..., BTHLE\DEV_...), not one of its services: its name is the device's.
bool deviceNode(const std::wstring& id) { return upper(id).find(L"\\DEV_") != std::wstring::npos; }
bool bluetoothNode(const std::wstring& id)
{
    std::wstring u = upper(id);
    return u.rfind(L"BTHENUM\\", 0) == 0 || u.rfind(L"BTHLE\\", 0) == 0 || u.find(L"\\BTHENUM#") != std::wstring::npos ||
           u.find(L"\\BTHLE#") != std::wstring::npos;
}

BluetoothNode reading(const std::wstring& id, const Properties& properties, std::optional<winrt::hstring> name)
{
    BluetoothNode r{ .id = winrt::to_string(id) };
    if (auto container = property<winrt::guid>(properties, kContainerId)) r.container = winrt::to_string(winrt::to_hstring(*container));
    if (name && deviceNode(id) && !name->empty()) r.name = winrt::to_string(*name);
    r.connected = property<bool>(properties, kConnected);
    if (auto level = property<std::uint8_t>(properties, kBattery)) r.battery = *level;
    return r;
}

void logReading(const BluetoothNode& r)
{
    platform::logLine("bluetooth: node %s container=%s connected=%d battery=%d%s", r.id.c_str(), r.container.c_str(),
                      r.connected ? int(*r.connected) : -1, r.battery.value_or(-1), r.name ? (" name=" + *r.name).c_str() : "");
}

BluetoothRadio radioOf(const wdr::Radio& radio)
{
    auto state = radio.State();
    return state == wdr::RadioState::On ? BluetoothRadio::On : state == wdr::RadioState::Unknown ? BluetoothRadio::Unknown : BluetoothRadio::Off;
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
    state_ = {};
    listed_ = false;
    radioRead_ = false;
    watchNodes();
    followRadio(alive_);
}

void BluetoothProvider::stop()
{
    *alive_ = false;
    added_.revoke();
    updated_.revoke();
    removed_.revoke();
    completed_.revoke();
    if (nodes_) {
        auto status = nodes_.Status();
        if (status == wde::DeviceWatcherStatus::Started || status == wde::DeviceWatcherStatus::EnumerationCompleted) nodes_.Stop();
    }
    nodes_ = nullptr;
    radioChanged_.revoke();
    radio_ = nullptr;
    presenter_->show({ BluetoothRadio::Missing, {} });
    presenter_.reset();
}

// Plug and Play's records of Bluetooth's device nodes: a watcher over them
// stays running (it's told of changes, it doesn't ask the devices), so a
// device connecting, dropping or reporting a new level shows at once.
void BluetoothProvider::watchNodes()
{
    try {
        nodes_ = wde::DeviceInformation::CreateWatcher(kBluetoothNodes, { kContainerId, kConnected, kBattery }, wde::DeviceInformationKind::Device);
    } catch (winrt::hresult_error const& e) {
        platform::logLine("bluetooth: can't watch device nodes (0x%08x)", static_cast<unsigned>(e.code()));
        listed_ = true; // nothing to list: the radio alone (its switch) still shows
        return;
    }
    added_ = nodes_.Added(winrt::auto_revoke, [this](auto&&, wde::DeviceInformation const& info) {
        std::wstring id(info.Id());
        if (!bluetoothNode(id)) return;
        BluetoothNode r = reading(id, info.Properties(), info.Name());
        post([this, r] {
            logReading(r);
            state_.node(r);
            publish();
        });
    });
    updated_ = nodes_.Updated(winrt::auto_revoke, [this](auto&&, wde::DeviceInformationUpdate const& update) {
        std::wstring id(update.Id());
        if (!bluetoothNode(id)) return;
        BluetoothNode r = reading(id, update.Properties(), std::nullopt);
        post([this, r] {
            logReading(r);
            state_.node(r);
            publish();
        });
    });
    removed_ = nodes_.Removed(winrt::auto_revoke, [this](auto&&, wde::DeviceInformationUpdate const& update) {
        post([this, id = winrt::to_string(update.Id())] {
            state_.nodeRemoved(id);
            publish();
        });
    });
    completed_ = nodes_.EnumerationCompleted(winrt::auto_revoke, [this](auto&&, auto&&) {
        post([this] {
            listed_ = true;
            platform::logLine("bluetooth: device nodes listed; %zu connected", state_.connected().size());
            publish();
        });
    });
    nodes_.Start();
}

// The radio: off, nothing is connected; none, no widget. Reading it is local, no traffic.
winrt::fire_and_forget BluetoothProvider::followRadio(std::shared_ptr<bool> alive)
{
    try {
        auto radios = co_await Radio::GetRadiosAsync();
        if (!*alive) co_return;
        for (Radio const& r : radios) {
            if (r.Kind() != wdr::RadioKind::Bluetooth) continue;
            post([this, r] {
                radio_ = r;
                radioChanged_ = radio_.StateChanged(winrt::auto_revoke, [this](Radio const& radio, auto&&) {
                    BluetoothRadio now = radioOf(radio);
                    post([this, now] {
                        platform::logLine("bluetooth: radio %s", now == BluetoothRadio::On ? "on" : "off");
                        state_.radio(now);
                        publish();
                    });
                });
                state_.radio(radioOf(radio_));
                radioRead_ = true;
                publish();
            });
            co_return;
        }
        post([this] {
            platform::logLine("bluetooth: no Bluetooth radio");
            state_.radio(BluetoothRadio::Missing);
            radioRead_ = true;
            publish();
        });
    } catch (...) { // decision 158: nothing leaves a fire_and_forget
        if (*alive) post([this] {
            platform::logLine("bluetooth: can't read the radio");
            radioRead_ = true; // the devices still show, without a switch
            publish();
        });
    }
}

void BluetoothProvider::onAction(const Action& action)
{
    if (action.actionId != bluetooth_action::kToggle || !radio_) return;
    setRadio(state_.radio() != BluetoothRadio::On, alive_);
}

// The switch: the user asked, so the radio is switched (Windows asks
// nothing of a desktop app; denied, the switch stays where it was).
winrt::fire_and_forget BluetoothProvider::setRadio(bool on, std::shared_ptr<bool> alive)
{
    try {
        Radio radio = radio_;
        auto access = co_await Radio::RequestAccessAsync();
        if (!*alive) co_return;
        if (access != wdr::RadioAccessStatus::Allowed) {
            platform::logLine("bluetooth: not allowed to switch the radio (%d)", static_cast<int>(access));
            co_return;
        }
        auto result = co_await radio.SetStateAsync(on ? wdr::RadioState::On : wdr::RadioState::Off);
        if (*alive && result != wdr::RadioAccessStatus::Allowed)
            platform::logLine("bluetooth: switching the radio failed (%d)", static_cast<int>(result));
    } catch (...) { // decision 158
        if (*alive) platform::logLine("bluetooth: switching the radio failed");
    }
}

void BluetoothProvider::publish()
{
    if (!listed_ || !radioRead_) return; // not everything read yet: no half a list
    presenter_->show({ state_.radio(), state_.connected() });
}

} // namespace cup
