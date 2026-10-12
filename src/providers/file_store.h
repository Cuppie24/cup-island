#pragma once
// Where providers' saved text lives on disk (Host::load/save, decision 028):
// <dir>/<provider id>/<key>.txt. Portable (std::filesystem), so it's tested
// on Linux; the app hands it to the registry.

#include <filesystem>
#include <string>

namespace cup {

// A key is a file name: letters, digits, '-' and '_', 1..64 of them.
bool validStoreKey(const std::string& key);

class FileStore {
public:
    explicit FileStore(std::filesystem::path dir) : dir_(std::move(dir)) {}

    std::string load(const std::string& providerId, const std::string& key) const; // "" if none
    // Written to a side file, then renamed over the old one: a crash
    // mid-write leaves the previous text, never half of each. "" deletes it.
    void save(const std::string& providerId, const std::string& key, const std::string& text) const;

    // A file from before the store (todos.txt): moved in as `key`, unless
    // the store already has one. Gone or unreadable: nothing happens.
    void adopt(const std::filesystem::path& legacy, const std::string& providerId, const std::string& key) const;

private:
    std::filesystem::path fileFor(const std::string& providerId, const std::string& key) const; // empty if invalid
    std::filesystem::path dir_;
};

} // namespace cup
