find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBVTERM REQUIRED IMPORTED_TARGET vterm)

qt_add_executable(moko-terminal
    src/main.cpp
    src/ptysession.cpp
    src/ptysession.h
    src/terminalview.cpp
    src/terminalview.h
)

qt_add_qml_module(moko-terminal
    URI MokoTerminal
    VERSION 0.1
    NO_PLUGIN
    NO_IMPORT_SCAN
    QML_FILES qml/Main.qml
)

target_include_directories(moko-terminal PRIVATE src ../common)
target_link_libraries(moko-terminal PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    PkgConfig::LIBVTERM
    util
)

install(TARGETS moko-terminal RUNTIME DESTINATION bin)
install(FILES data/org.moko.Terminal.desktop DESTINATION share/applications)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(moko-terminal-pty-tests
        tests/test_ptysession.cpp
        src/ptysession.cpp
        src/ptysession.h
    )
    target_include_directories(moko-terminal-pty-tests PRIVATE src)
    target_link_libraries(moko-terminal-pty-tests PRIVATE Qt6::Core Qt6::Test util)
    add_test(NAME moko-terminal-pty-tests COMMAND moko-terminal-pty-tests)
    set_tests_properties(moko-terminal-pty-tests PROPERTIES TIMEOUT 20)

    add_test(NAME moko-terminal-smoke COMMAND moko-terminal --smoke-test)
    set_tests_properties(moko-terminal-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        TIMEOUT 10
    )
endif()
