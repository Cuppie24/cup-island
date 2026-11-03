#include "service/index_client.h"

#include "service/service_limits.h"
#include "service/service_win.h"

namespace cup::service {
namespace {

Handle openPipe()
{
    for (int attempt = 0; attempt < 2; ++attempt) {
        Handle pipe(CreateFileW(kPipeName, GENERIC_READ | FILE_WRITE_DATA, 0, nullptr, OPEN_EXISTING,
                                SECURITY_SQOS_PRESENT | SECURITY_IMPERSONATION, nullptr));
        if (pipe.valid() || GetLastError() != ERROR_PIPE_BUSY) return pipe;
        WaitNamedPipeW(kPipeName, 2000);
    }
    return Handle();
}

std::optional<std::string> exchange(HANDLE pipe, const std::string& request)
{
    std::string framed = indexer::frame(request);
    DWORD written = 0;
    if (!WriteFile(pipe, framed.data(), static_cast<DWORD>(framed.size()), &written, nullptr) || written != framed.size())
        return std::nullopt;
    indexer::FrameReader reader(indexer::kMaxResponseBytes);
    std::uint8_t buffer[4096];
    for (;;) {
        if (auto body = reader.next()) return body;
        DWORD read = 0;
        if (!ReadFile(pipe, buffer, sizeof buffer, &read, nullptr) || read == 0 || reader.broken()) return std::nullopt;
        reader.feed(std::span<const std::uint8_t>(buffer, read));
    }
}

} // namespace

ServiceState serviceState()
{
    ServiceState out;
    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!manager) return out;
    if (SC_HANDLE service = OpenServiceW(manager, kServiceName, SERVICE_QUERY_STATUS)) {
        out.installed = true;
        SERVICE_STATUS_PROCESS status{};
        DWORD bytes = 0;
        if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<BYTE*>(&status), sizeof status, &bytes) &&
            status.dwCurrentState == SERVICE_RUNNING)
            out.pid = status.dwProcessId;
        CloseServiceHandle(service);
    }
    CloseServiceHandle(manager);
    return out;
}

Asked askIndex(const std::string& request, bool requireService)
{
    Asked out;
    Handle pipe = openPipe();
    if (!pipe.valid()) {
        out.problem = AskProblem::NotRunning;
        return out;
    }
    ULONG server = 0;
    if (!GetNamedPipeServerProcessId(pipe.get(), &server)) server = 0;
    out.serverPid = server;
    ServiceState state = serviceState();
    if ((state.pid && server != state.pid) || (requireService && !state.pid)) {
        out.problem = AskProblem::NotTheService;
        return out;
    }
    auto body = exchange(pipe.get(), request);
    if (!body) {
        out.problem = AskProblem::NoAnswer;
        return out;
    }
    // Parsed for the shapes this build knows; a module's other answers are
    // the caller's to read from `raw`.
    out.response = indexer::parseResponse(*body);
    out.raw = std::move(*body);
    return out;
}

} // namespace cup::service
