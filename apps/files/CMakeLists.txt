qt_add_executable(moko-files
    src/filemodel.cpp
    src/filemodel.h
    src/main.cpp
)

qt_add_qml_module(moko-files
    URI MokoFiles
    VERSION 0.1
    NO_PLUGIN
    NO_IMPORT_SCAN
    QML_FILES qml/Main.qml
)

target_include_directories(moko-files PRIVATE src ../common)
target_link_libraries(moko-files PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2)

install(TARGETS moko-files RUNTIME DESTINATION bin)
install(FILES data/org.moko.Files.desktop DESTINATION share/applications)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(moko-files-tests
        tests/test_filemodel.cpp
        src/filemodel.cpp
        src/filemodel.h
    )
    target_include_directories(moko-files-tests PRIVATE src ../common)
    target_link_libraries(moko-files-tests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    add_test(NAME moko-files-tests COMMAND moko-files-tests)
    set_tests_properties(moko-files-tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 20
    )

    add_test(NAME moko-files-smoke COMMAND moko-files --smoke-test)
    set_tests_properties(moko-files-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        TIMEOUT 10
    )
endif()
