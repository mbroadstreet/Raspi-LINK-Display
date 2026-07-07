cmake_minimum_required(VERSION 3.16)
project(ableton-link-pi-display LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fail early if Ableton Link is not present
set(LINK_DIR "${CMAKE_SOURCE_DIR}/third_party/link")
if(NOT EXISTS "${LINK_DIR}/AbletonLinkConfig.cmake")
    message(FATAL_ERROR "Ableton Link not found. Clone https://github.com/Ableton/link into third_party/link (or use git submodule).")
endif()

list(APPEND CMAKE_MODULE_PATH "${LINK_DIR}")
find_package(AbletonLink REQUIRED)

# SDL2 (required)
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

add_executable(ableton-link-pi-display
    src/main.cpp
    src/LinkEngine.cpp
    src/DisplayText.cpp
    src/SdlRenderer.cpp
)

target_include_directories(ableton-link-pi-display PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(ableton-link-pi-display
    Ableton::Link
    SDL2::SDL2
    SDL2_ttf::SDL2_ttf
)

if(WIN32)
    target_link_libraries(ableton-link-pi-display SDL2::SDL2main)
endif()

install(TARGETS ableton-link-pi-display DESTINATION bin)