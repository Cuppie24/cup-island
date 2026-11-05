#include "service/self_update.h"

#include "providers/update/release.h"
#include "providers/update/release_json.h"
#include "service/service_limits.h"
#include "service/service_log.h"

#include <bcrypt.h>
#include <winhttp.h>

#include <cstdio>
#include <functional>
#include <vector>

namespace cup::service {
namespace {

class HttpHandle {
public:
    explicit HttpHandle(HINTERNET h) : h_(h) {}
    ~HttpHandle()
    {
        if (h_) WinHttpCloseHandle(h_);
    }
    HttpHandle(const HttpHandle&) = delete;
    HttpHandle& operator=(const HttpHandle&) = delete;
    HINTERNET get() const { return h_; }
    explicit operator bool() const { return h_ != nullptr; }

private:
    HINTERNET h_;
};

// GETs an https URL from GitHub's API (JSON only: the service downloads no
// files, decision 088). Each chunk of the body goes to `sink`, which returns
// false to stop. False on any failure, a non-200 answer or more than maxBytes.
bool httpGet(const std::wstring& url, const std::string& version, std::size_t maxBytes,
             const std::function<bool(const char*, DWORD)>& sink)
{
    URL_COMPONENTS parts{ sizeof parts };
    wchar_t host[256] = {}, path[2048] = {};
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) return false;
    std::wstring agent = L"CupIslandIndex/" + widen(version); // GitHub's API wants a User-Agent
    HttpHandle session(WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                   WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) return false;
    WinHttpSetTimeouts(session.get(), 10000, 10000, 15000, 30000);
    HttpHandle connection(WinHttpConnect(session.get(), host, parts.nPort, 0));
    if (!connection) return false;
    HttpHandle request(WinHttpOpenRequest(connection.get(), L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request) return false;
    if (!WinHttpSendRequest(request.get(), L"Accept: application/vnd.github+json", static_cast<DWORD>(-1L),
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr))
        return false;
    DWORD status = 0, size = sizeof status;
    WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &size, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) {
        serviceLog("update: %ls answered %lu", host, status);
        return false;
    }
    std::vector<char> buffer(64 * 1024);
    std::size_t total = 0;
    for (;;) {
        DWORD read = 0;
        if (!WinHttpReadData(request.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &read)) return false;
        if (read == 0) return true;
        total += read;
        if (total > maxBytes || !sink(buffer.data(), read)) return false;
    }
}

// SHA-256 of everything readable from `file`, from its start, as lowercase hex.
std::string sha256Of(HANDLE file)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::string hex;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return hex;
    if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
        std::vector<UCHAR> buffer(64 * 1024);
        DWORD read = 0;
        bool ok = SetFilePointer(file, 0, nullptr, FILE_BEGIN) != INVALID_SET_FILE_POINTER;
        while (ok && ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) && read > 0)
            ok = BCryptHashData(hash, buffer.data(), read, 0) == 0;
        UCHAR digest[32];
        if (ok && BCryptFinishHash(hash, digest, sizeof digest, 0) == 0) {
            char two[3];
            for (UCHAR b : digest) {
                std::snprintf(two, sizeof two, "%02x", b);
                hex += two;
            }
        }
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return hex;
}

// This process's environment with TMP and TEMP pointing at `temp`: an
// installer unpacks helpers into its temp folder before running them, and
// SYSTEM's is C:\Windows\Temp, where any user can create files.
std::vector<wchar_t> environmentWithTemp(const std::filesystem::path& temp)
{
    std::vector<wchar_t> out;
    if (wchar_t* block = GetEnvironmentStringsW()) {
        for (const wchar_t* at = block; *at; at += wcslen(at) + 1) {
            std::wstring_view entry(at);
            if (_wcsnicmp(at, L"TMP=", 4) == 0 || _wcsnicmp(at, L"TEMP=", 5) == 0) continue;
            out.insert(out.end(), entry.begin(), entry.end());
            out.push_back(L'\0');
        }
        FreeEnvironmentStringsW(block);
    }
    for (const wchar_t* name : { L"TMP=", L"TEMP=" }) {
        std::wstring entry = name + temp.wstring();
        out.insert(out.end(), entry.begin(), entry.end());
        out.push_back(L'\0');
    }
    out.push_back(L'\0');
    return out;
}

// A copy of `source` (the app's, a plain local path) at `target`, at most
// kMaxInstallerBytes, and only from a fixed drive: not a USB stick or a
// mapped network drive another user could swap.
bool copyLocal(const std::wstring& source, const std::filesystem::path& target)
{
    std::wstring root = source.substr(0, 3);
    if (GetDriveTypeW(root.c_str()) != DRIVE_FIXED) return false;
    Handle in(CreateFileW(source.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!in.valid()) return false;
    Handle out(CreateFileW(target.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!out.valid()) return false;
    std::vector<char> buffer(64 * 1024);
    std::size_t total = 0;
    for (DWORD read = 0;;) {
        if (!ReadFile(in.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) return false;
        if (read == 0) return total > 0;
        total += read;
        DWORD written = 0;
        if (total > kMaxInstallerBytes || !WriteFile(out.get(), buffer.data(), read, &written, nullptr) || written != read) return false;
    }
}

// `file` reopened read-only, sharing only reading, if its SHA-256 is
// `sha256`: from then until the installer has started, nobody can change or
// delete it, and the hash was read through this same handle. Otherwise none.
Handle lockedIfMatches(const std::filesystem::path& file, const std::string& sha256)
{
    Handle locked(CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!locked.valid()) return Handle();
    std::string got = sha256Of(locked.get());
    if (got == sha256) return locked;
    serviceLog("update: checksum mismatch (got %s, expected %s)", got.c_str(), sha256.c_str());
    return Handle();
}

} // namespace

SelfUpdate::SelfUpdate(std::filesystem::path folder, std::string runningVersion)
    : folder_(std::move(folder)), version_(std::move(runningVersion))
{
}

SelfUpdate::~SelfUpdate()
{
    if (worker_.joinable()) worker_.join();
}

void SelfUpdate::removeLeftovers()
{
    std::error_code ec;
    std::filesystem::remove_all(folder_ / L"updates", ec);
}

indexer::UpdateAnswer SelfUpdate::request(const std::string& installer)
{
    // An installed dev build moves to the latest release when asked (the app
    // only asks the installed service): any release differs from it.
    Version running = parseVersion(version_).value_or(Version{ 0, 0, 0 });
    {
        std::lock_guard guard(lock_);
        ULONGLONG now = GetTickCount64();
        if (busy_ || (lastAt_ && now - lastAt_ < kUpdateSpacingMs)) return indexer::UpdateAnswer::Busy;
        lastAt_ = now;
        busy_ = true;
        // Still this version, so the last installer didn't replace us: say how it ended.
        DWORD code = 0;
        if (installer_.valid() && GetExitCodeProcess(installer_.get(), &code)) {
            if (code == STILL_ACTIVE) serviceLog("update: the last installer is still running");
            else serviceLog("update: the last installer exited with code %lu without replacing this service", code);
            installer_ = Handle();
        }
    }
    if (folder_.empty()) {
        std::lock_guard guard(lock_);
        busy_ = false;
        serviceLog("update: couldn't tell where this service is installed");
        return indexer::UpdateAnswer::Failed;
    }
    std::string json;
    bool fetched = httpGet(std::wstring(L"https://") + kGitHubHost + kLatestReleasePath, version_, kMaxReleaseJsonBytes,
                           [&](const char* data, DWORD size) {
                               json.append(data, size);
                               return true;
                           });
    auto release = fetched ? parseReleaseJson(json) : std::nullopt;
    auto offer = release ? indexOfferFor(running, *release) : std::nullopt;
    if (!offer) {
        std::lock_guard guard(lock_);
        busy_ = false;
        if (!release) {
            serviceLog("update: couldn't read GitHub's latest release");
            return indexer::UpdateAnswer::Failed;
        }
        return indexer::UpdateAnswer::Current;
    }
    std::string local = plainLocalPath(installer) ? installer : std::string();
    if (!installer.empty() && local.empty()) serviceLog("update: the app's installer path isn't a plain local one; ignored");
    if (local.empty()) { // the service downloads nothing itself (decision 088)
        std::lock_guard guard(lock_);
        busy_ = false;
        serviceLog("update: the app sent no installer for %s", versionText(offer->version).c_str());
        return indexer::UpdateAnswer::Failed;
    }
    serviceLog("update: %s -> %s", version_.c_str(), versionText(offer->version).c_str());
    if (worker_.joinable()) worker_.join(); // an earlier one, long finished
    worker_ = std::thread([this, sha = offer->sha256, name = offer->fileName, local] { install(sha, name, local); });
    return indexer::UpdateAnswer::Started;
}

void SelfUpdate::install(std::string sha256, std::string fileName, std::string installer)
{
    std::filesystem::path updates = folder_ / L"updates", file = updates / widen(fileName);
    auto finish = [this](const char* problem) {
        if (problem) serviceLog("update: %s", problem);
        std::lock_guard guard(lock_);
        busy_ = false;
    };
    std::error_code ec;
    std::filesystem::create_directories(updates, ec);
    // Only the app's copy (decisions 081, 088), and only if it has the release's hash.
    if (!copyLocal(widen(installer), file)) return finish("couldn't copy the app's installer");
    Handle locked = lockedIfMatches(file, sha256);
    if (!locked.valid()) {
        std::filesystem::remove(file, ec);
        return finish("the app's copy isn't the release's installer");
    }
    serviceLog("update: running the installer the app brought");
    std::wstring command = L"\"" + file.wstring() + L"\" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /LOG=\"" +
                           (folder_ / L"index-update.log").wstring() + L"\"";
    std::vector<wchar_t> environment = environmentWithTemp(updates);
    STARTUPINFOW startup{ sizeof startup };
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(file.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
                        environment.data(), updates.c_str(), &startup, &process))
        return finish("couldn't start the installer");
    CloseHandle(process.hThread);
    serviceLog("update: installer started; it will stop this service and start the new one (its log: %ls)",
               (folder_ / L"index-update.log").c_str());
    {
        std::lock_guard guard(lock_);
        installer_ = Handle(process.hProcess);
    }
    finish(nullptr);
}

} // namespace cup::service
