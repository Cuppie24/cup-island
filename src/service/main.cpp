// CupIslandIndex.exe: the file index service (decision 075).
//   started by the Service Control Manager: runs as the CupIslandIndex service
//   CupIslandIndex.exe --console: the same in a terminal, until Ctrl+C
//     (an administrator's terminal: reading drives needs it)
//   CupIslandIndex.exe --install | --stop | --uninstall: for its installer (decision 076)
// Wiring only: each drive gets a VolumeWatcher, clients get the PipeServer.

#include "service/pipe_server.h"
#include "service/self_update.h"
#include "service/service_control.h"
#include "service/service_limits.h"
#include "service/service_log.h"
#include "service/volume_watcher.h"

#include "cup_version.h"

#include <cstdio>
#include <memory>
#include <vector>

namespace cup::service {
namespace {

HANDLE g_stop = nullptr; // set to stop everything: SCM stop, shutdown, Ctrl+C
SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
SERVICE_STATUS g_status{};

void report(DWORD state, DWORD exitCode = NO_ERROR, DWORD waitHintMs = 0)
{
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwCurrentState = state;
    g_status.dwWin32ExitCode = exitCode;
    g_status.dwWaitHint = waitHintMs;
    g_status.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
    if (state == SERVICE_RUNNING || state == SERVICE_STOPPED) g_status.dwCheckPoint = 0;
    else ++g_status.dwCheckPoint;
    if (g_statusHandle) SetServiceStatus(g_statusHandle, &g_status);
}

// Runs until g_stop is set. False if the service couldn't start at all.
bool runIndex(bool console)
{
    std::filesystem::path folder = dataFolder();
    startServiceLog(folder, console);
    serviceLog("starting %s", CUP_VERSION_STRING);
    SelfUpdate update(folder, CUP_VERSION_STRING);
    update.removeLeftovers(); // an update that finished replaced us; one that didn't, left its installer
    std::vector<std::unique_ptr<VolumeWatcher>> volumes;
    for (VolumeInfo& volume : fixedNtfsVolumes()) {
        serviceLog("%s: indexing", volume.root.c_str());
        volumes.push_back(std::make_unique<VolumeWatcher>(std::move(volume), folder, g_stop));
    }
    if (volumes.empty()) serviceLog("no fixed NTFS drives found");
    for (auto& volume : volumes) volume->start();
    if (!console) report(SERVICE_RUNNING);
    bool ok;
    {
        PipeServer server(volumes, update, g_stop);
        ok = server.run();
        SetEvent(g_stop); // the pipe failing stops the drives too
    }
    volumes.clear(); // joins each watcher, which saves its index
    serviceLog(ok ? "stopped" : "stopped: the pipe couldn't be served");
    return ok;
}

DWORD WINAPI control(DWORD code, DWORD, void*, void*)
{
    switch (code) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        report(SERVICE_STOP_PENDING, NO_ERROR, 10000); // saving each drive's index takes well under that
        SetEvent(g_stop);
        return NO_ERROR;
    case SERVICE_CONTROL_INTERROGATE: return NO_ERROR;
    default: return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

void WINAPI serviceMain(DWORD, wchar_t**)
{
    g_statusHandle = RegisterServiceCtrlHandlerExW(kServiceName, control, nullptr);
    if (!g_statusHandle) return;
    report(SERVICE_START_PENDING, NO_ERROR, 5000);
    bool ok = runIndex(false);
    report(SERVICE_STOPPED, ok ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR);
}

BOOL WINAPI consoleControl(DWORD)
{
    SetEvent(g_stop);
    return TRUE;
}

} // namespace
} // namespace cup::service

int wmain(int argc, wchar_t** argv)
{
    using namespace cup::service;
    g_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stop) return 1;
    std::wstring_view command = argc > 1 ? std::wstring_view(argv[1]) : std::wstring_view();
    if (command == L"--install") return installService();
    if (command == L"--stop") return stopService();
    if (command == L"--uninstall") return uninstallService();
    if (command == L"--console") {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCtrlHandler(consoleControl, TRUE);
        std::puts("Cup Island file index: Ctrl+C stops it.");
        return runIndex(true) ? 0 : 1;
    }
    std::wstring name = kServiceName; // the table wants a writable string
    SERVICE_TABLE_ENTRYW table[] = { { name.data(), serviceMain }, { nullptr, nullptr } };
    if (!StartServiceCtrlDispatcherW(table)) {
        if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
            std::puts("This is the Cup Island file index service. To try it in a terminal, run it as administrator with --console.");
        return 1;
    }
    return 0;
}
