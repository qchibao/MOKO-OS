qt_add_executable(moko-package-installer
    src/main.cpp
    src/packageinstaller.cpp
    src/packageinstaller.h
)

qt_add_qml_module(moko-package-installer
    URI MokoPackageInstaller
    VERSION 0.1
    NO_PLUGIN
    NO_IMPORT_SCAN
    QML_FILES qml/Main.qml
)

target_include_directories(moko-package-installer PRIVATE src ../common)
target_link_libraries(moko-package-installer PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)

install(TARGETS moko-package-installer RUNTIME DESTINATION bin)
install(PROGRAMS data/moko-package-install-helper DESTINATION libexec)
install(FILES data/org.moko.PackageInstaller.desktop DESTINATION share/applications)
install(FILES data/org.moko.package-installer.policy DESTINATION /usr/share/polkit-1/actions)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(moko-package-installer-tests
        tests/test_packageinstaller.cpp
        src/packageinstaller.cpp
        src/packageinstaller.h
    )
    target_include_directories(moko-package-installer-tests PRIVATE src ../common)
    target_link_libraries(moko-package-installer-tests PRIVATE
        Qt6::Core Qt6::DBus Qt6::Gui Qt6::Test
    )
    add_test(NAME moko-package-installer-tests COMMAND moko-package-installer-tests)
    set_tests_properties(moko-package-installer-tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30
    )

    add_test(NAME moko-package-installer-smoke COMMAND moko-package-installer --smoke-test)
    set_tests_properties(moko-package-installer-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        TIMEOUT 10
    )

    add_test(NAME moko-package-helper-safety
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_helper_safety.sh
                ${CMAKE_CURRENT_SOURCE_DIR}/data/moko-package-install-helper)
    set_tests_properties(moko-package-helper-safety PROPERTIES TIMEOUT 10)
endif()
