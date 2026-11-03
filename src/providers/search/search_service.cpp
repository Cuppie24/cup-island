#include "providers/search/search_service.h"

#include "platform/log.h"
#include "platform/win.h"
#include "service/index_client.h"

#include <shellapi.h>

#include <filesystem>

namespace cup {
namespace {

std::filesystem::path bundledInstaller()
{
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    return std::filesystem::path(exe).parent_path() / L"CupIsland-Index-Setup.exe";
}

winrt::fire_and_forget probe(std::function<void(ServiceProbe)> done)
{
    winrt::apartment_context ui;
    co_await winrt::resume_background();
    ServiceProbe found;
    service::ServiceState state = service::serviceState();
    if (!state.installed) {
        found.state = ServiceProbe::State::Missing;
    } else {
        // requireService: only the installed service's answer counts.
        service::Asked asked = service::askIndex(indexer::encodeStatusRequest(1), true);
        if (asked.problem == service::AskProblem::None && asked.response->volumes) {
            found.state = ServiceProbe::State::Running;
            found.version = asked.response->version;
        } else {
            found.state = ServiceProbe::State::Stopped;
        }
    }
    platform::logLine("file search: %s%s", found.state == ServiceProbe::State::Missing ? "not installed"
                                           : found.state == ServiceProbe::State::Stopped ? "installed, not answering"
                                                                                         : "running ",
                      found.version.c_str());
    try {
        co_await ui;
    } catch (...) {
        co_return; // closing: nobody to tell
    }
    done(found);
}

winrt::fire_and_forget update(std::function<void(std::optional<indexer::UpdateAnswer>)> done)
{
    winrt::apartment_context ui;
    co_await winrt::resume_background();
    service::Asked asked = service::askIndex(indexer::encodeUpdateRequest(1), true);
    std::optional<indexer::UpdateAnswer> answer;
    if (asked.problem == service::AskProblem::None && asked.response->update) answer = asked.response->update;
    const char* said[] = { "started", "already current", "busy", "a dev build", "failed" };
    platform::logLine("file search: asked it to update: %s", answer ? said[static_cast<int>(*answer)] : "no answer");
    try {
        co_await ui;
    } catch (...) {
        co_return;
    }
    done(answer);
}

winrt::fire_and_forget run(std::function<void(bool)> done)
{
    winrt::apartment_context ui;
    co_await winrt::resume_background(); // ShellExecuteEx waits for the permission prompt's answer
    std::filesystem::path installer = bundledInstaller();
    std::wstring parameters = L"/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /LOG=\"" +
                              (platform::logFolder() / L"index-setup.log").wstring() + L"\"";
    SHELLEXECUTEINFOW info{ sizeof info };
    info.fMask = SEE_MASK_NOASYNC;
    info.lpVerb = L"runas"; // Windows' permission prompt: the installer needs admin (decision 076)
    info.lpFile = installer.c_str();
    info.lpParameters = parameters.c_str();
    info.nShow = SW_SHOWNORMAL;
    bool started = ShellExecuteExW(&info) != FALSE;
    DWORD error = started ? 0 : GetLastError();
    if (started) platform::logLine("file search: setting it up (%ls)", installer.c_str());
    else platform::logLine("file search: setup didn't start (%lu%s)", error, error == ERROR_CANCELLED ? ": permission refused" : "");
    try {
        co_await ui;
    } catch (...) {
        co_return;
    }
    done(started);
}

} // namespace

void probeSearchService(std::function<void(ServiceProbe)> done)
{
    probe(std::move(done));
}

void askSearchServiceUpdate(std::function<void(std::optional<indexer::UpdateAnswer>)> done)
{
    update(std::move(done));
}

bool searchInstallerBundled()
{
    std::error_code ec;
    return std::filesystem::is_regular_file(bundledInstaller(), ec);
}

void runSearchInstaller(std::function<void(bool started)> done)
{
    run(std::move(done));
}

} // namespace cup
