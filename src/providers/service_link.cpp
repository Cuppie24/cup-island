#include "providers/service_link.h"

#include "indexer/protocol.h"
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

ServiceReply replyOf(const service::Asked& asked, const std::string& body)
{
    switch (asked.problem) {
    case service::AskProblem::None: return { ServiceReply::Problem::None, body };
    case service::AskProblem::NotRunning: return { ServiceReply::Problem::NotRunning, {} };
    // A pipe someone else serves is no service of ours: treated as none running.
    case service::AskProblem::NotTheService: return { ServiceReply::Problem::NotRunning, {} };
    case service::AskProblem::NoAnswer: return { ServiceReply::Problem::NoAnswer, {} };
    }
    return { ServiceReply::Problem::NoAnswer, {} };
}

winrt::fire_and_forget probe(std::function<void(ServiceProbe)> done)
{
    winrt::apartment_context ui;
    co_await winrt::resume_background();
    ServiceProbe found;
    if (!service::serviceState().installed) {
        found.state = ServiceProbe::State::Missing;
    } else {
        service::Asked asked = service::askIndex(indexer::encodeStatusRequest(0), true);
        if (asked.problem == service::AskProblem::None && asked.response && asked.response->volumes) {
            found.state = ServiceProbe::State::Running;
            found.version = asked.response->version;
        } else {
            found.state = ServiceProbe::State::Stopped;
        }
    }
    platform::logLine("service: %s%s", found.state == ServiceProbe::State::Missing ? "not installed"
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

winrt::fire_and_forget send(std::string request, std::function<void(ServiceReply)> done)
{
    winrt::apartment_context ui;
    co_await winrt::resume_background();
    service::Asked asked = service::askIndex(request, true);
    std::string body;
    if (asked.problem == service::AskProblem::None) body = asked.raw;
    ServiceReply reply = replyOf(asked, body);
    try {
        co_await ui;
    } catch (...) {
        co_return;
    }
    done(std::move(reply));
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
    if (started) platform::logLine("service: setting up file search (%ls)", installer.c_str());
    else platform::logLine("service: setup didn't start (%lu%s)", error, error == ERROR_CANCELLED ? ": permission refused" : "");
    try {
        co_await ui;
    } catch (...) {
        co_return;
    }
    done(started);
}

} // namespace

void probeService(std::function<void(ServiceProbe)> done)
{
    probe(std::move(done));
}

void sendToService(std::string request, std::function<void(ServiceReply)> done)
{
    send(std::move(request), std::move(done));
}

bool serviceInstallerBundled()
{
    std::error_code ec;
    return std::filesystem::is_regular_file(bundledInstaller(), ec);
}

void runServiceInstaller(std::function<void(bool started)> done)
{
    run(std::move(done));
}

} // namespace cup
