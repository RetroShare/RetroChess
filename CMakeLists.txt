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
    gui/tile.cpp
    gui/validation.cpp
    gui/RetroChessChatWidgetHolder.cpp
    gui/RetroChessSettings.cpp
    gui/RetroChessUserNotify.cpp
    gui/toaster/ChessToaster.cpp
    gui/toaster/RetroChessToasterNotify.cpp
)

set(RETROCHESS_HEADERS
    RetroChessPlugin.h
    services/p3RetroChess.h
    services/rsRetroChessItems.h
    interface/rsRetroChess.h
    gui/NEMainpage.h
    gui/RetroChessNotify.h
    gui/chess.h
    gui/tile.h
    gui/validation.h
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

if(WIN32)
    target_compile_definitions(RetroChess PRIVATE WINDOWS_SYS)
    target_link_libraries(RetroChess PRIVATE retroshare retroshare-gui)
endif()

target_link_libraries(RetroChess PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::Network
)

target_compile_definitions(RetroChess PRIVATE RS_NO_WARN_DEPRECATED)
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
    ${CMAKE_SOURCE_DIR}/libretroshare/src
    ${CMAKE_SOURCE_DIR}/retroshare-gui/src
    ${CMAKE_SOURCE_DIR}/retroshare-gui/src/gui
    ${CMAKE_SOURCE_DIR}/supportlibs/rapidjson/include
)

install(TARGETS RetroChess DESTINATION lib/retroshare/plugins)
