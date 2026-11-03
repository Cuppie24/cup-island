#include "service/service_control.h"

#include "service/service_limits.h"
#include "service/service_win.h"

#include <cstdio>
#include <string>

namespace cup::service {
namespace {

// Closes a Service Control Manager handle (a different type from HANDLE).
class ScHandle {
public:
    explicit ScHandle(SC_HANDLE h) : h_(h) {}
    ~ScHandle()
    {
        if (h_) CloseServiceHandle(h_);
    }
    ScHandle(const ScHandle&) = delete;
    ScHandle& operator=(const ScHandle&) = delete;
    SC_HANDLE get() const { return h_; }
    explicit operator bool() const { return h_ != nullptr; }

private:
    SC_HANDLE h_;
};

int fail(const char* what)
{
    DWORD error = GetLastError();
    std::printf("%s failed (error %lu)\n", what, error);
    return error ? static_cast<int>(error) : 1;
}

// Asks the service to stop and waits for its process to exit: SCM's
// "stopped" comes a moment before the exe's file is free to replace.
int stopAndWait(SC_HANDLE service)
{
    SERVICE_STATUS_PROCESS status{};
    DWORD bytes = 0;
    if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<BYTE*>(&status), sizeof status, &bytes))
        return fail("Reading the service's state");
    if (status.dwCurrentState == SERVICE_STOPPED) return 0;
    Handle process(OpenProcess(SYNCHRONIZE, FALSE, status.dwProcessId));
    SERVICE_STATUS ignored{};
    if (status.dwCurrentState != SERVICE_STOP_PENDING && !ControlService(service, SERVICE_CONTROL_STOP, &ignored) &&
        GetLastError() != ERROR_SERVICE_NOT_ACTIVE)
        return fail("Stopping the service");
    // An install-time command, not the island: waiting in steps is fine here.
    ULONGLONG deadline = GetTickCount64() + kStopWaitMs;
    while (GetTickCount64() < deadline) {
        if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<BYTE*>(&status), sizeof status, &bytes))
            return fail("Reading the service's state");
        if (status.dwCurrentState == SERVICE_STOPPED) {
            if (process.valid()) WaitForSingleObject(process.get(), kStopWaitMs);
            std::puts("Stopped.");
            return 0;
        }
        Sleep(200);
    }
    std::puts("The service didn't stop in time.");
    return static_cast<int>(ERROR_SERVICE_REQUEST_TIMEOUT);
}

} // namespace

int installService()
{
    wchar_t exe[MAX_PATH * 2] = {};
    DWORD length = GetModuleFileNameW(nullptr, exe, MAX_PATH * 2);
    if (length == 0 || length >= MAX_PATH * 2) return fail("Finding this program's path");
    std::wstring command = L"\"" + std::wstring(exe) + L"\""; // quoted: see the header

    ScHandle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
    if (!manager) return fail("Opening the Service Control Manager");
    SC_HANDLE existing = OpenServiceW(manager.get(), kServiceName, SERVICE_ALL_ACCESS);
    // No account: LocalSystem, the only one that can open volumes (decision 075).
    ScHandle service(existing ? existing
                              : CreateServiceW(manager.get(), kServiceName, kServiceDisplayName, SERVICE_ALL_ACCESS,
                                               SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
                                               command.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr));
    if (!service) return fail("Creating the service");
    // An existing one gets this exe's path and settings: an update may have moved it.
    if (existing && !ChangeServiceConfigW(service.get(), SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
                                          command.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr, kServiceDisplayName))
        return fail("Updating the service");

    std::wstring text = kServiceDescription;
    SERVICE_DESCRIPTIONW description{ text.data() };
    ChangeServiceConfig2W(service.get(), SERVICE_CONFIG_DESCRIPTION, &description);
    // Delayed: it starts a little after sign-in's rush, not during it.
    SERVICE_DELAYED_AUTO_START_INFO delayed{ TRUE };
    ChangeServiceConfig2W(service.get(), SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &delayed);
    // Restarted 5 s, 30 s and 2 min after a crash; the count resets after a day.
    SC_ACTION actions[3] = { { SC_ACTION_RESTART, 5000 }, { SC_ACTION_RESTART, 30000 }, { SC_ACTION_RESTART, 120000 } };
    SERVICE_FAILURE_ACTIONSW failure{ 24 * 60 * 60, nullptr, nullptr, 3, actions };
    if (!ChangeServiceConfig2W(service.get(), SERVICE_CONFIG_FAILURE_ACTIONS, &failure)) return fail("Setting crash recovery");
    // Also when it stops with an error of its own (its pipe couldn't be served), not only a crash.
    SERVICE_FAILURE_ACTIONS_FLAG onErrors{ TRUE };
    ChangeServiceConfig2W(service.get(), SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &onErrors);

    if (!StartServiceW(service.get(), 0, nullptr) && GetLastError() != ERROR_SERVICE_ALREADY_RUNNING)
        return fail("Starting the service");
    std::puts("Installed and started.");
    return 0;
}

int stopService()
{
    ScHandle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!manager) return fail("Opening the Service Control Manager");
    ScHandle service(OpenServiceW(manager.get(), kServiceName, SERVICE_STOP | SERVICE_QUERY_STATUS));
    if (!service) return GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST ? 0 : fail("Opening the service");
    return stopAndWait(service.get());
}

int uninstallService()
{
    ScHandle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!manager) return fail("Opening the Service Control Manager");
    ScHandle service(OpenServiceW(manager.get(), kServiceName, SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE));
    if (!service) return GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST ? 0 : fail("Opening the service");
    if (int error = stopAndWait(service.get())) return error;
    if (!DeleteService(service.get()) && GetLastError() != ERROR_SERVICE_MARKED_FOR_DELETE) return fail("Removing the service");
    std::puts("Removed.");
    return 0;
}

} // namespace cup::service
