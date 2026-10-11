cmake_minimum_required(VERSION 3.21)
project(CupIsland LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W3 /EHsc /utf-8 /permissive-)
    add_compile_definitions(UNICODE _UNICODE)
else()
    add_compile_options(-Wall -Wextra -Wno-missing-field-initializers)
endif()

# --- core: pure C++, builds and tests on any OS --------------------------
add_library(cup_core STATIC
    src/core/easing.cpp
    src/core/island.cpp
    src/core/layout.cpp
)
target_include_directories(cup_core PUBLIC src)

option(CUP_BUILD_TESTS "Build core tests" ON)
if(CUP_BUILD_TESTS)
    enable_testing()
    add_executable(cup_tests
        tests/main.cpp
        tests/unit_tests.cpp
        tests/scenario_runner.cpp
    )
    target_link_libraries(cup_tests PRIVATE cup_core)
    target_compile_definitions(cup_tests PRIVATE CUP_SCENARIO_DIR="${CMAKE_SOURCE_DIR}/tests/scenarios")
    add_test(NAME core COMMAND cup_tests)
endif()

# --- app: Windows only ----------------------------------------------------
if(WIN32)
    add_executable(CupIsland
        src/app/main.cpp
        src/platform/interaction_layer.cpp
        src/platform/pill_renderer.cpp
        src/platform/render_layer.cpp
    )
    target_link_libraries(CupIsland PRIVATE cup_core WindowsApp CoreMessaging User32 dcomp)

    # C++/WinRT headers ship in the Windows SDK; most setups already have
    # them on the include path, this covers the ones that don't.
    if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        set(CUP_CPPWINRT_DIR "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/cppwinrt")
        if(EXISTS "${CUP_CPPWINRT_DIR}")
            target_include_directories(CupIsland PRIVATE "${CUP_CPPWINRT_DIR}")
        endif()
    endif()
endif()
