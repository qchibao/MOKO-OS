cmake_minimum_required(VERSION 3.21)
project(moko-compositor VERSION 0.1.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WLROOTS REQUIRED IMPORTED_TARGET wlroots-0.18)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

set(PROTOCOL_XML ${CMAKE_CURRENT_SOURCE_DIR}/protocols/moko-window-control-v1.xml)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(PROTOCOL_CODE ${GENERATED_DIR}/moko-window-control-v1-protocol.c)
set(PROTOCOL_SERVER_HEADER ${GENERATED_DIR}/moko-window-control-v1-server-protocol.h)
set(PROTOCOL_CLIENT_HEADER ${GENERATED_DIR}/moko-window-control-v1-client-protocol.h)
set(XDG_SHELL_XML ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml)
set(XDG_SHELL_HEADER ${GENERATED_DIR}/xdg-shell-protocol.h)
set(XDG_SHELL_CLIENT_HEADER ${GENERATED_DIR}/xdg-shell-client-protocol.h)
set(XDG_SHELL_CODE ${GENERATED_DIR}/xdg-shell-protocol.c)

add_custom_command(
    OUTPUT ${PROTOCOL_CODE} ${PROTOCOL_SERVER_HEADER} ${PROTOCOL_CLIENT_HEADER}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}
    COMMAND ${WAYLAND_SCANNER} private-code ${PROTOCOL_XML} ${PROTOCOL_CODE}
    COMMAND ${WAYLAND_SCANNER} server-header ${PROTOCOL_XML} ${PROTOCOL_SERVER_HEADER}
    COMMAND ${WAYLAND_SCANNER} client-header ${PROTOCOL_XML} ${PROTOCOL_CLIENT_HEADER}
    DEPENDS ${PROTOCOL_XML}
    VERBATIM
)

add_custom_command(
    OUTPUT ${XDG_SHELL_HEADER} ${XDG_SHELL_CLIENT_HEADER} ${XDG_SHELL_CODE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}
    COMMAND ${WAYLAND_SCANNER} server-header ${XDG_SHELL_XML} ${XDG_SHELL_HEADER}
    COMMAND ${WAYLAND_SCANNER} client-header ${XDG_SHELL_XML} ${XDG_SHELL_CLIENT_HEADER}
    COMMAND ${WAYLAND_SCANNER} private-code ${XDG_SHELL_XML} ${XDG_SHELL_CODE}
    DEPENDS ${XDG_SHELL_XML}
    VERBATIM
)

add_library(moko-window-geometry STATIC
    src/window_geometry.c
    src/window_geometry.h
)
target_include_directories(moko-window-geometry PUBLIC src)

add_executable(moko-compositor
    src/main.c
    ${PROTOCOL_CODE}
    ${PROTOCOL_SERVER_HEADER}
    ${XDG_SHELL_HEADER}
)
target_compile_definitions(moko-compositor PRIVATE WLR_USE_UNSTABLE)
target_compile_options(moko-compositor PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(moko-compositor PRIVATE ${GENERATED_DIR})
target_link_libraries(moko-compositor PRIVATE
    moko-window-geometry
    PkgConfig::WLROOTS
    PkgConfig::WAYLAND_SERVER
    PkgConfig::XKBCOMMON
    m
)

install(TARGETS moko-compositor RUNTIME DESTINATION bin)
install(FILES ${PROTOCOL_XML} DESTINATION share/moko/protocols)

include(CTest)
if(BUILD_TESTING)
    add_executable(moko-window-geometry-tests tests/test_window_geometry.c)
    target_link_libraries(moko-window-geometry-tests PRIVATE moko-window-geometry)
    add_test(NAME moko-window-geometry-tests COMMAND moko-window-geometry-tests)

    add_executable(moko-compositor-integration-client
        tests/integration_client.c
        ${PROTOCOL_CODE}
        ${PROTOCOL_CLIENT_HEADER}
        ${XDG_SHELL_CODE}
        ${XDG_SHELL_CLIENT_HEADER}
    )
    target_compile_options(moko-compositor-integration-client PRIVATE -Wall -Wextra -Wpedantic)
    target_include_directories(moko-compositor-integration-client PRIVATE ${GENERATED_DIR})
    target_link_libraries(moko-compositor-integration-client PRIVATE PkgConfig::WAYLAND_CLIENT)

    add_test(
        NAME moko-compositor-headless-integration
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_headless_compositor.sh
                $<TARGET_FILE:moko-compositor>
                $<TARGET_FILE:moko-compositor-integration-client>
    )
    set_tests_properties(moko-compositor-headless-integration PROPERTIES TIMEOUT 20)

    add_test(
        NAME moko-desktop-session-backend-selection
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_desktop_session.sh
                ${CMAKE_CURRENT_SOURCE_DIR}/../../core/moko-session/moko-desktop-session
                $<TARGET_FILE:moko-compositor>
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/mock_desktop_client.sh
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/mock_compositor.sh
    )
    set_tests_properties(moko-desktop-session-backend-selection PROPERTIES TIMEOUT 20)
endif()
