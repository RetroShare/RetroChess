# plugins/RetroChess/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(RetroChessPlugin)

if(NOT DEFINED QT_VERSION_MAJOR)
    find_package(Qt6 QUIET COMPONENTS Core)
    if(Qt6_FOUND)
        set(QT_VERSION_MAJOR 6)
    else()
        find_package(Qt5 COMPONENTS Core REQUIRED)
        set(QT_VERSION_MAJOR 5)
    endif()
endif()
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Widgets Multimedia Network REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(RETROCHESS_SOURCES
    RetroChessPlugin.cpp
    services/p3RetroChess.cc
    services/rsRetroChessItems.cc
    gui/NEMainpage.cpp
    gui/RetroChessNotify.cpp
    gui/chess.cpp
    gui/ChessDebugWidget.cpp
    gui/ChessTrafficDialog.cpp
    gui/ChessSpectatorsWidget.cpp
    gui/ChessBoard.cpp
    gui/ChessPosition.cpp
    gui/RetroChessSessionService.cpp
    gui/RetroChessLeaderboard.cpp
    gui/ChessGameHistory.cpp
    gui/ChessGameReviewDialog.cpp
    gui/tile.cpp

    gui/RetroChessChatWidgetHolder.cpp
    gui/RetroChessSettings.cpp
    gui/RetroChessUserNotify.cpp
    gui/toaster/ChessToaster.cpp
    gui/toaster/RetroChessToasterNotify.cpp
    gui/ChessClockWidget.cpp
    gui/ChessGameSetupDialog.cpp
)

set(RETROCHESS_HEADERS
    RetroChessPlugin.h
    services/p3RetroChess.h
    services/RetroChessTunnelDebug.h
    services/rsRetroChessItems.h
    interface/rsRetroChess.h
    gui/NEMainpage.h
    gui/RetroChessNotify.h
    gui/chess.h
    gui/ChessDebugWidget.h
    gui/ChessTrafficDialog.h
    gui/ChessSpectatorsWidget.h
    gui/ChessBoard.h
    gui/ChessPosition.h
    gui/RetroChessSessionService.h
    gui/RetroChessLeaderboard.h
    gui/ChessGameHistory.h
    gui/ChessGameReviewDialog.h
    gui/tile.h
    gui/ChessTimeControl.h
    gui/ChessClockWidget.h
    gui/ChessGameSetupDialog.h

    gui/RetroChessChatWidgetHolder.h
    gui/RetroChessSettings.h
    gui/RetroChessUserNotify.h
    gui/toaster/ChessToaster.h
    gui/toaster/RetroChessToasterNotify.h
)

set(RETROCHESS_FORMS
    gui/NEMainpage.ui
    gui/chess.ui
    gui/toaster/ChessToaster.ui
)

set(RETROCHESS_RESOURCES
    gui/RetroChess_images.qrc
    gui/RetroChess_pieces.qrc
)

add_library(RetroChess MODULE
    ${RETROCHESS_SOURCES}
    ${RETROCHESS_HEADERS}
    ${RETROCHESS_FORMS}
    ${RETROCHESS_RESOURCES}
)

set_target_properties(RetroChess PROPERTIES
    PREFIX "lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/gui;${CMAKE_CURRENT_SOURCE_DIR}/gui/toaster"
)

# Resolve the RetroShare super-project root so this file works both from the
# super-project (add_subdirectory) and standalone (cmake run in this dir),
# as long as the plugin lives in <root>/plugins/RetroChess.
get_filename_component(RS_SUPERPROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

if(WIN32)
    target_compile_definitions(RetroChess PRIVATE WINDOWS_SYS)

    # A Windows DLL has to resolve every symbol at link time, so unlike the ELF
    # builds this plugin must link against RetroShare itself. In the super-project
    # the CMake targets are there; standalone they are not, and this file then did
    # not even configure. Fall back to the import libraries of a RetroShare build
    # tree: MinGW writes libretroshare.dll.a next to libretroshare.dll, and
    # libretroshare-gui.dll.a for the GUI executable, which carries ENABLE_EXPORTS.
    if(TARGET retroshare AND TARGET retroshare-gui)
        target_link_libraries(RetroChess PRIVATE retroshare retroshare-gui)
    else()
        set(RS_BUILD_DIR "${RS_SUPERPROJECT_ROOT}/Build-cmake" CACHE PATH
            "RetroShare build directory to link against when building standalone")

        find_library(RS_LIBRETROSHARE_IMPLIB NAMES retroshare
                     HINTS "${RS_BUILD_DIR}/libretroshare" NO_DEFAULT_PATH)
        find_library(RS_GUI_IMPLIB NAMES retroshare-gui
                     HINTS "${RS_BUILD_DIR}/retroshare-gui" NO_DEFAULT_PATH)

        if(NOT RS_LIBRETROSHARE_IMPLIB OR NOT RS_GUI_IMPLIB)
            message(FATAL_ERROR
                "Standalone Windows build needs the import libraries of a built "
                "RetroShare. Looked in ${RS_BUILD_DIR} for libretroshare.dll.a and "
                "libretroshare-gui.dll.a. Build RetroShare first, or point "
                "-DRS_BUILD_DIR=<its build directory> at it.")
        endif()

        message(STATUS "RetroChess: standalone Windows link against ${RS_BUILD_DIR}")
        target_link_libraries(RetroChess PRIVATE
            "${RS_LIBRETROSHARE_IMPLIB}" "${RS_GUI_IMPLIB}")
    endif()
endif()

target_link_libraries(RetroChess PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::Network
)

target_compile_definitions(RetroChess PRIVATE RS_NO_WARN_DEPRECATED)

# Must match the host build: RS_USE_CALENDAR adds a member to RsPlugInInterfaces
# (retroshare/rsplugin.h) before mGxsTunnels, so a plugin built without it reads
# the wrong pointer and crashes at startup. ON by default, like libretroshare.
option(RS_USE_CALENDAR "Build against a RetroShare with the GXS Calendar service (default in CMake builds)" ON)
if(RS_USE_CALENDAR)
    target_compile_definitions(RetroChess PRIVATE RS_USE_CALENDAR)
endif()
target_compile_options(RetroChess PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall;-Wextra;-Wno-deprecated-declarations>
    $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,15>>:-Wno-template-body>
)

if(RS_DEVELOPMENT_BUILD)
    target_compile_options(RetroChess PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-g;-O0>
    )
endif()


target_include_directories(RetroChess PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/gui
    ${CMAKE_CURRENT_SOURCE_DIR}/services
    ${CMAKE_CURRENT_SOURCE_DIR}/interface
    ${RS_SUPERPROJECT_ROOT}/libretroshare/src
    ${RS_SUPERPROJECT_ROOT}/retroshare-gui/src
    ${RS_SUPERPROJECT_ROOT}/retroshare-gui/src/gui
    ${RS_SUPERPROJECT_ROOT}/supportlibs/rapidjson/include
)

install(TARGETS RetroChess DESTINATION lib/retroshare/plugins)
