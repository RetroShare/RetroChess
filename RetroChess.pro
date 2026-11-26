!include("../Common/retroshare_plugin.pri"): error("Could not include file ../Common/retroshare_plugin.pri")

# Generated files must never be shared between Qt major versions or build modes.
CONFIG(debug, debug|release) {
    RETROCHESS_BUILD_MODE = debug
} else {
    RETROCHESS_BUILD_MODE = release
}
RETROCHESS_GENERATED_DIR = $$OUT_PWD/temp/qt$${QT_MAJOR_VERSION}/$${RETROCHESS_BUILD_MODE}
OBJECTS_DIR = $$RETROCHESS_GENERATED_DIR/obj
MOC_DIR = $$RETROCHESS_GENERATED_DIR/moc
RCC_DIR = $$RETROCHESS_GENERATED_DIR/qrc
UI_DIR = $$RETROCHESS_GENERATED_DIR/ui

greaterThan(QT_MAJOR_VERSION, 4) {
	# Qt 5 and Qt 6
	QT += widgets
}

exists($$[QMAKE_MKSPECS]/features/mobility.prf) {
  CONFIG += mobility
} else {
  QT += multimedia
}
CONFIG += qt uic qrc resources
MOBILITY = multimedia
DESTDIR = lib
TARGET = RetroChess

DEPENDPATH  += ../../retroshare-gui/src/temp/ui ../../libretroshare/src
INCLUDEPATH += ../../retroshare-gui/src/temp/ui ../../libretroshare/src
INCLUDEPATH += ../../retroshare-gui/src/retroshare-gui

INCLUDEPATH += ../../rapidjson-1.1.0

#################################### Linux ########################################

linux-* {
	#INCLUDEPATH += /usr/include
}

#################################### Windows #####################################

win32 {
	LIBS_DIR = $$PWD/../../../libs
	# Use the toolchain's CRT, matching RetroShare. Tunnel buffers are
	# allocated by the core and freed by this plugin; never force -lucrt
	# into an MSVCRT build.
}

	QMAKE_CXXFLAGS *= -Wall

# RapidJSON 1.1.0 contains an assignment operator that modifies const members.
# GCC 15 checks this eagerly in template bodies, even when it is never used.
greaterThan(QMAKE_GCC_MAJOR_VERSION, 14) {
	QMAKE_CXXFLAGS += -Wno-template-body
}

################################### HEADERS & SOURCES #############################

SOURCES = RetroChessPlugin.cpp               \
          4connect/FourConnect.cpp           \
          backgammon/RetroChessBackgammon.cpp \
          backgammon/core/BgRules.cpp \
          backgammon/core/BgMatch.cpp \
          backgammon/services/BackgammonService.cpp \
          backgammon/gui/BackgammonGameWindow.cpp \
          backgammon/gui/BackgammonSettingsWidget.cpp \
          backgammon/gui/BackgammonSettingsDialog.cpp \
          backgammon/gui/BackgammonResultDialog.cpp \
          backgammon/gui/BackgammonSounds.cpp \
          backgammon/gui/BoardTheme.cpp \
          backgammon/gui/BoardWidget.cpp \
          backgammon/gui/BackgammonSettings.cpp \
          backgammon/gui/toaster/BackgammonToaster.cpp \
          backgammon/gui/toaster/BackgammonToasterNotify.cpp \
          services/p3RetroChess.cc           \
          services/rsRetroChessItems.cc \
          gui/NEMainpage.cpp \
          gui/RetroChessNotify.cpp \
          gui/chess.cpp \
          gui/ChessDebugWidget.cpp \
          gui/RetroChessDebugDialog.cpp \
          gui/ChessSpectatorsWidget.cpp \
          gui/ChessBoard.cpp \
          gui/ChessPosition.cpp \
          gui/RetroChessSessionService.cpp \
          gui/RetroChessLeaderboard.cpp \
          gui/ChessGameHistory.cpp \
          gui/ChessGameReviewDialog.cpp \
          gui/tile.cpp \
          gui/RetroChessChatWidgetHolder.cpp \
          gui/RetroChessSettings.cpp \
          gui/RetroChessUserNotify.cpp \
          gui/toaster/ChessToaster.cpp \
          gui/toaster/RetroChessToasterNotify.cpp \
          gui/ChessClockWidget.cpp \
          gui/ChessGameSetupDialog.cpp

HEADERS = RetroChessPlugin.h                 \
          4connect/FourBoard.h               \
          4connect/FourConnect.h             \
          backgammon/RetroChessBackgammon.h \
          backgammon/core/BgRules.h \
          backgammon/core/BgMatch.h \
          backgammon/services/BackgammonService.h \
          backgammon/services/BackgammonDebug.h \
          backgammon/gui/BackgammonConstants.h \
          backgammon/gui/BackgammonGameWindow.h \
          backgammon/gui/BackgammonSettingsWidget.h \
          backgammon/gui/BackgammonSettingsDialog.h \
          backgammon/gui/BackgammonResultDialog.h \
          backgammon/gui/BackgammonSounds.h \
          backgammon/gui/BackgammonStyle.h \
          backgammon/gui/BoardTheme.h \
          backgammon/gui/BoardWidget.h \
          backgammon/gui/BackgammonSettings.h \
          backgammon/gui/toaster/BackgammonToaster.h \
          backgammon/gui/toaster/BackgammonToasterNotify.h \
          gui/ChessDebugWidget.h \
          gui/RetroChessDebugDialog.h \
          gui/ChessSpectatorsWidget.h \
          gui/ChessBoard.h \
          gui/ChessPosition.h \
          gui/RetroChessSessionService.h \
          gui/RetroChessLeaderboard.h \
          gui/ChessGameHistory.h \
          gui/ChessGameReviewDialog.h \
          services/p3RetroChess.h            \
          services/RetroChessTunnelDebug.h   \
          services/rsRetroChessItems.h       \
          interface/rsRetroChess.h \
          gui/NEMainpage.h \
          gui/RetroChessNotify.h \
          gui/tile.h \
          gui/chess.h \
          gui/RetroChessChatWidgetHolder.h \
          gui/RetroChessSettings.h \
          gui/RetroChessUserNotify.h \
          gui/toaster/ChessToaster.h \
          gui/toaster/RetroChessToasterNotify.h \
          gui/ChessTimeControl.h \
          gui/ChessClockWidget.h \
          gui/ChessGameSetupDialog.h

FORMS += \
          gui/NEMainpage.ui \
          gui/RetroChessDebugDialog.ui \
          gui/chess.ui \
          gui/toaster/ChessToaster.ui \
          4connect/FourResultDialog.ui \
          backgammon/gui/BackgammonGameWindow.ui \
          backgammon/gui/BackgammonSettingsWidget.ui \
          backgammon/gui/BackgammonSettingsDialog.ui \
          backgammon/gui/BackgammonResultDialog.ui \
          backgammon/gui/toaster/BackgammonToaster.ui

RESOURCES = gui/RetroChess_images.qrc
RESOURCES += gui/RetroChess_pieces.qrc
RESOURCES += backgammon/gui/Backgammon_images.qrc
RESOURCES += backgammon/gui/Backgammon_sounds.qrc
RESOURCES += backgammon/gui/Backgammon_qss.qrc

# Backgammon (backgammon/) includes "core/...", "gui/..." and "services/..."
# relative to its own folder, as in the standalone plugin, and needs C++17.
INCLUDEPATH += $$PWD/backgammon
DEPENDPATH  += $$PWD/backgammon
CONFIG += c++17
