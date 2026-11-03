#pragma once
// The index service's named pipe (decision 075): \\.\pipe\CupIslandIndex,
// one thread per client, each request answered as that client.
// Who may connect:
//  - local only (PIPE_REJECT_REMOTE_CLIENTS, and NETWORK denied);
//  - interactive users may read and write data, but not create instances
//    of the pipe (FILE_CREATE_PIPE_INSTANCE is part of GENERIC_WRITE, so
//    they get FILE_WRITE_DATA alone): nobody else can pose as the service
//    while it runs;
//  - the first instance is made with FILE_FLAG_FIRST_PIPE_INSTANCE: if
//    another process took the name first, the service refuses to start.
// Clients open it with GENERIC_READ | FILE_WRITE_DATA, and should check the
// server is the service (GetNamedPipeServerProcessId), as cupindex does.

#include "service/volume_watcher.h"

#include <atomic>
#include <list>
#include <memory>
#include <thread>
#include <vector>

namespace cup::service {

class PipeServer {
public:
    PipeServer(const std::vector<std::unique_ptr<VolumeWatcher>>& volumes, HANDLE stopEvent);
    ~PipeServer(); // joins every client thread; the stop event must be set first

    // Accepts clients until the stop event is set. False if the pipe could
    // not be made (its name taken, or no memory).
    bool run();

private:
    struct Client {
        std::thread thread;
        std::atomic<bool> done{ false };
    };

    void serve(Handle pipe);
    void reap(); // joins clients that have finished

    const std::vector<std::unique_ptr<VolumeWatcher>>& volumes_;
    HANDLE stop_;
    std::list<std::unique_ptr<Client>> clients_;
};

} // namespace cup::service
