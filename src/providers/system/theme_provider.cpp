#include "providers/system/theme_provider.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <cstdio>

namespace cup {
namespace {

constexpr wchar_t kKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
constexpr wchar_t kApps[] = L"AppsUseLightTheme";
constexpr wchar_t kSystem[] = L"SystemUsesLightTheme"; // taskbar, Start

std::optional<DWORD> readValue(HKEY key, const wchar_t* name)
{
    DWORD value = 0, size = sizeof value;
    if (RegGetValueW(key, nullptr, name, RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) return std::nullopt;
    return value;
}

} // namespace

struct ThemeProvider::Watch {
    HKEY key = nullptr;
    HANDLE changed = nullptr; // auto-reset; signalled by the registry, or by stop() to end the watch
    std::atomic<bool> alive{ true }; // the watch reads it on the thread pool
    ~Watch()
    {
        if (changed) CloseHandle(changed);
        if (key) RegCloseKey(key);
    }
};

void ThemeProvider::start(Host& host)
{
    presenter_ = std::make_unique<ThemePresenter>(host);
    watch_ = std::make_shared<Watch>();
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kKey, 0, KEY_READ | KEY_NOTIFY, &watch_->key) != ERROR_SUCCESS) {
        watch_->key = nullptr;
        std::printf("[cup-island] theme: can't open the Personalize key\n");
        return; // no tile: we can't tell or change the mode
    }
    watch_->changed = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    read();
    watch(watch_);
}

void ThemeProvider::stop()
{
    watch_->alive = false;
    if (watch_->changed) SetEvent(watch_->changed); // wakes the watch so it can end
    watch_.reset();
    presenter_->show(std::nullopt);
    presenter_.reset();
}

void ThemeProvider::onAction(const Action& action)
{
    if (action.actionId != ThemePresenter::kToggle || !dark_ || !watch_) return;
    DWORD light = *dark_ ? 1 : 0; // switching: dark -> light
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return;
    RegSetValueExW(key, kApps, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&light), sizeof light);
    RegSetValueExW(key, kSystem, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&light), sizeof light);
    RegCloseKey(key);
    announce();
    read(); // show it now; the watch confirms
}

void ThemeProvider::read()
{
    std::optional<DWORD> apps = readValue(watch_->key, kApps);
    dark_ = apps ? std::optional<bool>(*apps == 0) : std::nullopt;
    presenter_->show(dark_);
}

winrt::fire_and_forget ThemeProvider::watch(std::shared_ptr<Watch> watch)
{
    auto post = post_; // `this` is only touched back on the UI thread, after the alive check
    while (watch->alive) {
        // One-shot: re-armed after every change. Thread-agnostic, since the
        // coroutine resumes on the thread pool.
        LONG armed = RegNotifyChangeKeyValue(watch->key, FALSE, REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_THREAD_AGNOSTIC,
                                             watch->changed, TRUE);
        if (armed != ERROR_SUCCESS) co_return;
        co_await winrt::resume_on_signal(watch->changed);
        post([this, watch] {
            if (watch->alive) read();
        });
    }
}

winrt::fire_and_forget ThemeProvider::announce()
{
    co_await winrt::resume_background();
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"ImmersiveColorSet"),
                        SMTO_ABORTIFHUNG, 200, nullptr);
}

} // namespace cup
