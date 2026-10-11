#pragma once
// Battery widget on Home (decision 009), read from Windows' PowerManager.
// Event-driven: it reads only when Windows says the charge or the power
// supply changed. Handlers run on background threads and post to the UI
// thread; every decision is in BatteryPresenter.

#include "core/provider.h"
#include "providers/system/system_panels.h"

#include <winrt/Windows.Foundation.h>

#include <functional>
#include <memory>

namespace cup {

class BatteryProvider : public Provider {
public:
    using PostToUi = std::function<void(std::function<void()>)>;
    explicit BatteryProvider(PostToUi postToUi) : post_(std::move(postToUi)) {}

    const char* id() const override { return "system.battery"; }
    void start(Host& host) override;
    void stop() override;
    void onAction(const Action&) override {}

private:
    void read();

    PostToUi post_;
    std::unique_ptr<BatteryPresenter> presenter_;
    std::shared_ptr<bool> alive_; // false after stop(): late callbacks become no-ops
    winrt::event_token chargeChanged_{}, statusChanged_{}, supplyChanged_{};
};

} // namespace cup
