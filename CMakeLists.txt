cmake_minimum_required(VERSION 3.19)

project (
    hkm
    VERSION 0.1
    LANGUAGES
        CXX
        C
)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED)

set(CMAKE_BUILD_TYPE Debug)

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
    message(STATUS "Adding debug symbols to executable")
    add_compile_options(-ggdb)
endif()

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(
        -Wall 
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Wno-unused-value
        -Wno-missing-field-initializers
        -Wno-narrowing
        -Wno-pointer-arith
    )
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(
    deps
    REQUIRED
    IMPORTED_TARGET
    wayland-client
    wayland-protocols
    wayland-cursor
    cairo
    xkbcommon
)
pkg_search_module(GTEST REQUIRED gtest_main)

# use wayland-scanner to generate the necessary code and header files from xml protocol definitions set(WAYLAND_PROTO_DIR "/usr/share/wayland-protocols")
set(WAYLAND_ROOTS_PROTO_DIR "/usr/share/wlr-protocols")
find_program(WL_SCANNER wayland-scanner REQUIRED)

set(PROTOCOL_FILES
    "${WAYLAND_PROTO_DIR}/stable/xdg-shell/xdg-shell.xml"
    "${WAYLAND_PROTO_DIR}/staging/cursor-shape/cursor-shape-v1.xml"
    "${WAYLAND_PROTO_DIR}/unstable/tablet/tablet-unstable-v2.xml"
    "${WAYLAND_PROTO_DIR}/unstable/xdg-output/xdg-output-unstable-v1.xml"
    "${WAYLAND_ROOTS_PROTO_DIR}/unstable/wlr-layer-shell-unstable-v1.xml"
    "${WAYLAND_ROOTS_PROTO_DIR}/unstable/wlr-virtual-pointer-unstable-v1.xml"
)
if(NOT EXISTS "./protocols")
    make_directory("./protocols")

    foreach (protocolFile ${PROTOCOL_FILES})
        get_filename_component(protoBasename ${protocolFile} NAME_WLE) # gets the basename of the file without the extension
        execute_process(
            COMMAND ${WL_SCANNER} client-header 
            INPUT_FILE ${protocolFile}
            OUTPUT_FILE "./protocols/${protoBasename}.h"
        )
        execute_process(
            COMMAND ${WL_SCANNER} private-code 
            INPUT_FILE ${protocolFile}
            OUTPUT_FILE "./protocols/${protoBasename}.c"
        )
    endforeach()
endif()

file(
  GLOB_RECURSE
  HYPRMOUSE_SRC
  "src/*.cpp"
  "protocols/*.c"
)

add_executable(hkm ${HYPRMOUSE_SRC})
target_link_libraries(hkm PRIVATE PkgConfig::deps)
target_include_directories(hkm PRIVATE "./include" "./protocols")

enable_testing()

file(GLOB_RECURSE TEST_FILES "test/*.cpp")
add_executable(hkm_tests ${TEST_FILES})
target_include_directories(hkm_tests PRIVATE "include/")
target_compile_options(hkm_tests PRIVATE ${GTEST_CFLAGS})
target_link_libraries(hkm_tests PRIVATE ${GTEST_LDFLAGS})

include(GoogleTest)
gtest_discover_tests(hkm_tests)
