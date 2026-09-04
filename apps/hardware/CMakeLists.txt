qt_add_executable(moko-hardware-diagnostics
    src/hardwareprobe.cpp
    src/hardwareprobe.h
    src/main.cpp
)

qt_add_qml_module(moko-hardware-diagnostics
    URI MokoHardwareDiagnostics
    VERSION 0.1
    NO_PLUGIN
    NO_IMPORT_SCAN
    QML_FILES qml/Main.qml
)

target_include_directories(moko-hardware-diagnostics PRIVATE src ../common)
target_link_libraries(moko-hardware-diagnostics PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)

install(TARGETS moko-hardware-diagnostics RUNTIME DESTINATION bin)
install(FILES data/org.moko.HardwareDiagnostics.desktop DESTINATION share/applications)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(moko-hardware-diagnostics-tests
        tests/test_hardwareprobe.cpp
        src/hardwareprobe.cpp
        src/hardwareprobe.h
    )
    target_include_directories(moko-hardware-diagnostics-tests PRIVATE src)
    target_link_libraries(moko-hardware-diagnostics-tests PRIVATE
        Qt6::Core Qt6::DBus Qt6::Gui Qt6::Test
    )
    add_test(NAME moko-hardware-diagnostics-tests COMMAND moko-hardware-diagnostics-tests)
    set_tests_properties(moko-hardware-diagnostics-tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        TIMEOUT 30
    )

    add_test(NAME moko-hardware-diagnostics-smoke
        COMMAND moko-hardware-diagnostics --smoke-test)
    set_tests_properties(moko-hardware-diagnostics-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        TIMEOUT 15
    )
endif()
