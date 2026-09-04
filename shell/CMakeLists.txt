cmake_minimum_required(VERSION 3.21)
project(moko-shell VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(moko-shell
    src/main.cpp
)

qt_add_qml_module(moko-shell
    URI MokoShell
    VERSION 0.1
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
        ../assets/wallpapers/moko-ice-desktop.png
)

target_link_libraries(moko-shell PRIVATE Qt6::Quick)

install(TARGETS moko-shell
    RUNTIME DESTINATION bin
)
