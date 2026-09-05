cmake_minimum_required(VERSION 3.21)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(moko-ai VERSION 0.1.0 LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Core DBus)
    qt_standard_project_setup(REQUIRES 6.5)
    include(CTest)
endif()

add_library(moko-ai-actions STATIC
    actions/aiactions.cpp
    actions/aiactions.h
)
target_include_directories(moko-ai-actions PUBLIC actions)
target_link_libraries(moko-ai-actions PUBLIC Qt6::Core Qt6::DBus)

add_library(moko-ai-providers STATIC
    providers/aiprovider.h
    providers/localstubprovider.cpp
    providers/localstubprovider.h
    providers/remoteapiprovider.h
)
target_include_directories(moko-ai-providers PUBLIC providers)
target_link_libraries(moko-ai-providers PUBLIC Qt6::Core)

add_library(moko-ai-ui STATIC
    ui/aicontroller.cpp
    ui/aicontroller.h
)
target_include_directories(moko-ai-ui PUBLIC ui)
target_link_libraries(moko-ai-ui PUBLIC Qt6::Core Qt6::DBus)

qt_add_executable(moko-ai-daemon
    daemon/aidaemon.cpp
    daemon/aidaemon.h
    daemon/main.cpp
)
target_include_directories(moko-ai-daemon PRIVATE daemon)
target_link_libraries(moko-ai-daemon PRIVATE
    Qt6::Core
    Qt6::DBus
    moko-ai-actions
    moko-ai-providers
)

install(TARGETS moko-ai-daemon RUNTIME DESTINATION bin)
install(FILES daemon/data/org.moko.AI1.xml DESTINATION share/dbus-1/interfaces)
install(FILES daemon/data/org.moko.AI1.service DESTINATION share/dbus-1/services)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(moko-ai-tests
        tests/test_ai.cpp
        daemon/aidaemon.cpp
        daemon/aidaemon.h
    )
    target_include_directories(moko-ai-tests PRIVATE daemon actions providers)
    target_link_libraries(moko-ai-tests PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
        moko-ai-actions
        moko-ai-providers
    )
    add_test(NAME moko-ai-tests COMMAND moko-ai-tests)
    set_tests_properties(moko-ai-tests PROPERTIES TIMEOUT 20)

    add_test(NAME moko-ai-daemon-smoke
        COMMAND dbus-run-session -- $<TARGET_FILE:moko-ai-daemon> --smoke-test)
    set_tests_properties(moko-ai-daemon-smoke PROPERTIES TIMEOUT 10)

    qt_add_executable(moko-ai-dbus-tests tests/test_dbus.cpp)
    target_link_libraries(moko-ai-dbus-tests PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
        moko-ai-ui
    )
    add_test(NAME moko-ai-dbus-tests
        COMMAND ${CMAKE_COMMAND} -E env
                MOKO_AI_DAEMON_PATH=$<TARGET_FILE:moko-ai-daemon>
                dbus-run-session -- $<TARGET_FILE:moko-ai-dbus-tests>)
    set_tests_properties(moko-ai-dbus-tests PROPERTIES TIMEOUT 10)
endif()
