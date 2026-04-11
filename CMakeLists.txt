cmake_minimum_required(VERSION 3.20)
project(flick VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network)

qt6_add_resources(QML_RESOURCES src/qml/qml.qrc)

add_executable(flick
    src/main.cpp
    src/notestore.cpp
    src/mathengine.cpp
    src/autopaste.cpp
    src/syntaxhighlighter.cpp
    src/markdownstyler.cpp
    src/githubsync.cpp
    ${QML_RESOURCES}
)

target_link_libraries(flick PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
)

install(TARGETS flick DESTINATION bin)
install(FILES packaging/io.github.pascalwiemers.flick.desktop
        DESTINATION share/applications)
install(FILES packaging/flick.svg
        DESTINATION share/icons/hicolor/scalable/apps)
install(FILES packaging/io.github.pascalwiemers.flick.appdata.xml
        DESTINATION share/metainfo)
