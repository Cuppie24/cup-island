#include "providers/update/github.h"

#include "platform/log.h"
#include "platform/win.h"

#include <shellapi.h>

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Security.Cryptography.Core.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.Filters.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Web.Http.h>

#include <filesystem>
#include <fstream>

namespace cup {
namespace {

namespace http = winrt::Windows::Web::Http;
namespace json = winrt::Windows::Data::Json;
namespace crypto = winrt::Windows::Security::Cryptography;
using platform::logLine;

constexpr wchar_t kLatestRelease[] = L"https://api.github.com/repos/Cuppie24/cup-island/releases/latest";

// GitHub refuses requests without a User-Agent. No cache: a stale "latest"
// would hide a release for as long as the cache kept it.
http::HttpClient client(const std::string& appVersion, bool api)
{
    http::Filters::HttpBaseProtocolFilter filter;
    filter.CacheControl().ReadBehavior(http::Filters::HttpCacheReadBehavior::MostRecent);
    http::HttpClient c(filter);
    auto headers = c.DefaultRequestHeaders();
    headers.UserAgent().TryParseAdd(L"CupIsland/" + winrt::to_hstring(appVersion));
    if (api) headers.Accept().TryParseAdd(L"application/vnd.github+json");
    return c;
}

// A string field, or "" if it's missing or not a string (GitHub sends null for some).
std::string field(const json::JsonObject& o, const wchar_t* name)
{
    if (!o.HasKey(name)) return {};
    auto value = o.GetNamedValue(name);
    return value.ValueType() == json::JsonValueType::String ? winrt::to_string(value.GetString()) : std::string();
}

std::optional<GitHubRelease> parseRelease(const winrt::hstring& text)
{
    json::JsonObject o{ nullptr };
    if (!json::JsonObject::TryParse(text, o)) return std::nullopt;
    GitHubRelease r{ .tag = field(o, L"tag_name"), .page = field(o, L"html_url") };
    if (o.HasKey(L"assets") && o.GetNamedValue(L"assets").ValueType() == json::JsonValueType::Array) {
        for (const auto& value : o.GetNamedArray(L"assets")) {
            if (value.ValueType() != json::JsonValueType::Object) continue;
            auto a = value.as<json::JsonObject>(); // not GetObject(): windows.h #defines that to GetObjectW
            r.assets.push_back({ field(a, L"name"), field(a, L"browser_download_url"), field(a, L"digest") });
        }
    }
    return r;
}

std::filesystem::path updatesFolder() { return platform::logFolder().parent_path() / L"updates"; } // %LOCALAPPDATA%\CupIsland\updates

std::string sha256Of(const winrt::Windows::Storage::Streams::IBuffer& data)
{
    auto sha256 = crypto::Core::HashAlgorithmProvider::OpenAlgorithm(crypto::Core::HashAlgorithmNames::Sha256());
    return winrt::to_string(crypto::CryptographicBuffer::EncodeToHexString(sha256.HashData(data))); // lowercase
}

// Silent, no prompts, no reboot; the installer's own log next to ours, for a failed update.
bool runInstaller(const std::filesystem::path& installer)
{
    std::wstring parameters = L"/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /LOG=\"" +
                              (platform::logFolder() / L"update-install.log").wstring() + L"\"";
    SHELLEXECUTEINFOW info{ sizeof info };
    info.fMask = SEE_MASK_NOASYNC;
    info.lpFile = installer.c_str();
    info.lpParameters = parameters.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&info)) return true;
    logLine("update: couldn't start %ls (0x%08lx)", installer.c_str(), GetLastError());
    return false;
}

winrt::fire_and_forget fetch(std::string appVersion, std::function<void(std::optional<GitHubRelease>)> done)
{
    winrt::apartment_context ui; // `done` runs back on this (the UI) thread
    std::optional<GitHubRelease> release;
    try {
        auto response = co_await client(appVersion, true).GetAsync(winrt::Windows::Foundation::Uri(kLatestRelease));
        if (response.IsSuccessStatusCode()) {
            release = parseRelease(co_await response.Content().ReadAsStringAsync());
            if (release) logLine("update check: the latest release is %s", release->tag.c_str());
            else logLine("update check: GitHub's answer didn't parse");
        } else {
            logLine("update check: GitHub answered %d", static_cast<int>(response.StatusCode()));
        }
    } catch (const winrt::hresult_error& e) {
        logLine("update check failed (offline?): 0x%08x %ls", static_cast<unsigned>(e.code()), e.message().c_str());
    }
    try {
        co_await ui;
    } catch (...) {
        co_return; // the app is closing: nobody to tell
    }
    done(release);
}

winrt::fire_and_forget install(UpdateOffer offer, std::string appVersion, std::function<void()> quit, std::function<void(std::string)> failed)
{
    winrt::apartment_context ui;
    std::string problem;
    std::filesystem::path installer = updatesFolder() / winrt::to_hstring(offer.fileName).c_str();
    try {
        logLine("update: downloading %s", offer.url.c_str());
        auto response = co_await client(appVersion, false).GetAsync(winrt::Windows::Foundation::Uri(winrt::to_hstring(offer.url)));
        if (!response.IsSuccessStatusCode()) {
            logLine("update: the download answered %d", static_cast<int>(response.StatusCode()));
            problem = "Couldn't download the update";
        } else {
            auto data = co_await response.Content().ReadAsBufferAsync();
            co_await winrt::resume_background(); // hashing and writing a few MB: off the UI thread
            std::string sha = sha256Of(data);
            if (sha != offer.sha256) {
                logLine("update: checksum mismatch (got %s, expected %s)", sha.c_str(), offer.sha256.c_str());
                problem = "The download was damaged. Try again";
            } else {
                std::error_code error;
                std::filesystem::create_directories(installer.parent_path(), error);
                std::ofstream out(installer, std::ios::binary | std::ios::trunc);
                out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.Length()));
                if (!out) problem = "Couldn't save the update";
            }
        }
    } catch (const winrt::hresult_error& e) {
        logLine("update: download failed: 0x%08x %ls", static_cast<unsigned>(e.code()), e.message().c_str());
        problem = "Couldn't download the update";
    }
    try {
        co_await ui;
    } catch (...) {
        co_return;
    }
    if (problem.empty()) {
        if (runInstaller(installer)) {
            logLine("update: installing %s; quitting so it can replace the island", offer.fileName.c_str());
            quit();
            co_return;
        }
        problem = "Couldn't start the installer";
    }
    failed(problem);
}

} // namespace

void fetchLatestRelease(const std::string& appVersion, std::function<void(std::optional<GitHubRelease>)> done)
{
    fetch(appVersion, std::move(done));
}

void downloadAndInstall(const UpdateOffer& offer, const std::string& appVersion, std::function<void()> quit,
                        std::function<void(std::string)> failed)
{
    install(offer, appVersion, std::move(quit), std::move(failed));
}

void removeOldInstallers()
{
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(updatesFolder(), error))
        if (entry.path().extension() == L".exe") std::filesystem::remove(entry.path(), error); // one still running stays: next time
}

void openInBrowser(const std::string& url)
{
    if (url.rfind("https://", 0) != 0) return; // only web pages (offerFor keeps GitHub's)
    ShellExecuteW(nullptr, L"open", winrt::to_hstring(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

} // namespace cup
