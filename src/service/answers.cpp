#include "service/answers.h"

#include "indexer/protocol.h"
#include "indexer/query.h"
#include "service/service_limits.h"

#include "cup_version.h"

#include <algorithm>
#include <limits>

namespace cup::service {

using namespace cup::indexer;

namespace {

std::string answerQuery(const QueryRequest& request, const std::vector<std::unique_ptr<VolumeWatcher>>& volumes,
                        FolderAccess& access)
{
    Query query = parseQuery(request.text);
    if (query.empty()) return encodeResults(request.id, {});
    // Ask each drive for more than wanted: some will be hidden from this user.
    std::size_t candidates = std::min(request.limit * kCandidateFactor, kMaxCandidates);
    std::vector<std::vector<Found>> found;
    std::vector<std::vector<Hit>> hits;
    for (const auto& volume : volumes) {
        found.push_back(volume->find(query, candidates));
        hits.emplace_back();
        for (const Found& f : found.back()) hits.back().push_back(f.hit);
    }
    auto now = static_cast<std::int64_t>(GetTickCount64());
    std::vector<ResultRow> rows;
    for (const Pick& pick : mergeHits(hits, std::numeric_limits<std::size_t>::max())) {
        const Found& f = found[pick.list][pick.index];
        if (!access.visible(pick.list, f.folder, f.folderPath, now)) continue;
        rows.push_back({ f.name, f.path, f.hit.folder, f.hidden });
        if (rows.size() == request.limit) break;
    }
    return encodeResults(request.id, rows);
}

} // namespace

std::string answer(std::string_view body, const std::vector<std::unique_ptr<VolumeWatcher>>& volumes, FolderAccess& access,
                   SelfUpdate& update)
{
    Request request = parseRequest(body);
    if (const auto* bad = std::get_if<BadRequest>(&request)) return encodeError(bad->id, bad->code, bad->message);
    if (const auto* status = std::get_if<StatusRequest>(&request)) {
        std::vector<VolumeStatus> list;
        for (const auto& volume : volumes) list.push_back(volume->status());
        return encodeStatus(status->id, CUP_VERSION_STRING, list);
    }
    if (const auto* asked = std::get_if<UpdateRequest>(&request)) return encodeUpdate(asked->id, update.request());
    return answerQuery(std::get<QueryRequest>(request), volumes, access);
}

bool canListFolder(const std::string& folderPath)
{
    // \\?\ lifts MAX_PATH; BACKUP_SEMANTICS is what lets CreateFile open a
    // folder at all (it grants nothing unless the backup privilege is
    // enabled in the token, which an ordinary user's isn't).
    std::wstring path = L"\\\\?\\" + widen(folderPath);
    Handle folder(CreateFileW(path.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    return folder.valid();
}

} // namespace cup::service
