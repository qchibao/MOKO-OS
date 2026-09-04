cmake_minimum_required(VERSION 3.21)
project(moko-shell VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.5)

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(moko-shell
    src/main.cpp
)

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

target_link_libraries(moko-shell PRIVATE Qt6::Quick Qt6::QuickControls2)

install(TARGETS moko-shell
    RUNTIME DESTINATION bin
)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME moko-shell-smoke COMMAND moko-shell --smoke-test --windowed)
    set_tests_properties(moko-shell-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        TIMEOUT 10
    )
endif()
