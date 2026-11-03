#pragma once
// A client of the file index service's pipe (decisions 075, 076), shared by
// cupindex.exe and the app. Blocking: the app calls it off the UI thread.
// It opens the pipe with GENERIC_READ | FILE_WRITE_DATA (all a client is
// granted) and impersonation allowed (the service checks folders as the
// caller), and trusts the pipe only if the installed service serves it:
// anyone could have made a pipe with that name before the service did.

#include "indexer/protocol.h"

#include <optional>
#include <string>

namespace cup::service {

// Is the service installed, and running? Its process id when running.
struct ServiceState {
    bool installed = false;
    unsigned long pid = 0; // 0: not running
};
ServiceState serviceState();

enum class AskProblem { None, NotRunning, NotTheService, NoAnswer };

struct Asked {
    AskProblem problem = AskProblem::None;
    unsigned long serverPid = 0; // who served the pipe
    std::string raw;             // the answer's JSON-RPC body, as it came
    std::optional<indexer::Response> response; // ...parsed, when it's a shape indexer/protocol knows
};

// Sends one request, reads its answer. requireService: refuse a pipe not
// served by the installed service (false for cupindex with --console runs,
// which it reports instead).
Asked askIndex(const std::string& request, bool requireService);

} // namespace cup::service
