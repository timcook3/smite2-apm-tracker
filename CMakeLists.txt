cmake_minimum_required(VERSION 3.16)
project(smite2_apm_tracker CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "This project targets Windows. Use MinGW to cross-compile on other platforms (see README).")
endif()

add_executable(smite2_apm_tracker WIN32
    src/main.cpp
    src/Config.cpp
    src/GameFocus.cpp
    src/HeatmapWindow.cpp
    src/InputHook.cpp
    src/ApmCalculator.cpp
    src/PingMonitor.cpp
    src/ServerDetector.cpp
    src/OverlayWindow.cpp
)

target_include_directories(smite2_apm_tracker PRIVATE include)
target_compile_definitions(smite2_apm_tracker PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0601)
target_link_libraries(smite2_apm_tracker PRIVATE iphlpapi ws2_32 gdi32 user32 shell32)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(smite2_apm_tracker PRIVATE -Wall -Wextra)
    # -municode selects the wWinMain (wide) entry point.
    target_link_options(smite2_apm_tracker PRIVATE -static -mwindows -municode)
endif()
