# build:

## build with qmake in console:

put/clone `RetroChess` to `RetroShare/plugins/` recommend

	cd ${YOUR_DIR}/RetroShare/plugins/RetroChess/
	mkdir build
	cd build
	qmake ..
	make -j$(nproc)

# Code struct:
## Class's relation:

- `NEMainPage`  (based on MainPage): as Main page for inviting and launch the game
- `RetroChessWindow`(based on QWidget): for display the chessboard and players' information. and movement judge 
- `Tile` (based on QLabel): for display each block and units.
- `p3RetroChess` (based on RetroChess & RsPQIService): message send/receive service

## Progress:

GUI: `RetroChessWindow` setup the game gui.

Message 
