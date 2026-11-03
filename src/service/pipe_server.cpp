#include "service/pipe_server.h"

#include "indexer/protocol.h"
#include "service/answers.h"
#include "service/service_limits.h"
#include "service/service_log.h"

#include <sddl.h>

#include <algorithm>

namespace cup::service {
namespace {

// NETWORK denied; SYSTEM and Administrators everything; interactive users
// FILE_GENERIC_READ | FILE_WRITE_DATA (0x12008B), so no pipe instances.
constexpr wchar_t kPipeSecurity[] = L"D:P(D;;GA;;;NU)(A;;GA;;;SY)(A;;GA;;;BA)(A;;0x12008B;;;IU)";
constexpr DWORD kPipeBufferBytes = 64 * 1024;

// One overlapped read or write that a stop request can interrupt.
bool pipeIo(HANDLE pipe, HANDLE stop, bool write, void* data, DWORD size, DWORD& bytes)
{
    Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event.valid()) return false;
    OVERLAPPED ov{};
    ov.hEvent = event.get();
    BOOL ok = write ? WriteFile(pipe, data, size, nullptr, &ov) : ReadFile(pipe, data, size, nullptr, &ov);
    if (!ok && GetLastError() != ERROR_IO_PENDING) return false;
    HANDLE events[2] = { ov.hEvent, stop };
    if (WaitForMultipleObjects(2, events, FALSE, INFINITE) != WAIT_OBJECT_0) {
        CancelIoEx(pipe, &ov);
        GetOverlappedResult(pipe, &ov, &bytes, TRUE);
        return false;
    }
    return GetOverlappedResult(pipe, &ov, &bytes, FALSE) != FALSE;
}

bool writeAll(HANDLE pipe, HANDLE stop, const std::string& data)
{
    std::size_t at = 0;
    while (at < data.size()) {
        DWORD bytes = 0;
        auto chunk = static_cast<DWORD>(std::min<std::size_t>(data.size() - at, kPipeBufferBytes));
        if (!pipeIo(pipe, stop, true, const_cast<char*>(data.data() + at), chunk, bytes) || bytes == 0) return false;
        at += bytes;
    }
    return true;
}

// Acts as the client for its lifetime. Failing to stop acting as the client
// would leave this SYSTEM thread with someone else's rights, or worse,
// confused about whose it has: the process ends at once instead.
class Impersonation {
public:
    explicit Impersonation(HANDLE pipe) : active_(ImpersonateNamedPipeClient(pipe) != FALSE) {}
    ~Impersonation()
    {
        if (active_ && !RevertToSelf()) {
            serviceLog("could not stop acting as a client: exiting");
            TerminateProcess(GetCurrentProcess(), 1);
        }
    }
    Impersonation(const Impersonation&) = delete;
    Impersonation& operator=(const Impersonation&) = delete;
    bool active() const { return active_; }

private:
    bool active_;
};

} // namespace

PipeServer::PipeServer(const std::vector<std::unique_ptr<VolumeWatcher>>& volumes, HANDLE stopEvent)
    : volumes_(volumes), stop_(stopEvent)
{
}

PipeServer::~PipeServer()
{
    for (auto& client : clients_)
        if (client->thread.joinable()) client->thread.join();
}

bool PipeServer::run()
{
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(kPipeSecurity, SDDL_REVISION_1, &descriptor, nullptr)) {
        serviceLog("pipe: bad security descriptor (error %lu)", GetLastError());
        return false;
    }
    SECURITY_ATTRIBUTES security{ sizeof security, descriptor, FALSE };
    bool first = true, ok = true;
    Handle connected(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    while (connected.valid() && WaitForSingleObject(stop_, 0) != WAIT_OBJECT_0) {
        reap();
        DWORD openMode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | (first ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0);
        Handle pipe(CreateNamedPipeW(kPipeName, openMode, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                     PIPE_UNLIMITED_INSTANCES, kPipeBufferBytes, kPipeBufferBytes, 0, &security));
        if (!pipe.valid()) {
            DWORD error = GetLastError();
            if (first) {
                serviceLog("pipe: can't create it (error %lu)%s", error,
                           error == ERROR_ACCESS_DENIED ? ": another process holds the name" : "");
                ok = false;
                break;
            }
            serviceLog("pipe: can't create another instance (error %lu)", error);
            if (WaitForSingleObject(stop_, 1000) == WAIT_OBJECT_0) break;
            continue;
        }
        first = false;

        OVERLAPPED ov{};
        ov.hEvent = connected.get();
        ResetEvent(ov.hEvent);
        bool isConnected = ConnectNamedPipe(pipe.get(), &ov) != FALSE;
        if (!isConnected) {
            DWORD error = GetLastError();
            if (error == ERROR_PIPE_CONNECTED) {
                isConnected = true;
            } else if (error == ERROR_IO_PENDING) {
                HANDLE events[2] = { ov.hEvent, stop_ };
                DWORD bytes = 0;
                if (WaitForMultipleObjects(2, events, FALSE, INFINITE) != WAIT_OBJECT_0) {
                    CancelIoEx(pipe.get(), &ov);
                    GetOverlappedResult(pipe.get(), &ov, &bytes, TRUE);
                    break;
                }
                isConnected = GetOverlappedResult(pipe.get(), &ov, &bytes, FALSE) != FALSE;
            }
        }
        if (!isConnected) continue;
        if (static_cast<int>(clients_.size()) >= kMaxClients) {
            serviceLog("pipe: %d clients already, refusing another", kMaxClients);
            DisconnectNamedPipe(pipe.get());
            continue;
        }
        auto client = std::make_unique<Client>();
        Client* c = client.get();
        HANDLE raw = pipe.release(); // the thread owns it from here
        clients_.push_back(std::move(client));
        c->thread = std::thread([this, c, raw] {
            serve(Handle(raw));
            c->done = true;
        });
    }
    LocalFree(descriptor);
    return ok;
}

void PipeServer::serve(Handle pipe)
{
    indexer::FolderAccess access(canListFolder);
    indexer::FrameReader reader(indexer::kMaxRequestBytes);
    std::vector<std::uint8_t> buffer(4096);
    for (;;) {
        DWORD bytes = 0;
        if (!pipeIo(pipe.get(), stop_, false, buffer.data(), static_cast<DWORD>(buffer.size()), bytes)) break; // gone, or stopping
        reader.feed(std::span<const std::uint8_t>(buffer.data(), bytes));
        if (reader.broken()) {
            serviceLog("pipe: a client sent a message over the size limit; closing it");
            break;
        }
        bool written = true;
        while (auto body = reader.next()) {
            std::string reply;
            {
                Impersonation as(pipe.get());
                reply = as.active() ? answer(*body, volumes_, access)
                                    : indexer::encodeError(std::nullopt, indexer::kInternalError, "can't act as the client");
            }
            if (!writeAll(pipe.get(), stop_, indexer::frame(reply))) {
                written = false;
                break;
            }
        }
        if (!written) break;
    }
    DisconnectNamedPipe(pipe.get());
}

void PipeServer::reap()
{
    for (auto it = clients_.begin(); it != clients_.end();) {
        if ((*it)->done) {
            (*it)->thread.join();
            it = clients_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace cup::service
