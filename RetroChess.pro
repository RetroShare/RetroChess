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

DEPENDPATH += ../../retroshare-gui/src/temp/ui ../../libretroshare/src
INCLUDEPATH += ../../retroshare-gui/src/temp/ui ../../libretroshare/src
INCLUDEPATH += ../../retroshare-gui/src/retroshare-gui

INCLUDEPATH += ../../rapidjson-1.1.0

#################################### Windows #####################################

linux-* {
	#INCLUDEPATH += /usr/include
}

win32 {
	LIBS_DIR = $$PWD/../../../libs
}

QMAKE_CXXFLAGS *= -Wall

SOURCES = RetroChessPlugin.cpp               \
          services/p3RetroChess.cc           \
          services/rsRetroChessItems.cc \
          gui/NEMainpage.cpp \
          gui/RetroChessNotify.cpp \
          gui/chess.cpp \
          gui/tile.cpp \
          gui/validation.cpp \
          gui/RetroChessChatWidgetHolder.cpp

HEADERS = RetroChessPlugin.h                 \
          services/p3RetroChess.h            \
          services/rsRetroChessItems.h       \
          interface/rsRetroChess.h \
          gui/NEMainpage.h \
          gui/RetroChessNotify.h \
          gui/tile.h \
          gui/validation.h \
          gui/chess.h \
          gui/RetroChessChatWidgetHolder.h

FORMS += \
          gui/NEMainpage.ui \
          gui/chess.ui

RESOURCES = gui/RetroChess_images.qrc
