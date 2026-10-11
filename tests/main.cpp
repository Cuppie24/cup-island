// Test entry point: unit tests, then every *.scenario file in the directory
// given as argv[1] (defaults to tests/scenarios next to the sources).

#include "expect.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace cup::test {
void runUnitTests();
void runTokensTests();
void runContentTests();
void runProviderTests();
void runMediaTests();
void runFullscreenTests();
void runHomeTests();
void runPomodoroTests();
void runSystemTests();
void runItemTests();
void runTodoTests();
int runScenarioFile(const std::filesystem::path& file); // returns scenarios run
}

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;
    using namespace cup::test;

    runUnitTests();
    runTokensTests();
    runContentTests();
    runProviderTests();
    runMediaTests();
    runFullscreenTests();
    runHomeTests();
    runPomodoroTests();
    runSystemTests();
    runItemTests();
    runTodoTests();

    fs::path dir = argc > 1 ? fs::path(argv[1]) : fs::path(CUP_SCENARIO_DIR);
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir))
        if (entry.path().extension() == ".scenario") files.push_back(entry.path());
    std::sort(files.begin(), files.end());

    int scenarios = 0;
    for (const auto& file : files) scenarios += runScenarioFile(file);

    std::printf("%d scenarios in %zu files, %d failure(s)\n", scenarios, files.size(), failures());
    return failures() == 0 ? 0 : 1;
}
