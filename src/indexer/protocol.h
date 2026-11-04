#pragma once
// What goes over the index service's pipe (decision 075): JSON-RPC 2.0, the
// shape PS §8 plans for plugins, each message prefixed with its length
// (4 bytes, little-endian). Methods are named by module (decision 078):
//   index.query    {"text": "report ext:pdf", "limit": 50} -> {"hits": [{name, path, folder, hidden}]}
//   index.status   {}  -> {"protocol": 1, "version": "0.8.0", "volumes": [{root, state, files}]}
//   service.update {"installer": "C:\\...\\CupIsland-Index-Setup.exe"}
//                  -> {"update": "started" | "current" | "busy" | "dev" | "failed"}
// `service.update` names no version: the service installs GitHub's latest
// release of itself, or nothing (decision 076). `installer`, optional, is
// where the app's own copy of the service installer is: the service runs it
// only if its SHA-256 is the one the release's notes give (decision 081).
// No cancel: a search takes tens of milliseconds, and a client drops an
// answer whose id it no longer wants. Both sides are here, so the service,
// the cupindex tool and the island share one tested encoding.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cup::indexer {

// Bumped when a message changes shape. The island speaks this one and the
// one before it (decision 075: the service updates separately, with UAC).
constexpr int kProtocolVersion = 1;
// A request is a query someone typed: 16 KB is plenty. An answer is at most
// kMaxResults rows with paths up to ~32 K characters each, kept under 8 MB.
constexpr std::size_t kMaxRequestBytes = 16 * 1024;
constexpr std::size_t kMaxResponseBytes = 8 * 1024 * 1024;
constexpr std::size_t kDefaultResults = 50;
constexpr std::size_t kMaxResults = 200;

// JSON-RPC 2.0 error codes.
constexpr int kParseError = -32700;
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kInvalidParams = -32602;
constexpr int kInternalError = -32603;

// --- framing -------------------------------------------------------------
std::string frame(std::string_view body); // the length, then the body

// Collects bytes as they arrive and hands out whole messages. A length past
// `maxBytes` breaks it for good: the connection is then closed, since the
// stream can't be trusted to find the next message.
class FrameReader {
public:
    explicit FrameReader(std::size_t maxBytes) : max_(maxBytes) {}
    void feed(std::span<const std::uint8_t> bytes);
    std::optional<std::string> next();
    bool broken() const { return broken_; }

private:
    std::string pending_;
    std::size_t max_;
    bool broken_ = false;
};

// --- requests ------------------------------------------------------------
struct QueryRequest {
    std::int64_t id = 0;
    std::string text;
    std::size_t limit = kDefaultResults; // 1..kMaxResults
};
struct StatusRequest {
    std::int64_t id = 0;
};
struct UpdateRequest {
    std::int64_t id = 0;
    std::string installer; // the app's copy of the service installer; may be empty
};
struct BadRequest {
    std::optional<std::int64_t> id; // none when even the id couldn't be read
    int code = kInvalidRequest;
    std::string message;
};
using Request = std::variant<QueryRequest, StatusRequest, UpdateRequest, BadRequest>;

Request parseRequest(std::string_view body);
// Any method with its params (a JSON object's text, "" for none): what
// ServiceHost sends for a provider's Host::callService.
std::string encodeCall(std::int64_t id, std::string_view method, std::string_view params);
std::string queryParams(std::string_view text, std::size_t limit); // index.query's
std::string encodeQuery(std::int64_t id, std::string_view text, std::size_t limit);
std::string encodeStatusRequest(std::int64_t id);
std::string encodeUpdateRequest(std::int64_t id, std::string_view installer = {});

// --- answers -------------------------------------------------------------
struct ResultRow {
    std::string name;
    std::string path; // the whole path, "C:\Docs\report.pdf"
    bool folder = false;
    bool hidden = false;
    bool operator==(const ResultRow&) const = default;
};

enum class VolumeState { Scanning, Ready, Failed };

struct VolumeStatus {
    std::string root; // "C:\"
    VolumeState state = VolumeState::Scanning;
    std::size_t files = 0;
    bool operator==(const VolumeStatus&) const = default;
};

// What an update request did: an update started (the service is about to
// stop and come back as the new version), it's already the latest, one ran
// in the last few minutes, it's a dev build (never updates), or the check or
// download failed (it says why in its log).
enum class UpdateAnswer { Started, Current, Busy, DevBuild, Failed };

// Rows are dropped from the end if the answer would pass kMaxResponseBytes.
std::string encodeResults(std::int64_t id, const std::vector<ResultRow>& rows);
// version: the service's own ("0.8.0", or a dev build's "0.7.2-4-g45dbf60").
std::string encodeStatus(std::int64_t id, std::string_view version, const std::vector<VolumeStatus>& volumes);
std::string encodeUpdate(std::int64_t id, UpdateAnswer answer);
std::string encodeError(std::optional<std::int64_t> id, int code, std::string_view message);

// An answer as a client reads it: exactly one of rows, volumes, update or error.
struct Response {
    std::int64_t id = 0;
    std::optional<std::vector<ResultRow>> rows;
    std::optional<std::vector<VolumeStatus>> volumes;
    int protocol = 0;    // from a status answer
    std::string version; // likewise
    std::optional<UpdateAnswer> update;
    std::optional<std::string> error;
};
std::optional<Response> parseResponse(std::string_view body);

} // namespace cup::indexer
