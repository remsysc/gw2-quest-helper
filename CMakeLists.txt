cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" GW2QH_RAW_VERSION LIMIT_COUNT 1)
string(STRIP "${GW2QH_RAW_VERSION}" GW2QH_CLEAN_VERSION)

project(GW2QuestHelper VERSION ${GW2QH_CLEAN_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GW2QH_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
    "${GW2QH_GENERATED_DIR}/version.h"
    @ONLY
)

add_library(gw2qh_core STATIC
    src/gw2_logic.cpp
    src/tracker_service.cpp
)

target_include_directories(gw2qh_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GW2QH_GENERATED_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

if(WIN32)
    target_compile_definitions(gw2qh_core PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)

    add_library(GW2QuestHelper SHARED
        src/entry.cpp
        src/http_client_winhttp.cpp
        src/ui_renderer.cpp
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp
    )

    target_include_directories(GW2QuestHelper PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${GW2QH_GENERATED_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nexus
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mumble
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    )

    target_compile_definitions(GW2QuestHelper PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(GW2QuestHelper PRIVATE -Wno-nontrivial-memcall)
    endif()

    target_link_libraries(GW2QuestHelper PRIVATE
        gw2qh_core
        winhttp
        shell32
        user32
        kernel32
    )

    set_target_properties(GW2QuestHelper PROPERTIES
        PREFIX ""
        OUTPUT_NAME "GW2QuestHelper"
        SUFFIX ".dll"
    )
else()
    enable_testing()
    add_executable(gw2qh_tests
        tests/test_gw2_logic.cpp
    )
    target_link_libraries(gw2qh_tests PRIVATE gw2qh_core pthread)
    add_test(NAME gw2qh_tests COMMAND gw2qh_tests)
endif()
