#include "providers/system/battery_provider.h"

#include <winrt/Windows.System.Power.h>

namespace cup {

using winrt::Windows::System::Power::BatteryStatus;
using winrt::Windows::System::Power::PowerManager;
using winrt::Windows::System::Power::PowerSupplyStatus;

void BatteryProvider::start(Host& host)
{
    presenter_ = std::make_unique<BatteryPresenter>(host);
    alive_ = std::make_shared<bool>(true);
    // Static events: subscribed by token, removed in stop().
    auto changed = [this, alive = alive_](auto&&...) {
        post_([this, alive] {
            if (*alive) read();
        });
    };
    chargeChanged_ = PowerManager::RemainingChargePercentChanged(changed);
    statusChanged_ = PowerManager::BatteryStatusChanged(changed);
    supplyChanged_ = PowerManager::PowerSupplyStatusChanged(changed);
    read();
}

void BatteryProvider::stop()
{
    *alive_ = false;
    PowerManager::RemainingChargePercentChanged(chargeChanged_);
    PowerManager::BatteryStatusChanged(statusChanged_);
    PowerManager::PowerSupplyStatusChanged(supplyChanged_);
    presenter_->show(std::nullopt);
    presenter_.reset();
}

void BatteryProvider::read()
{
    BatteryStatus status = PowerManager::BatteryStatus();
    if (status == BatteryStatus::NotPresent) {
        presenter_->show(std::nullopt); // a desktop: no widget
        return;
    }
    bool onCharger = PowerManager::PowerSupplyStatus() != PowerSupplyStatus::NotPresent;
    bool pluggedIn = status == BatteryStatus::Charging || (onCharger && status == BatteryStatus::Idle);
    presenter_->show(BatteryReading{ PowerManager::RemainingChargePercent(), pluggedIn });
}

} // namespace cup
