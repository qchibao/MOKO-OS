cmake_minimum_required(VERSION 3.21)
project(moko-system-apps VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    DBus
    Gui
    Quick
    QuickControls2
    WebEngineCore
    WebEngineQuick
)
qt_standard_project_setup(REQUIRES 6.5)

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
    qt_policy(SET QTP0004 NEW)
endif()

include(CTest)

add_subdirectory(common)
add_subdirectory(files)
add_subdirectory(package-installer)
add_subdirectory(browser)
add_subdirectory(hardware)
add_subdirectory(settings)
add_subdirectory(terminal)
