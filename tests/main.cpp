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
void runCommandTests();
void runMediaTests();
void runLyricsTests();
void runFullscreenTests();
void runPageRowTests();
void runPomodoroTests();
void runPomodoroStatsTests();
void runSystemTests();
void runItemTests();
void runTodoTests();
void runPlacementTests();
void runStoreTests();
void runKeyboardTests();
void runBadgeTests();
void runListCardTests();
void runGroupTests();
void runSettingsTests();
void runSettingsNewTests();
void runShortcutSettingsTests();
void runUpdateTests();
void runUpdateInstallTests();
void runCoverTests();
void runMotionTests();
void runIndexerTests();
void runIndexerQueryTests();
void runIndexerProtocolTests();
void runServiceHostTests();
void runDoubleTapTests();
void runSearchTests();
void runSearchCommandTests();
void runWindowTests();
void runWindowThemeTests();
void runWhatsNewTests();
void runWindowCardTests();
void runWindowMenuTests();
void runWindowDragTests();
void runTaskBoardTests();
void runWindowFieldTests();
void runMenuTests();
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
    runCommandTests();
    runMediaTests();
    runLyricsTests();
    runFullscreenTests();
    runPageRowTests();
    runPomodoroTests();
    runPomodoroStatsTests();
    runSystemTests();
    runItemTests();
    runTodoTests();
    runPlacementTests();
    runStoreTests();
    runKeyboardTests();
    runBadgeTests();
    runListCardTests();
    runGroupTests();
    runSettingsTests();
    runSettingsNewTests();
    runShortcutSettingsTests();
    runUpdateTests();
    runUpdateInstallTests();
    runCoverTests();
    runMotionTests();
    runIndexerTests();
    runIndexerQueryTests();
    runIndexerProtocolTests();
    runServiceHostTests();
    runDoubleTapTests();
    runSearchTests();
    runSearchCommandTests();
    runWindowTests();
    runWindowThemeTests();
    runWhatsNewTests();
    runWindowCardTests();
    runWindowMenuTests();
    runWindowDragTests();
    runTaskBoardTests();
    runWindowFieldTests();
    runMenuTests();

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
