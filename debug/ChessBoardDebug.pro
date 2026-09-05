# Standalone RetroChess board/rules debugger. No RetroShare services required.
QT += widgets
CONFIG += c++11
TEMPLATE = app
TARGET = RetroChessBoardDebug

SOURCES += ChessBoardDebugMain.cpp \
           ../gui/ChessBoard.cpp \
           ../gui/ChessPosition.cpp

HEADERS += ../gui/ChessBoard.h \
           ../gui/ChessPosition.h

RESOURCES += ../gui/RetroChess_images.qrc
