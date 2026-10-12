#include "providers/file_store.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>

namespace cup {
namespace fs = std::filesystem;

namespace {

bool validName(const std::string& s, bool dots)
{
    if (s.empty() || s.size() > 64 || s.front() == '.') return false;
    return std::all_of(s.begin(), s.end(), [&](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || (dots && c == '.');
    });
}

} // namespace

bool validStoreKey(const std::string& key) { return validName(key, false); }

fs::path FileStore::fileFor(const std::string& providerId, const std::string& key) const
{
    if (dir_.empty() || !validName(providerId, /* dots */ true) || !validStoreKey(key)) return {};
    return dir_ / providerId / (key + ".txt");
}

std::string FileStore::load(const std::string& providerId, const std::string& key) const
{
    fs::path file = fileFor(providerId, key);
    if (file.empty()) return {};
    std::ifstream in(file, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

void FileStore::save(const std::string& providerId, const std::string& key, const std::string& text) const
{
    fs::path file = fileFor(providerId, key);
    if (file.empty()) return;
    std::error_code ec;
    if (text.empty()) {
        fs::remove(file, ec);
        return;
    }
    fs::create_directories(file.parent_path(), ec);
    fs::path side = file;
    side += ".new";
    {
        std::ofstream out(side, std::ios::binary | std::ios::trunc);
        out << text;
        if (!out.flush()) return; // disk full, no access: keep what was there
    }
    fs::rename(side, file, ec); // replaces the old file in one step
}

void FileStore::adopt(const fs::path& legacy, const std::string& providerId, const std::string& key) const
{
    fs::path file = fileFor(providerId, key);
    std::error_code ec;
    if (file.empty() || !fs::exists(legacy, ec) || fs::exists(file, ec)) return;
    fs::create_directories(file.parent_path(), ec);
    fs::rename(legacy, file, ec);
}

} // namespace cup
