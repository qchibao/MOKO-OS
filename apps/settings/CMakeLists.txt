qt_add_executable(moko-settings
    src/main.cpp
    src/systemsettings.cpp
    src/systemsettings.h
)

qt_add_qml_module(moko-settings
    URI MokoSettings
    VERSION 0.1
    NO_PLUGIN
    NO_IMPORT_SCAN
    QML_FILES qml/Main.qml
)

target_include_directories(moko-settings PRIVATE src ../common)
target_link_libraries(moko-settings PRIVATE Qt6::Core Qt6::DBus Qt6::Gui Qt6::Quick Qt6::QuickControls2)

install(TARGETS moko-settings RUNTIME DESTINATION bin)
install(FILES data/org.moko.Settings.desktop DESTINATION share/applications)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(moko-settings-tests
        tests/test_systemsettings.cpp
        src/systemsettings.cpp
        src/systemsettings.h
    )
    target_include_directories(moko-settings-tests PRIVATE src)
    target_link_libraries(moko-settings-tests PRIVATE Qt6::Core Qt6::DBus Qt6::Gui Qt6::Test)
    add_test(NAME moko-settings-tests COMMAND moko-settings-tests)
    set_tests_properties(moko-settings-tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 20
    )

    add_test(NAME moko-settings-smoke COMMAND moko-settings --smoke-test)
    set_tests_properties(moko-settings-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        TIMEOUT 10
    )
endif()
