cmake_minimum_required(VERSION 3.20)
project(flick VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Shared pure-C++ core (no Qt dependency) ──────────────────────
set(CORE_SOURCES
    src/core/core_notestore.cpp
    src/core/core_mathengine.cpp
    src/core/core_listengine.cpp
    src/core/core_statsengine.cpp
)

if(APPLE)
    # ── macOS: native AppKit app (no Qt) ─────────────────────────
    enable_language(OBJCXX)

    set(MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.pascalwiemers.flick")
    set(MACOSX_BUNDLE_BUNDLE_NAME "Flick")
    set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})

    add_executable(flick MACOSX_BUNDLE
        ${CORE_SOURCES}
        src/macos/main.mm
        src/macos/AppDelegate.mm
        src/macos/EditorViewController.mm
        src/macos/GitHubSyncManager.mm
    )

    target_compile_options(flick PRIVATE -fobjc-arc)

    target_link_libraries(flick PRIVATE
        "-framework AppKit"
        "-framework Foundation"
        "-framework QuartzCore"
    )

    set_target_properties(flick PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/packaging/Info.plist
    )

else()
    # ── Linux: Qt frontend with wrappers around core ─────────────
    set(CMAKE_AUTOMOC ON)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network)

    qt6_add_resources(QML_RESOURCES src/qml/qml.qrc)

    add_executable(flick
        ${CORE_SOURCES}
        src/qml/qt_notestore.cpp
        src/qml/qt_mathengine.cpp
        src/qml/qt_listengine.cpp
        src/qml/qt_statsengine.cpp
        src/autopaste.cpp
        src/syntaxhighlighter.cpp
        src/markdownstyler.cpp
        src/githubsync.cpp
        src/windowdragger_fallback.cpp
        src/main.cpp
        ${QML_RESOURCES}
    )

    target_link_libraries(flick PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Network
    )
endif()

install(TARGETS flick DESTINATION bin)
install(FILES packaging/io.github.pascalwiemers.flick.desktop
        DESTINATION share/applications)
install(FILES packaging/flick.svg
        DESTINATION share/icons/hicolor/scalable/apps)
install(FILES packaging/io.github.pascalwiemers.flick.appdata.xml
        DESTINATION share/metainfo)
