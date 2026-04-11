cmake_minimum_required(VERSION 3.20)
project(flick VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network)

qt6_add_resources(QML_RESOURCES src/qml/qml.qrc)

set(MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.pascalwiemers.flick")
set(MACOSX_BUNDLE_BUNDLE_NAME "Flick")
set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})

if(APPLE)
    set(BUNDLE_FLAG MACOSX_BUNDLE)
endif()

add_executable(flick ${BUNDLE_FLAG}
    src/main.cpp
    src/notestore.cpp
    src/mathengine.cpp
    src/autopaste.cpp
    src/syntaxhighlighter.cpp
    src/markdownstyler.cpp
    src/githubsync.cpp
    ${QML_RESOURCES}
)

if(APPLE)
    target_sources(flick PRIVATE src/windowdragger.mm)
    target_link_libraries(flick PRIVATE "-framework AppKit")
else()
    target_sources(flick PRIVATE src/windowdragger_fallback.cpp)
endif()

target_link_libraries(flick PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
)

if(APPLE)
    set_target_properties(flick PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/packaging/Info.plist
    )
endif()

install(TARGETS flick DESTINATION bin)
install(FILES packaging/io.github.pascalwiemers.flick.desktop
        DESTINATION share/applications)
install(FILES packaging/flick.svg
        DESTINATION share/icons/hicolor/scalable/apps)
install(FILES packaging/io.github.pascalwiemers.flick.appdata.xml
        DESTINATION share/metainfo)
