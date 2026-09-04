cmake_minimum_required(VERSION 3.21)
project(moko-shell VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS DBus Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.5)

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(moko-shell
    src/applicationiconprovider.cpp
    src/applicationiconprovider.h
    src/applicationregistry.cpp
    src/applicationregistry.h
    src/applicationservice.cpp
    src/applicationservice.h
    src/main.cpp
)

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
        qml/components/MokoGlyph.qml
        qml/components/AppTile.qml
        qml/components/TopBar.qml
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

    qt_add_executable(moko-application-registry-tests
        tests/test_applicationregistry.cpp
        src/applicationregistry.cpp
        src/applicationregistry.h
    )
    target_include_directories(moko-application-registry-tests PRIVATE src)
    target_link_libraries(moko-application-registry-tests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    add_test(NAME moko-application-registry-tests COMMAND moko-application-registry-tests)
    set_tests_properties(moko-application-registry-tests PROPERTIES TIMEOUT 15)
endif()
