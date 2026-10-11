cmake_minimum_required(VERSION 3.21)
project(CupIsland LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Link the MSVC C++ runtime into the exe, so it runs on PCs without the
# Visual C++ Redistributable and the installer doesn't need to ship it.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MSVC)
    add_compile_options(/W3 /EHsc /utf-8 /permissive-)
    add_compile_definitions(UNICODE _UNICODE)
else()
    add_compile_options(-Wall -Wextra -Wno-missing-field-initializers)
endif()

# --- core: pure C++, builds and tests on any OS --------------------------
add_library(cup_core STATIC
    src/core/alert_queue.cpp
    src/core/content.cpp
    src/core/easing.cpp
    src/core/fullscreen.cpp
    src/core/hit_test.cpp
    src/core/home.cpp
    src/core/island.cpp
    src/core/layout.cpp
    src/core/slot_layout.cpp
    src/core/split.cpp
    src/core/tabs.cpp
    src/core/tokens.cpp
    src/core/tokens_json.cpp
)
target_include_directories(cup_core PUBLIC src)

# --- providers: data sources; the pure parts build on any OS -------------------
add_library(cup_providers STATIC
    src/providers/demo/demo_media_provider.cpp
    src/providers/media/media_presenter.cpp
    src/providers/media/now_playing.cpp
    src/providers/pomodoro/pomodoro_provider.cpp
    src/providers/system/system_panels.cpp
    src/providers/provider_registry.cpp
)
target_link_libraries(cup_providers PUBLIC cup_core)

option(CUP_BUILD_TESTS "Build core tests" ON)
if(CUP_BUILD_TESTS)
    enable_testing()
    add_executable(cup_tests
        tests/content_tests.cpp
        tests/fullscreen_tests.cpp
        tests/home_tests.cpp
        tests/item_tests.cpp
        tests/main.cpp
        tests/media_tests.cpp
        tests/pomodoro_tests.cpp
        tests/provider_tests.cpp
        tests/unit_tests.cpp
        tests/scenario_runner.cpp
        tests/system_tests.cpp
        tests/tokens_tests.cpp
    )
    target_link_libraries(cup_tests PRIVATE cup_providers)
    target_compile_definitions(cup_tests PRIVATE
        CUP_SCENARIO_DIR="${CMAKE_SOURCE_DIR}/tests/scenarios"
        CUP_TOKENS_FILE="${CMAKE_SOURCE_DIR}/assets/tokens.json")
    add_test(NAME core COMMAND cup_tests)
endif()

# --- app: Windows only ----------------------------------------------------
if(WIN32)
    enable_language(RC)
    add_executable(CupIsland
        assets/cup_island.rc
        src/app/live_tokens.cpp
        src/app/main.cpp
        src/platform/blob.cpp
        src/platform/element_painting.cpp
        src/platform/element_parts.cpp
        src/platform/element_visuals.cpp
        src/platform/file_watcher.cpp
        src/platform/fullscreen_watcher.cpp
        src/platform/graphics_device.cpp
        src/platform/interaction_layer.cpp
        src/platform/liquid_bridge.cpp
        src/platform/pill_renderer.cpp
        src/platform/render_layer.cpp
        src/platform/tray_icon.cpp
        src/platform/wall_clock.cpp
        src/providers/media/smtc_media_provider.cpp
        src/providers/media/smtc_reading.cpp
        src/providers/system/battery_provider.cpp
        src/providers/system/bluetooth_provider.cpp
        src/providers/system/theme_provider.cpp
    )
    target_link_libraries(CupIsland PRIVATE cup_providers WindowsApp CoreMessaging User32 dcomp d3d11 d2d1 dwrite dxgi dwmapi shell32)
    # Release: a GUI app, no console window (quit from the tray icon). Debug keeps
    # the console for the log. mainCRTStartup lets main() stay the entry point.
    target_link_options(CupIsland PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:/SUBSYSTEM:WINDOWS>
        $<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>)
    # Dev builds read and live-reload the source tree's tokens.json directly.
    target_compile_definitions(CupIsland PRIVATE CUP_TOKENS_SOURCE_PATH=L"${CMAKE_SOURCE_DIR}/assets/tokens.json")

    # C++/WinRT headers ship in the Windows SDK; most setups already have
    # them on the include path, this covers the ones that don't.
    if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        set(CUP_CPPWINRT_DIR "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/cppwinrt")
        if(EXISTS "${CUP_CPPWINRT_DIR}")
            target_include_directories(CupIsland PRIVATE "${CUP_CPPWINRT_DIR}")
        endif()
    endif()
endif()
