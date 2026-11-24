#include "providers/media/app_volume.h"

#include "platform/log.h"
#include "providers/media/app_activation.h"

#include <appmodel.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>

#include <algorithm>
#include <cwctype>
#include <iterator>
#include <utility>
#include <vector>

namespace cup {
namespace {

std::wstring lowered(std::wstring s)
{
    for (auto& c : s) c = static_cast<wchar_t>(std::towlower(c));
    return s;
}

// Which processes are the app: by exe name (desktop apps), or by the
// package's app id (Store apps, whose session id is "Package_hash!App").
struct AppProcesses {
    std::wstring appId;              // lower case
    std::vector<std::wstring> stems; // exe names it may run as; none for a Store app

    bool has(DWORD pid) const
    {
        if (pid == 0) return false; // the system's sounds
        std::wstring stem = exeStemOfProcess(pid);
        if (!stem.empty() && std::find(stems.begin(), stems.end(), stem) != stems.end()) return true;
        bool same = false;
        if (HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
            wchar_t id[APPLICATION_USER_MODEL_ID_MAX_LENGTH];
            UINT32 length = static_cast<UINT32>(std::size(id));
            same = GetApplicationUserModelId(process, &length, id) == ERROR_SUCCESS && lowered(id) == appId;
            CloseHandle(process);
        }
        return same;
    }
};

// The app's audio sessions on every output in use: a browser has one per
// device it plays on. Sessions that ended (expired) are left out.
std::vector<winrt::com_ptr<ISimpleAudioVolume>> sessionsOf(const std::wstring& appId)
{
    std::vector<winrt::com_ptr<ISimpleAudioVolume>> found;
    AppProcesses app{ lowered(appId), appExeStems(appId) };
    winrt::com_ptr<IMMDeviceEnumerator> devices;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(devices.put())))) return found;
    winrt::com_ptr<IMMDeviceCollection> outputs;
    UINT outputCount = 0;
    if (FAILED(devices->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, outputs.put())) || FAILED(outputs->GetCount(&outputCount))) return found;
    for (UINT d = 0; d < outputCount; ++d) {
        winrt::com_ptr<IMMDevice> device;
        winrt::com_ptr<IAudioSessionManager2> manager;
        winrt::com_ptr<IAudioSessionEnumerator> sessions;
        int count = 0;
        if (FAILED(outputs->Item(d, device.put()))) continue;
        if (FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, manager.put_void()))) continue;
        if (FAILED(manager->GetSessionEnumerator(sessions.put())) || FAILED(sessions->GetCount(&count))) continue;
        for (int i = 0; i < count; ++i) {
            winrt::com_ptr<IAudioSessionControl> control;
            winrt::com_ptr<IAudioSessionControl2> process;
            winrt::com_ptr<ISimpleAudioVolume> volume;
            AudioSessionState state = AudioSessionStateExpired;
            DWORD pid = 0;
            if (FAILED(sessions->GetSession(i, control.put())) || FAILED(control->GetState(&state)) || state == AudioSessionStateExpired) continue;
            if (FAILED(control->QueryInterface(IID_PPV_ARGS(process.put()))) || FAILED(process->GetProcessId(&pid)) || !app.has(pid)) continue;
            if (SUCCEEDED(control->QueryInterface(IID_PPV_ARGS(volume.put())))) found.push_back(std::move(volume));
        }
    }
    return found;
}

std::optional<AppVolume::Level> levelOf(const std::wstring& appId)
{
    for (const auto& session : sessionsOf(appId)) {
        float volume = 1;
        BOOL muted = FALSE;
        if (SUCCEEDED(session->GetMasterVolume(&volume)) && SUCCEEDED(session->GetMute(&muted)))
            return AppVolume::Level{ std::clamp(volume, 0.0f, 1.0f), muted != FALSE };
    }
    return std::nullopt;
}

} // namespace

std::shared_ptr<AppVolume> AppVolume::start(Report report)
{
    std::shared_ptr<AppVolume> volume(new AppVolume(std::move(report)));
    volume->controller_ = winrt::Windows::System::DispatcherQueueController::CreateOnDedicatedThread(); // Windows 10 1709+
    volume->queue_ = volume->controller_.DispatcherQueue();
    volume->post([] {
        try { // normally there already: then this is a no-op
            winrt::init_apartment(winrt::apartment_type::single_threaded);
        } catch (winrt::hresult_error const&) {
        }
    });
    return volume;
}

void AppVolume::stop()
{
    stopped_ = true;
    controller_.ShutdownQueueAsync(); // the thread ends once its queue has drained; a call stuck in Windows parks it
}

void AppVolume::post(std::function<void()> work)
{
    queue_.TryEnqueue([self = shared_from_this(), work = std::move(work)] {
        if (self->stopped_) return;
        try { // an exception out of a queued task would end the app
            work();
        } catch (winrt::hresult_error const& e) {
            platform::logLine("media: volume call failed (0x%08x)", static_cast<unsigned>(e.code()));
        } catch (std::exception const& e) {
            platform::logLine("media: volume call failed (%s)", e.what());
        }
    });
}

void AppVolume::read(const std::string& app)
{
    post([this, app] { report_(app, levelOf(std::wstring(winrt::to_hstring(app)))); });
}

void AppVolume::set(const std::string& app, float volume)
{
    std::uint64_t mine = ++latestSet_;
    post([this, app, volume, mine] {
        if (mine != latestSet_) return; // a newer one is queued behind this
        for (const auto& session : sessionsOf(std::wstring(winrt::to_hstring(app)))) {
            session->SetMasterVolume(std::clamp(volume, 0.0f, 1.0f), nullptr);
            session->SetMute(FALSE, nullptr); // setting a level is asking to hear it
        }
    });
}

void AppVolume::setMuted(const std::string& app, bool muted)
{
    post([app, muted] {
        for (const auto& session : sessionsOf(std::wstring(winrt::to_hstring(app)))) session->SetMute(muted ? TRUE : FALSE, nullptr);
    });
}

} // namespace cup
