// The providers' store (decision 028): each provider's keys are its own, and
// the file store writes them under the data folder, safely.

#include "expect.h"
#include "providers/file_store.h"
#include "providers/provider_registry.h"

#include <filesystem>
#include <fstream>

namespace cup::test {
namespace {

// Saves and loads one key through the Host it's given.
struct Keeper : Provider {
    std::string name;
    Host* host = nullptr;
    explicit Keeper(std::string id) : name(std::move(id)) {}
    const char* id() const override { return name.c_str(); }
    void start(Host& h) override { host = &h; }
    void stop() override {}
    void onAction(const Action&) override {}
};

void write(const std::filesystem::path& file, const std::string& text)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << text;
}

} // namespace

void runStoreTests()
{
    expect(validStoreKey("todos") && validStoreKey("a_b-9") && !validStoreKey("") && !validStoreKey("../x") &&
           !validStoreKey("a.b") && !validStoreKey("a/b") && !validStoreKey(std::string(65, 'a')), "keys are plain file names");

    // In memory (no store handed in): per provider, and bad keys do nothing.
    ProviderRegistry registry{ [](const Event&, const char*) {}, [] { return Ms{ 42 }; } };
    auto a = std::make_unique<Keeper>("a");
    auto b = std::make_unique<Keeper>("b");
    Keeper* first = a.get();
    Keeper* second = b.get();
    registry.add(std::move(a));
    registry.add(std::move(b));
    registry.start("a");
    registry.start("b");
    first->host->save("list", "one");
    second->host->save("list", "two");
    first->host->save("../b/list", "sneaky");
    expect(first->host->load("list") == "one" && second->host->load("list") == "two", "each provider has its own keys");
    expect(first->host->load("../b/list").empty(), "a key that isn't a name is ignored");
    expect(first->host->wallClock() == Ms{ 42 }, "no wall clock handed in: the registry's clock");

    // On disk.
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "cup_store_tests";
    fs::remove_all(dir);
    FileStore files(dir / "store");
    expect(files.load("pomodoro", "todos").empty(), "nothing saved yet: empty");
    files.save("pomodoro", "todos", "[ ] Ship it\n");
    expect(files.load("pomodoro", "todos") == "[ ] Ship it\n" && fs::exists(dir / "store" / "pomodoro" / "todos.txt"),
           "saved under <provider>/<key>.txt");
    expect(!fs::exists(dir / "store" / "pomodoro" / "todos.txt.new"), "no side file left behind");
    files.save("pomodoro", "todos", "");
    expect(!fs::exists(dir / "store" / "pomodoro" / "todos.txt"), "saving nothing deletes it");
    files.save("..", "x", "no");
    files.save("ok", "../x", "no");
    expect(!fs::exists(dir / "x.txt") && !fs::exists(dir / "store" / "x.txt"), "ids and keys can't leave the folder");

    // Files from before the store move in, once.
    write(dir / "todos.txt", "[x] Old\n");
    files.adopt(dir / "todos.txt", "pomodoro", "todos");
    expect(files.load("pomodoro", "todos") == "[x] Old\n" && !fs::exists(dir / "todos.txt"), "a legacy file moves in");
    write(dir / "todos.txt", "stale");
    files.adopt(dir / "todos.txt", "pomodoro", "todos");
    expect(files.load("pomodoro", "todos") == "[x] Old\n", "but never over what the store has");
    files.adopt(dir / "missing.txt", "pomodoro", "other");
    expect(files.load("pomodoro", "other").empty(), "a missing legacy file: nothing");
    fs::remove_all(dir);
}

} // namespace cup::test
