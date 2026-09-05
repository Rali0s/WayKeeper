cmake_minimum_required(VERSION 3.21)

project(fallout_assistant VERSION 0.1.0 LANGUAGES CXX)

option(OFFGRID_BUILD_TESTS "Build the Phase-A tests" ON)
option(WAYKEEPER_ENABLE_OLLAMA "Enable the legacy local Ollama adapter" OFF)
option(WAYKEEPER_APPLIANCE_MODE "Prevent the embedded UI from exiting to a Linux shell" OFF)

find_package(GDAL QUIET)
find_package(SQLite3 QUIET)
add_library(offgrid_core
    src/energy.cpp
    src/guide.cpp
    src/herbs.cpp
    src/image.cpp
    src/inventory.cpp
    src/io_scout.cpp
    src/journal.cpp
    src/library.cpp
    src/map.cpp
    src/map_annotations.cpp
    src/mesh.cpp
    src/network.cpp
    src/ollama.cpp
    src/profile.cpp
    src/schematics.cpp
    src/sentinel.cpp
    src/settings.cpp
    src/telemetry.cpp
    src/terminal.cpp
    src/ui.cpp
    src/unlocks.cpp
)

target_include_directories(offgrid_core PUBLIC include)
target_compile_features(offgrid_core PUBLIC cxx_std_20)
target_compile_definitions(offgrid_core PRIVATE OFFGRID_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(offgrid_core PUBLIC
    WAYKEEPER_ENABLE_OLLAMA=$<BOOL:${WAYKEEPER_ENABLE_OLLAMA}>
    WAYKEEPER_APPLIANCE_MODE=$<BOOL:${WAYKEEPER_APPLIANCE_MODE}>)

if(WIN32)
    target_link_libraries(offgrid_core PUBLIC ws2_32)
endif()

if(GDAL_FOUND)
    target_link_libraries(offgrid_core PUBLIC GDAL::GDAL)
    target_compile_definitions(offgrid_core PUBLIC OFFGRID_HAVE_GDAL=1)
endif()

if(SQLite3_FOUND)
    target_link_libraries(offgrid_core PUBLIC SQLite3::SQLite3)
    target_compile_definitions(offgrid_core PUBLIC OFFGRID_HAVE_SQLITE3=1)
endif()

if(MSVC)
    target_compile_options(offgrid_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(offgrid_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(offgrid-assistant src/main.cpp)
target_link_libraries(offgrid-assistant PRIVATE offgrid_core)
if(OFFGRID_BUILD_TESTS)
    enable_testing()
    add_executable(offgrid-tests tests/test_core.cpp)
    target_link_libraries(offgrid-tests PRIVATE offgrid_core)
    add_test(NAME offgrid-core COMMAND offgrid-tests)
endif()
