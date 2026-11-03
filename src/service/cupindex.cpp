// cupindex.exe: asks the file index service from a terminal (decision 075).
//   cupindex status
//   cupindex query report ext:pdf [--limit 20]
// Needs no admin: it's an ordinary client, the same as the island will be
// (milestone 5), and shows what such a client must do: open the pipe with
// GENERIC_READ | FILE_WRITE_DATA, and check the server is the service.

#include "indexer/protocol.h"
#include "service/service_limits.h"
#include "service/service_win.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace cup;

namespace {

// The service's process id, or 0 if it isn't installed or running.
DWORD servicePid()
{
    DWORD pid = 0;
    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!manager) return 0;
    if (SC_HANDLE service = OpenServiceW(manager, service::kServiceName, SERVICE_QUERY_STATUS)) {
        SERVICE_STATUS_PROCESS status{};
        DWORD bytes = 0;
        if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<BYTE*>(&status), sizeof status, &bytes) &&
            status.dwCurrentState == SERVICE_RUNNING)
            pid = status.dwProcessId;
        CloseServiceHandle(service);
    }
    CloseServiceHandle(manager);
    return pid;
}

service::Handle openPipe()
{
    for (int attempt = 0; attempt < 2; ++attempt) {
        // Impersonation level: the service checks folders as us. Asking for
        // less would only make it show nothing.
        service::Handle pipe(CreateFileW(service::kPipeName, GENERIC_READ | FILE_WRITE_DATA, 0, nullptr, OPEN_EXISTING,
                                         SECURITY_SQOS_PRESENT | SECURITY_IMPERSONATION, nullptr));
        if (pipe.valid() || GetLastError() != ERROR_PIPE_BUSY) return pipe;
        WaitNamedPipeW(service::kPipeName, 2000);
    }
    return service::Handle();
}

bool ask(HANDLE pipe, const std::string& request, indexer::Response& response)
{
    std::string framed = indexer::frame(request);
    DWORD written = 0;
    if (!WriteFile(pipe, framed.data(), static_cast<DWORD>(framed.size()), &written, nullptr) || written != framed.size()) return false;
    indexer::FrameReader reader(indexer::kMaxResponseBytes);
    std::uint8_t buffer[4096];
    for (;;) {
        if (auto body = reader.next()) {
            auto parsed = indexer::parseResponse(*body);
            if (!parsed) return false;
            response = *parsed;
            return true;
        }
        DWORD read = 0;
        if (!ReadFile(pipe, buffer, sizeof buffer, &read, nullptr) || read == 0 || reader.broken()) return false;
        reader.feed(std::span<const std::uint8_t>(buffer, read));
    }
}

int usage()
{
    std::puts("usage: cupindex status\n       cupindex query <words, ext:pdf, path:docs, file:, folder:> [--limit N]");
    return 1;
}

const char* stateName(indexer::VolumeState s)
{
    return s == indexer::VolumeState::Ready ? "ready" : s == indexer::VolumeState::Scanning ? "scanning" : "failed";
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2) return usage();
    std::wstring command = argv[1];
    std::string text;
    std::size_t limit = indexer::kDefaultResults;
    for (int i = 2; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--limit" && i + 1 < argc) {
            limit = static_cast<std::size_t>(std::wcstoul(argv[++i], nullptr, 10));
            continue;
        }
        if (!text.empty()) text += ' ';
        text += service::narrow(arg);
    }
    if (command != L"status" && (command != L"query" || text.empty() || limit == 0)) return usage();

    service::Handle pipe = openPipe();
    if (!pipe.valid()) {
        std::printf("The file index isn't running (error %lu).\n", GetLastError());
        return 2;
    }
    // Anyone could have made a pipe with this name before the service did;
    // trust it only if the running service is the one serving it.
    ULONG serverPid = 0;
    DWORD expected = servicePid();
    if (!GetNamedPipeServerProcessId(pipe.get(), &serverPid)) serverPid = 0;
    if (expected && serverPid != expected) {
        std::printf("Refusing: the pipe is served by process %lu, not the service (%lu).\n", serverPid, expected);
        return 3;
    }
    if (!expected) std::printf("(Not the installed service: process %lu, as with --console.)\n", serverPid);

    indexer::Response response;
    std::string request = command == L"status" ? indexer::encodeStatusRequest(1) : indexer::encodeQuery(1, text, limit);
    if (!ask(pipe.get(), request, response)) {
        std::puts("The service's answer couldn't be read.");
        return 2;
    }
    if (response.error) {
        std::printf("Error: %s\n", response.error->c_str());
        return 2;
    }
    if (response.volumes) {
        std::printf("protocol %d\n", response.protocol);
        for (const auto& v : *response.volumes) std::printf("%s  %-8s  %zu files\n", v.root.c_str(), stateName(v.state), v.files);
    }
    if (response.rows) {
        for (const auto& row : *response.rows) std::printf("%s%s\n", row.path.c_str(), row.folder ? "\\" : "");
        std::printf("%zu found\n", response.rows->size());
    }
    return 0;
}
