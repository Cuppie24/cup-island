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
    # Release builds keep debug info too (in each .obj: no shared .pdb for
    # parallel compiles to fight over), so the linker writes CupIsland.pdb:
    # what reads a crash or hang dump from an installed copy (decisions 013, 036).
    add_compile_options($<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CONFIG:Debug>>>:/Z7>) # C++ only, not the .rc
else()
    add_compile_options(-Wall -Wextra -Wno-missing-field-initializers)
endif()

# --- core: pure C++, builds and tests on any OS --------------------------
add_library(cup_core STATIC
    src/core/alert_queue.cpp
    src/core/badges.cpp
    src/core/card_overlays.cpp
    src/core/content.cpp
    src/core/dialog.cpp
    src/core/easing.cpp
    src/core/fullscreen.cpp
    src/core/hit_test.cpp
    src/core/home.cpp
    src/core/island.cpp
    src/core/island_queries.cpp
    src/core/item_metrics.cpp
    src/core/keyboard.cpp
    src/core/json_reader.cpp
    src/core/layout.cpp
    src/core/lift.cpp
    src/core/lists.cpp
    src/core/placement.cpp
    src/core/shortcuts.cpp
    src/core/slot_layout.cpp
    src/core/split.cpp
    src/core/tabs.cpp
    src/core/text_edit.cpp
    src/core/tokens.cpp
    src/core/tokens_json.cpp
)
target_include_directories(cup_core PUBLIC src)

# --- providers: data sources; the pure parts build on any OS -------------------
add_library(cup_providers STATIC
    src/providers/demo/demo_media_provider.cpp
    src/providers/media/art_state.cpp
    src/providers/media/media_presenter.cpp
    src/providers/media/now_playing.cpp
    src/providers/media/session_choice.cpp
    src/providers/media/tab_match.cpp
    src/providers/pomodoro/pomodoro_provider.cpp
    src/providers/pomodoro/pomodoro_session.cpp
    src/providers/pomodoro/todo_list.cpp
    src/providers/system/settings_card.cpp
    src/providers/system/settings_provider.cpp
    src/providers/system/system_panels.cpp
    src/providers/file_store.cpp
    src/providers/update/release.cpp
    src/providers/update/update_provider.cpp
    src/providers/provider_registry.cpp
)
target_link_libraries(cup_providers PUBLIC cup_core)

option(CUP_BUILD_TESTS "Build core tests" ON)
if(CUP_BUILD_TESTS)
    enable_testing()
    add_executable(cup_tests
        tests/badge_tests.cpp
        tests/content_tests.cpp
        tests/fullscreen_tests.cpp
        tests/home_tests.cpp
        tests/item_tests.cpp
        tests/keyboard_tests.cpp
        tests/list_card_tests.cpp
        tests/main.cpp
        tests/media_tests.cpp
        tests/placement_tests.cpp
        tests/pomodoro_tests.cpp
        tests/provider_tests.cpp
        tests/unit_tests.cpp
        tests/update_tests.cpp
        tests/scenario_runner.cpp
        tests/settings_tests.cpp
        tests/shortcut_settings_tests.cpp
        tests/store_tests.cpp
        tests/system_tests.cpp
        tests/todo_tests.cpp
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
    # The app is x64-only (installer, TR §3). A 32-bit compiler here usually
    # means a default "Developer PowerShell for VS"; build.ps1 picks x64 itself.
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "32-bit compiler detected. Use .\\build.ps1, or an x64 developer environment, then delete ${CMAKE_BINARY_DIR} and configure again.")
    endif()
    enable_language(RC)

    # The version (decision 036). The Release workflow passes its own through
    # build.ps1 (-DCUP_VERSION=0.4.0); build.ps1 passes git's for a local build.
    # Configured without one (an IDE), it asks git here: "0.3.0" on a release
    # tag, "0.3.0-5-gabc1234" five commits on, "-dirty" with local changes.
    # Anything but X.Y.Z is a dev build, which never offers updates.
    set(CUP_VERSION "" CACHE STRING "The app's version, X.Y.Z (empty: ask git)")
    set(CUP_APP_VERSION "${CUP_VERSION}")
    if(NOT CUP_APP_VERSION)
        execute_process(COMMAND git describe --tags --match "v[0-9]*" --dirty
                        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE CUP_APP_VERSION
                        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE describeFailed)
        if(describeFailed OR NOT CUP_APP_VERSION)
            set(CUP_APP_VERSION "0.0.0-dev") # no git, or no release tag yet
        endif()
    endif()
    string(REGEX REPLACE "^v" "" CUP_APP_VERSION "${CUP_APP_VERSION}")
    set(CUP_VERSION_MAJOR 0)
    set(CUP_VERSION_MINOR 0)
    set(CUP_VERSION_PATCH 0)
    if(CUP_APP_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
        set(CUP_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(CUP_VERSION_MINOR ${CMAKE_MATCH_2})
        set(CUP_VERSION_PATCH ${CMAKE_MATCH_3})
    endif()
    configure_file(src/app/cup_version.h.in "${CMAKE_BINARY_DIR}/generated/cup_version.h" @ONLY)
    message(STATUS "Cup Island version: ${CUP_APP_VERSION}")

    add_executable(CupIsland
        assets/cup_island.rc
        src/app/keyboard_input.cpp
        src/app/live_tokens.cpp
        src/app/main.cpp
        src/platform/autostart.cpp
        src/platform/badge_dots.cpp
        src/platform/blob.cpp
        src/platform/blob_motion.cpp
        src/platform/blob_shape.cpp
        src/platform/crash_report.cpp
        src/platform/displays.cpp
        src/platform/element_art.cpp
        src/platform/element_painting.cpp
        src/platform/field_painting.cpp
        src/platform/element_parts.cpp
        src/platform/element_visuals.cpp
        src/platform/list_viewports.cpp
        src/platform/file_watcher.cpp
        src/platform/fullscreen_watcher.cpp
        src/platform/graphics_device.cpp
        src/platform/hotkeys.cpp
        src/platform/interaction_layer.cpp
        src/platform/log.cpp
        src/platform/liquid_bridge.cpp
        src/platform/pill_renderer.cpp
        src/platform/render_layer.cpp
        src/platform/single_instance.cpp
        src/platform/text_input.cpp
        src/platform/stay_on_top.cpp
        src/platform/tray_icon.cpp
        src/platform/watchdog.cpp
        src/platform/wall_clock.cpp
        src/providers/media/app_activation.cpp
        src/providers/media/browser_tabs.cpp
        src/providers/media/smtc_media_provider.cpp
        src/providers/media/smtc_reading.cpp
        src/providers/system/theme_provider.cpp
        src/providers/update/github.cpp
    )
    # The icon isn't an #include, so the build doesn't see it change on its own.
    set_source_files_properties(assets/cup_island.rc PROPERTIES OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/assets/cup_island.ico)
    target_include_directories(CupIsland PRIVATE "${CMAKE_BINARY_DIR}/generated") # cup_version.h, for the code and the .rc
    target_link_libraries(CupIsland PRIVATE cup_providers WindowsApp CoreMessaging User32 Advapi32 Ole32 Dbghelp d3d11 d2d1 dwrite dxgi dwmapi shell32 Shcore wevtapi)
    # Release: a GUI app, no console window (quit from the tray icon). Debug keeps
    # the console for the log. mainCRTStartup lets main() stay the entry point.
    # /DEBUG writes the pdb; it would turn off /OPT:REF and /OPT:ICF, so they're asked back.
    target_link_options(CupIsland PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:/SUBSYSTEM:WINDOWS>
        $<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>
        $<$<NOT:$<CONFIG:Debug>>:/DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:/OPT:REF>
        $<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>)
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
