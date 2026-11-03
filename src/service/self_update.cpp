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

// GETs an https URL. Each chunk of the body goes to `sink`, which returns
// false to stop. Follows GitHub's redirect to its download host. False on
// any failure, a non-200 answer or more than maxBytes.
bool httpGet(const std::wstring& url, const std::string& version, bool api, std::size_t maxBytes,
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
    const wchar_t* accept = api ? L"Accept: application/vnd.github+json" : WINHTTP_NO_ADDITIONAL_HEADERS;
    if (!WinHttpSendRequest(request.get(), accept, api ? static_cast<DWORD>(-1L) : 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
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

indexer::UpdateAnswer SelfUpdate::request()
{
    auto running = parseVersion(version_);
    if (!running) return indexer::UpdateAnswer::DevBuild; // a dev build never updates (decision 036)
    {
        std::lock_guard guard(lock_);
        ULONGLONG now = GetTickCount64();
        if (busy_ || (lastAt_ && now - lastAt_ < kUpdateSpacingMs)) return indexer::UpdateAnswer::Busy;
        lastAt_ = now;
        busy_ = true;
    }
    std::string json;
    bool fetched = httpGet(std::wstring(L"https://") + kGitHubHost + kLatestReleasePath, version_, true, kMaxReleaseJsonBytes,
                           [&](const char* data, DWORD size) {
                               json.append(data, size);
                               return true;
                           });
    auto release = fetched ? parseReleaseJson(json) : std::nullopt;
    auto offer = release ? indexOfferFor(*running, *release) : std::nullopt;
    if (!offer) {
        std::lock_guard guard(lock_);
        busy_ = false;
        if (!release) {
            serviceLog("update: couldn't read GitHub's latest release");
            return indexer::UpdateAnswer::Failed;
        }
        return indexer::UpdateAnswer::Current;
    }
    serviceLog("update: %s -> %s", version_.c_str(), versionText(offer->version).c_str());
    if (worker_.joinable()) worker_.join(); // an earlier one, long finished
    worker_ = std::thread([this, url = offer->url, sha = offer->sha256, name = offer->fileName] { install(url, sha, name); });
    return indexer::UpdateAnswer::Started;
}

void SelfUpdate::install(std::string url, std::string sha256, std::string fileName)
{
    std::filesystem::path updates = folder_ / L"updates", file = updates / widen(fileName);
    auto finish = [this](const char* problem) {
        if (problem) serviceLog("update: %s", problem);
        std::lock_guard guard(lock_);
        busy_ = false;
    };
    std::error_code ec;
    std::filesystem::create_directories(updates, ec);
    {
        Handle out(CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!out.valid()) return finish("couldn't create the download");
        bool ok = httpGet(widen(url), version_, false, kMaxInstallerBytes, [&](const char* data, DWORD size) {
            DWORD written = 0;
            return WriteFile(out.get(), data, size, &written, nullptr) && written == size;
        });
        if (!ok) return finish("the download failed");
    }
    // Reopened read-only, sharing only reading: from here until the installer
    // has started, nobody can change or delete it. The hash is read through
    // this same handle.
    Handle locked(CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!locked.valid()) return finish("couldn't reopen the download");
    std::string got = sha256Of(locked.get());
    if (got != sha256) {
        serviceLog("update: checksum mismatch (got %s, expected %s)", got.c_str(), sha256.c_str());
        locked.reset();
        std::filesystem::remove(file, ec);
        return finish(nullptr);
    }
    std::wstring command = L"\"" + file.wstring() + L"\" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /LOG=\"" +
                           (folder_ / L"index-update.log").wstring() + L"\"";
    std::vector<wchar_t> environment = environmentWithTemp(updates);
    STARTUPINFOW startup{ sizeof startup };
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(file.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
                        environment.data(), updates.c_str(), &startup, &process))
        return finish("couldn't start the installer");
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    serviceLog("update: installer started; it will stop this service and start the new one");
    finish(nullptr);
}

} // namespace cup::service
