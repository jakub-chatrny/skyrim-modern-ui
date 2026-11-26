# Build files under plugin/ (this file, CMakePresets.json, cmake/) are adapted
# from https://github.com/codepuncher/CommonLibSSE-NG-template (GPL-3.0).
cmake_minimum_required(VERSION 3.21)

project(
    SkyrimModernUI
    VERSION 0.0.1
    DESCRIPTION "Modern character screen for Skyrim SE (Prisma UI data bridge)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib/commonlibsse-ng/CMakeLists.txt")
    message(FATAL_ERROR "lib/commonlibsse-ng is not checked out. Run: git submodule update --init --recursive")
endif()

find_package(spdlog CONFIG REQUIRED)
# directxtk must be found before add_commonlibsse_plugin (CommonLibSSE-NG links
# Microsoft::DirectXTK transitively). rapidcsv must be in vcpkg.json because
# CommonLibSSE-NG unconditionally adds it as an include dir.
find_package(directxtk CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
# stb_image_write, header only: PNG encoding for item pictures.
find_package(Stb REQUIRED)

set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR ON CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SKSE_SUPPORT_XBYAK OFF CACHE BOOL "" FORCE)

# With SE, AE and VR all enabled, CommonLibSSE-NG fetches a prebuilt MSVC-only
# CommonLibSSE.lib on GITHUB_ACTIONS; hide the variable for this call only.
set(_github_actions "$ENV{GITHUB_ACTIONS}")
unset(ENV{GITHUB_ACTIONS})
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/lib/commonlibsse-ng" "${CMAKE_CURRENT_BINARY_DIR}/commonlibsse-ng"
                 EXCLUDE_FROM_ALL)
set(ENV{GITHUB_ACTIONS} "${_github_actions}")

include("${CMAKE_CURRENT_SOURCE_DIR}/lib/commonlibsse-ng/cmake/CommonLibSSE.cmake")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/src")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/src/version.rc" @ONLY)

# Auto-generates SKSEPlugin_Version; the sources only need SKSEPluginLoad.
add_commonlibsse_plugin(
    ${PROJECT_NAME}
    NAME "SkyrimModernUI"
    AUTHOR "kuba"
    SOURCES
        src/Plugin.cpp
        src/CharacterMenu.cpp
        src/CameraStage.cpp
        src/Equipment.cpp
        src/Outfits.cpp
        src/Thumbnails.cpp
        src/Quests.cpp
        src/Snapshot.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/src/version.rc"
    USE_ADDRESS_LIBRARY)

# llvm-rc does not see the /imsvc flags from the cross toolchain.
if(DEFINED XWIN_DIR)
    target_include_directories(
        ${PROJECT_NAME} SYSTEM PRIVATE "$<$<COMPILE_LANGUAGE:RC>:${XWIN_DIR}/sdk/include/um>"
                                       "$<$<COMPILE_LANGUAGE:RC>:${XWIN_DIR}/sdk/include/shared>")
endif()

target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${Stb_INCLUDE_DIR})
# PrismaUI_API.h calls GetModuleHandle with a wide string.
# NOMINMAX: keep the Windows headers from defining min/max macros over std::min/std::max.
target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".dll")

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /Zi)
    target_link_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Release>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>")
endif()
