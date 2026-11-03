// cupindex.exe: asks the file index service from a terminal (decision 075).
//   cupindex status
//   cupindex query report ext:pdf [--limit 20]
//   cupindex update   (the service installs GitHub's latest release of itself, decision 076)
// Needs no admin: it's an ordinary client, the same as the island will be
// (milestone 5), and shows what such a client must do: open the pipe with
// GENERIC_READ | FILE_WRITE_DATA, and check the server is the service.

#include "indexer/protocol.h"
#include "service/index_client.h"
#include "service/service_win.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace cup;

namespace {

int usage()
{
    std::puts("usage: cupindex status\n       cupindex query <words, ext:pdf, path:docs, file:, folder:> [--limit N]\n       cupindex update");
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
    if (command != L"status" && command != L"update" && (command != L"query" || text.empty() || limit == 0)) return usage();

    std::string request = command == L"status"   ? indexer::encodeStatusRequest(1)
                          : command == L"update" ? indexer::encodeUpdateRequest(1)
                                                 : indexer::encodeQuery(1, text, limit);
    // Not requireService: a --console run has no installed service to match,
    // which is said, not refused. A pipe served by someone other than a
    // running service is refused either way (index_client).
    service::Asked asked = service::askIndex(request, false);
    switch (asked.problem) {
    case service::AskProblem::NotRunning: std::puts("The file index isn't running."); return 2;
    case service::AskProblem::NotTheService:
        std::printf("Refusing: the pipe is served by process %lu, not the service.\n", asked.serverPid);
        return 3;
    case service::AskProblem::NoAnswer: std::puts("The service's answer couldn't be read."); return 2;
    case service::AskProblem::None: break;
    }
    if (!service::serviceState().pid) std::printf("(Not the installed service: process %lu, as with --console.)\n", asked.serverPid);
    const indexer::Response& response = *asked.response;
    if (response.error) {
        std::printf("Error: %s\n", response.error->c_str());
        return 2;
    }
    if (response.volumes) {
        std::printf("version %s, protocol %d\n", response.version.c_str(), response.protocol);
        for (const auto& v : *response.volumes) std::printf("%s  %-8s  %zu files\n", v.root.c_str(), stateName(v.state), v.files);
    }
    if (response.update) {
        const char* said[] = { "Updating: the service will restart as the new version.", "Already the latest release.",
                               "An update was tried a few minutes ago; try later.", "A dev build: it never updates.",
                               "The update failed: see the service's log." };
        std::puts(said[static_cast<int>(*response.update)]);
    }
    if (response.rows) {
        for (const auto& row : *response.rows) std::printf("%s%s\n", row.path.c_str(), row.folder ? "\\" : "");
        std::printf("%zu found\n", response.rows->size());
    }
    return 0;
}
