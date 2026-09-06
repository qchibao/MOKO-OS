qt_add_executable(moko-browser
    src/browsercontroller.cpp
    src/browsercontroller.h
    src/browserdownloadmodel.cpp
    src/browserdownloadmodel.h
    src/browserhistorymodel.cpp
    src/browserhistorymodel.h
    src/browserurl.cpp
    src/browserurl.h
    src/main.cpp
    ../common/src/mokofilepickermodel.cpp
    ../common/src/mokofilepickermodel.h
)

set(MOKO_FILE_DIALOG_QML ${CMAKE_CURRENT_SOURCE_DIR}/../common/qml/MokoFileDialog.qml)
set_source_files_properties(${MOKO_FILE_DIALOG_QML} PROPERTIES
    QT_RESOURCE_ALIAS MokoFileDialog.qml
)

qt_add_qml_module(moko-browser
    URI MokoBrowser
    VERSION 0.1
    NO_PLUGIN
    NO_IMPORT_SCAN
    QML_FILES qml/Main.qml ${MOKO_FILE_DIALOG_QML}
)

target_include_directories(moko-browser PRIVATE src ../common ../common/src)
target_link_libraries(moko-browser PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::WebEngineCore
    Qt6::WebEngineQuick
)

install(TARGETS moko-browser RUNTIME DESTINATION bin)
install(FILES data/org.moko.Browser.desktop DESTINATION share/applications)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(moko-browser-tests
        tests/test_browser.cpp
        src/browserdownloadmodel.cpp
        src/browserdownloadmodel.h
        src/browserhistorymodel.cpp
        src/browserhistorymodel.h
        src/browserurl.cpp
        src/browserurl.h
    )
    target_include_directories(moko-browser-tests PRIVATE src ../common)
    target_link_libraries(moko-browser-tests PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
        Qt6::WebEngineCore
    )
    set_target_properties(moko-browser-tests PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
    add_test(NAME moko-browser-tests COMMAND moko-browser-tests)
    set_tests_properties(moko-browser-tests PROPERTIES TIMEOUT 20)

    add_test(NAME moko-browser-smoke
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_browser_smoke.sh
                $<TARGET_FILE:moko-browser>
    )
    set_tests_properties(moko-browser-smoke PROPERTIES TIMEOUT 30)
endif()
