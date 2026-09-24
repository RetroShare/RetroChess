!include("../Common/retroshare_plugin.pri"): error("Could not include file ../Common/retroshare_plugin.pri")

greaterThan(QT_MAJOR_VERSION, 4) {
	# Qt 5
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
          services/p3RetroChess.cc           \
          services/rsRetroChessItems.cc \
          gui/NEMainpage.cpp \
          gui/RetroChessNotify.cpp \
          gui/chess.cpp \
          gui/ChessDebugWidget.cpp \
          gui/ChessTrafficDialog.cpp \
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
          gui/RetroChessFlair.cpp \
          gui/RetroChessUserNotify.cpp \
          gui/toaster/ChessToaster.cpp \
          gui/toaster/RetroChessToasterNotify.cpp \
          gui/ChessClockWidget.cpp \
          gui/ChessGameSetupDialog.cpp

HEADERS = RetroChessPlugin.h                 \
          gui/ChessDebugWidget.h \
          gui/ChessTrafficDialog.h \
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
          gui/RetroChessFlair.h \
          gui/RetroChessUserNotify.h \
          gui/toaster/ChessToaster.h \
          gui/toaster/RetroChessToasterNotify.h \
          gui/ChessTimeControl.h \
          gui/ChessClockWidget.h \
          gui/ChessGameSetupDialog.h

FORMS += \
          gui/NEMainpage.ui \
          gui/chess.ui \
          gui/toaster/ChessToaster.ui

RESOURCES = gui/RetroChess_images.qrc
RESOURCES += gui/RetroChess_pieces.qrc
RESOURCES += gui/RetroChess_flair.qrc
