# Specify the minimum version of CMake 
cmake_minimum_required(VERSION 3.5)

# Specify the project name
project(ppm VERSION 1.0)

set(MOC_HEADERS
    mywindow.h
    workflow.h
    usersettingspopup.h
    actionconfirmationdialog.h
    googlegateway.h
    googlechatgateway.h
    retakedialog.h
    usersettings.h
    projectsettingsdialog.h
    projectsettingsdialog_flowchart.h
    flowchartscene.h
    mainflowchartscene.h
    sceneknowledgepanel.h
    registerscenedialog.h
    aboutpopup.h
    )

set(HEADERS
    ${MOC_HEADERS}
    configurationpaths.h
    platform.h
    workflowtypes.h
    renamescript.h
    sceneinfotag.h
    projectsettings_common.h
)

set(SOURCES
main.cpp
configurationpaths.cpp
platform.cpp
mywindow.cpp
workflow.cpp
usersettingspopup.cpp
actionconfirmationdialog.cpp
googlegateway.cpp
googlechatgateway.cpp
renamescript.cpp
retakedialog.cpp
usersettings.cpp
sceneinfotag.cpp
projectsettingsdialog.cpp
projectsettings_common.cpp
projectsettingsdialog_flowchart.cpp
flowchartscene.cpp
mainflowchartscene.cpp
sceneknowledgepanel.cpp
registerscenedialog.cpp
aboutpopup.cpp
)

#-----------------------------------------------------------------------------
# Third Party & System Libs
# specify qtdir
    set(QTDIR $ENV{QTDIR} CACHE PATH "Qt installation directory")
    if(NOT EXISTS ${QTDIR})
        message("Specify QTDIR properly")
        return()
    endif()
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${QTDIR};${QTDIR}/lib/cmake")

# Add Qt libraries. QtCore and QtGui libraries will be 
# added accordingly as the Widgets depends on them. 
find_package( Qt6 REQUIRED
    Widgets
    LinguistTools
    Network
    Core5Compat
    Concurrent
    Qml
)

# ビルドディレクトリを include ディレクトリに追加するオプションを有効にする。
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(MOC_SOURCES)

# Resource file(s)
set(RESOURCES ppm_resources.qrc)

if(WIN32)
    set(RC_FILE ppm.rc)
endif()

qt6_add_resources(SOURCES ${RESOURCES})

# generate Qt translations and messages
set(LANGUAGES ja)
# 翻訳対象のファイルを設定する。
SET(TRANSLATION_TARGET_FILES
    translations/macos_menu.cpp
    ${HEADERS}
    ${SOURCES}
)

set(translation)
foreach(lang ${LANGUAGES})
    set(name "${CMAKE_SOURCE_DIR}/translations/PPM_${lang}.ts")
    list(APPEND translation ${name})
    set_source_files_properties(${name} PROPERTIES
        OUTPUT_LOCATION "${CMAKE_SOURCE_DIR}/../stuff/loc"
    )
endforeach()

qt6_create_translation(message ${translation} ${TRANSLATION_TARGET_FILES})

add_custom_target("translation_PPM" DEPENDS ${message})
set_target_properties("translation_PPM" PROPERTIES
    EXCLUDE_FROM_DEFAULT_BUILD TRUE
)



qt6_wrap_cpp(MOC_SOURCES ${MOC_HEADERS})

set(SOURCES
    ${SOURCES}
    ${MOC_SOURCES}
)


# 実行ファイルを追加する。
#add_executable(${PROJECT_NAME} WIN32 ${HEADERS} ${SOURCES} ${RC_FILE})
add_executable(${PROJECT_NAME} WIN32 ${HEADERS} ${SOURCES} ${RESOURCES} ${RC_FILE})

if(APPLE)
    set_source_files_properties(macos/ppm.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(${PROJECT_NAME} PRIVATE macos/ppm.icns)
    # The UI uses SVG icons. Resolve plugins from the same Qt installation as
    # the linked libraries, including when a global plugin directory exists.
    find_package(Qt6 REQUIRED COMPONENTS Svg)
    target_link_libraries(${PROJECT_NAME} Qt6::Svg)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "PPM"
        MACOSX_BUNDLE_BUNDLE_NAME "PPM"
        MACOSX_BUNDLE_ICON_FILE "ppm.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.opentoonz.ppm"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ppm-qt-$<CONFIG>.conf"
        CONTENT "[Paths]\nPlugins=$<TARGET_FILE_DIR:Qt6::QCocoaIntegrationPlugin>/..\n")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/../Resources"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/ppm-qt-$<CONFIG>.conf"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/../Resources/qt.conf"
        VERBATIM
    )
endif()

# バージョン番号は、上の project() の VERSION だけで管理する（「PPM について」に表示される）
target_compile_definitions(${PROJECT_NAME} PRIVATE PPM_VERSION="${PROJECT_VERSION}")

# PPM のライセンスの正本は、リポジトリ直下の LICENSE。
# 「PPM について」と配布物で使うため、Stuff フォルダの LICENSE フォルダにコピーする
# （LICENSE を書き換えると、次のビルドのときに CMake が再実行されてコピーし直される）
configure_file(${CMAKE_SOURCE_DIR}/../LICENSE
               ${CMAKE_SOURCE_DIR}/../stuff/LICENSE/LICENSE.txt COPYONLY)

# link Qt library. Core and Gui will be linked accordingly
target_link_libraries(${PROJECT_NAME} Qt6::Widgets Qt6::Network Qt6::Core5Compat Qt6::Concurrent Qt6::Qml)

add_definitions(
    -DNOMINMAX
    -D_USE_MATH_DEFINES
)

# qt_allow_non_utf8_sources(${PROJECT_NAME})

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# Set a default build type to Release
if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    endif()
endif()

# Focused behavior tests are opt-in for developer builds.
option(PPM_BUILD_TESTS "Build application behavior tests" OFF)
if(PPM_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(ppm_configuration_tests
        tests/configurationpaths_test.cpp configurationpaths.cpp)
    set_target_properties(ppm_configuration_tests PROPERTIES AUTOMOC ON)
    target_compile_options(ppm_configuration_tests PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    target_link_libraries(ppm_configuration_tests PRIVATE Qt6::Test)
    add_test(NAME ppm_configuration_tests COMMAND ppm_configuration_tests)
    add_executable(ppm_sceneinfotag_tests tests/sceneinfotag_test.cpp sceneinfotag.cpp)
    set_target_properties(ppm_sceneinfotag_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ppm_sceneinfotag_tests PRIVATE Qt6::Test Qt6::Gui)
    add_test(NAME ppm_sceneinfotag_tests COMMAND ppm_sceneinfotag_tests)
    add_executable(ppm_confirmation_tests tests/confirmationdialog_test.cpp
        actionconfirmationdialog.cpp workflow.cpp usersettings.cpp configurationpaths.cpp platform.cpp
        sceneinfotag.cpp renamescript.cpp googlegateway.cpp googlechatgateway.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/moc_actionconfirmationdialog.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/moc_workflow.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/moc_googlegateway.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/moc_googlechatgateway.cpp)
    add_dependencies(ppm_confirmation_tests ppm)
    set_target_properties(ppm_confirmation_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ppm_confirmation_tests PRIVATE Qt6::Test Qt6::Widgets
        Qt6::Network Qt6::Core5Compat Qt6::Qml)
    add_test(NAME ppm_confirmation_tests COMMAND ppm_confirmation_tests)
    set_tests_properties(ppm_confirmation_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
    if(APPLE)
        add_executable(ppm_macos_operations_tests tests/macos_operations_test.cpp platform.cpp)
        set_target_properties(ppm_macos_operations_tests PROPERTIES AUTOMOC ON)
        target_link_libraries(ppm_macos_operations_tests PRIVATE Qt6::Test)
        add_test(NAME ppm_macos_operations_tests COMMAND ppm_macos_operations_tests)
    endif()
endif()
