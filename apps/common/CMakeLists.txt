if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(moko-file-picker-tests
        tests/test_mokofilepickermodel.cpp
        src/mokofilepickermodel.cpp
        src/mokofilepickermodel.h
    )
    target_include_directories(moko-file-picker-tests PRIVATE src)
    target_link_libraries(moko-file-picker-tests PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME moko-file-picker-tests COMMAND moko-file-picker-tests)
    set_tests_properties(moko-file-picker-tests PROPERTIES TIMEOUT 10)
endif()
