cmake_minimum_required(VERSION 3.21)
project(moko-shell VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS DBus Quick QuickControls2)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
qt_standard_project_setup(REQUIRES 6.5)

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
    qt_policy(SET QTP0004 NEW)
endif()

set(MOKO_WINDOW_PROTOCOL_XML
    ${CMAKE_CURRENT_SOURCE_DIR}/../compositor/moko-compositor/protocols/moko-window-control-v1.xml)
set(MOKO_WINDOW_PROTOCOL_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(MOKO_WINDOW_PROTOCOL_CODE
    ${MOKO_WINDOW_PROTOCOL_DIR}/moko-window-control-v1-protocol.c)
set(MOKO_WINDOW_PROTOCOL_HEADER
    ${MOKO_WINDOW_PROTOCOL_DIR}/moko-window-control-v1-client-protocol.h)

add_custom_command(
    OUTPUT ${MOKO_WINDOW_PROTOCOL_CODE} ${MOKO_WINDOW_PROTOCOL_HEADER}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MOKO_WINDOW_PROTOCOL_DIR}
    COMMAND ${WAYLAND_SCANNER} private-code
            ${MOKO_WINDOW_PROTOCOL_XML} ${MOKO_WINDOW_PROTOCOL_CODE}
    COMMAND ${WAYLAND_SCANNER} client-header
            ${MOKO_WINDOW_PROTOCOL_XML} ${MOKO_WINDOW_PROTOCOL_HEADER}
    DEPENDS ${MOKO_WINDOW_PROTOCOL_XML}
    VERBATIM
)

qt_add_executable(moko-shell
    src/applicationiconprovider.cpp
    src/applicationiconprovider.h
    src/applicationregistry.cpp
    src/applicationregistry.h
    src/applicationservice.cpp
    src/applicationservice.h
    src/desktopsettingsservice.cpp
    src/desktopsettingsservice.h
    src/main.cpp
    src/notificationmodel.cpp
    src/notificationmodel.h
    src/powerkeyinhibitor.cpp
    src/powerkeyinhibitor.h
    src/screenshotcontroller.cpp
    src/screenshotcontroller.h
    src/sessionlifecycle.cpp
    src/sessionlifecycle.h
    src/bluetoothagent.cpp
    src/bluetoothagent.h
    src/systemcontrol.cpp
    src/systemcontrol.h
    src/systemcontroltypes.h
    src/systemcontrolutils.cpp
    src/systemcontrolutils.h
    src/windowmanager.cpp
    src/windowmanager.h
    ${MOKO_WINDOW_PROTOCOL_CODE}
    ${MOKO_WINDOW_PROTOCOL_HEADER}
)
target_include_directories(moko-shell PRIVATE ${MOKO_WINDOW_PROTOCOL_DIR})

add_library(moko-ai-ui STATIC
    ../ai/ui/aicontroller.cpp
    ../ai/ui/aicontroller.h
)
target_include_directories(moko-ai-ui PUBLIC ../ai/ui)
target_link_libraries(moko-ai-ui PUBLIC Qt6::Core Qt6::DBus)

qt_add_qml_module(moko-shell
    URI MokoShell
    VERSION 0.1
    NO_PLUGIN
    NO_IMPORT_SCAN
    QML_FILES
        qml/Main.qml
        qml/components/GlassPanel.qml
        qml/components/MokoActionButton.qml
        qml/components/MokoGlyph.qml
        qml/components/MokoSlider.qml
        qml/components/MokoSwitch.qml
        qml/components/StatusIcon.qml
        qml/components/AppTile.qml
        qml/components/TopBar.qml
        qml/components/ControlCenterPanel.qml
        qml/components/NotificationCenterPanel.qml
        qml/components/PowerMenu.qml
        qml/components/LauncherPanel.qml
        qml/components/AiPanel.qml
        qml/components/Dock.qml
    RESOURCES
        assets/wallpaper-desktop.png
)

target_link_libraries(moko-shell PRIVATE
    Qt6::DBus
    Qt6::Quick
    Qt6::QuickControls2
    PkgConfig::WAYLAND_CLIENT
    moko-ai-ui
)

install(TARGETS moko-shell
    RUNTIME DESTINATION bin
)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    add_test(NAME moko-shell-smoke COMMAND moko-shell --smoke-test --windowed)
    set_tests_properties(moko-shell-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        TIMEOUT 10
    )

    add_test(NAME moko-shell-boot-timing
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_shell_boot_timing.sh
                $<TARGET_FILE:moko-shell>)
    set_tests_properties(moko-shell-boot-timing PROPERTIES TIMEOUT 10)

    add_test(NAME moko-shell-power-menu-smoke
        COMMAND moko-shell --smoke-test --windowed --power-menu)
    set_tests_properties(moko-shell-power-menu-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        TIMEOUT 10
    )

    qt_add_executable(moko-application-registry-tests
        tests/test_applicationregistry.cpp
        src/applicationservice.cpp
        src/applicationservice.h
        src/applicationregistry.cpp
        src/applicationregistry.h
    )
    target_include_directories(moko-application-registry-tests PRIVATE src)
    target_link_libraries(moko-application-registry-tests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    add_test(NAME moko-application-registry-tests COMMAND moko-application-registry-tests)
    set_tests_properties(moko-application-registry-tests PROPERTIES TIMEOUT 15)

    qt_add_executable(moko-system-control-tests
        tests/test_systemcontrol.cpp
        src/systemcontrolutils.cpp
        src/systemcontrolutils.h
    )
    target_include_directories(moko-system-control-tests PRIVATE src)
    target_link_libraries(moko-system-control-tests PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME moko-system-control-tests COMMAND moko-system-control-tests)
    set_tests_properties(moko-system-control-tests PROPERTIES TIMEOUT 10)

    qt_add_executable(moko-system-control-backend-tests
        tests/test_systemcontrol_backend.cpp
        src/bluetoothagent.cpp
        src/bluetoothagent.h
        src/systemcontrol.cpp
        src/systemcontrol.h
        src/systemcontrolutils.cpp
        src/systemcontrolutils.h
        src/powerkeyinhibitor.cpp
        src/powerkeyinhibitor.h
    )
    target_include_directories(moko-system-control-backend-tests PRIVATE src)
    target_link_libraries(moko-system-control-backend-tests PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
    qt_add_executable(moko-fake-networkmanager tests/fake_networkmanager.cpp)
    target_include_directories(moko-fake-networkmanager PRIVATE src)
    target_link_libraries(moko-fake-networkmanager PRIVATE Qt6::Core Qt6::DBus)
    qt_add_executable(moko-fake-logind tests/fake_logind.cpp)
    target_link_libraries(moko-fake-logind PRIVATE Qt6::Core Qt6::DBus)
    add_test(NAME moko-system-control-backend-tests
        COMMAND dbus-run-session --
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_systemcontrol_backend_tests.sh
                $<TARGET_FILE:moko-fake-networkmanager>
                $<TARGET_FILE:moko-fake-logind>
                $<TARGET_FILE:moko-system-control-backend-tests>)
    set_tests_properties(moko-system-control-backend-tests PROPERTIES TIMEOUT 15)

    qt_add_executable(moko-notification-model-tests
        tests/test_notificationmodel.cpp
        src/notificationmodel.cpp
        src/notificationmodel.h
    )
    target_include_directories(moko-notification-model-tests PRIVATE src)
    target_link_libraries(moko-notification-model-tests PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME moko-notification-model-tests COMMAND moko-notification-model-tests)
    set_tests_properties(moko-notification-model-tests PROPERTIES TIMEOUT 10)

    qt_add_executable(moko-notification-dbus-tests
        tests/test_notificationdbus.cpp
        src/notificationmodel.cpp
        src/notificationmodel.h
    )
    target_include_directories(moko-notification-dbus-tests PRIVATE src)
    target_link_libraries(moko-notification-dbus-tests PRIVATE
        Qt6::Core Qt6::DBus Qt6::Test)
    add_test(NAME moko-notification-dbus-tests
        COMMAND dbus-run-session -- $<TARGET_FILE:moko-notification-dbus-tests>)
    set_tests_properties(moko-notification-dbus-tests PROPERTIES TIMEOUT 15)

    qt_add_executable(moko-screenshot-controller-tests
        tests/test_screenshotcontroller.cpp
        src/screenshotcontroller.cpp
        src/screenshotcontroller.h
    )
    target_include_directories(moko-screenshot-controller-tests PRIVATE src)
    target_link_libraries(moko-screenshot-controller-tests PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME moko-screenshot-controller-tests COMMAND moko-screenshot-controller-tests)
    set_tests_properties(moko-screenshot-controller-tests PROPERTIES TIMEOUT 10)

    qt_add_executable(moko-session-lifecycle-tests
        tests/test_sessionlifecycle.cpp
        src/sessionlifecycle.cpp
        src/sessionlifecycle.h
    )
    target_include_directories(moko-session-lifecycle-tests PRIVATE src)
    target_link_libraries(moko-session-lifecycle-tests PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
    add_test(NAME moko-session-lifecycle-tests COMMAND moko-session-lifecycle-tests)
    set_tests_properties(moko-session-lifecycle-tests PROPERTIES TIMEOUT 10)
endif()
