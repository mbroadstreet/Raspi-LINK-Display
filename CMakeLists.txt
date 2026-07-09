cmake_minimum_required(VERSION 3.16)

project(ableton_link_pi_display
    VERSION 0.4.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LINK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/link" CACHE PATH "Path to Ableton/link checkout")

if(NOT EXISTS "${LINK_ROOT}/AbletonLinkConfig.cmake")
    message(FATAL_ERROR
        "Ableton/link was not found at ${LINK_ROOT}.\n"
        "Add it with:\n"
        "  git submodule add https://github.com/Ableton/link third_party/link\n"
        "  git submodule update --init --recursive\n"
    )
endif()

include("${LINK_ROOT}/AbletonLinkConfig.cmake")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)

add_executable(link-pi-display
    src/main.cpp
    src/Config.cpp
    src/DisplayText.cpp
    src/LinkEngine.cpp
    src/SdlRenderer.cpp
)

target_include_directories(link-pi-display
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(link-pi-display
    PRIVATE
        Ableton::Link
        PkgConfig::SDL2
        PkgConfig::SDL2_TTF
        pthread
)

target_compile_options(link-pi-display
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

install(TARGETS link-pi-display DESTINATION bin)
