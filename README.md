# RetroShare 0.6 Chess Plugin

This is a Chess plugin, to play with your RetroShare Friends & Contacts

# build & install:

put/clone `RetroChess` to `RetroShare/plugins/` recommend

	cd ${YOUR_DIR}/RetroShare/plugins/RetroChess/
	mkdir build
	cd build
	qmake ..
	make 

Copy your RetroChess.dll to "Data/extensions6" (Windows)
Then restart your RetroShare. You'll see a chess logo in your chat dialog's tool-bar or home's tool-bar.

# Usage:

Send a invite via chat.

# Standalone chessboard debugger

RetroChess includes a small standalone Qt application for reproducing and
debugging chess-rule problems without starting RetroShare, creating a tunnel,
or connecting a second player. It uses the same `ChessBoard` input widget and
`ChessPosition` rules/state class as the plugin game window.

The debugger supports:

- Click-to-move and drag-and-drop moves.
- Loading and copying complete FEN positions.
- Resetting to the initial position.
- Testing legal moves, castling, en-passant and promotion.
- A timestamped log of accepted and rejected moves.
- A deterministic SHA-256 hash for comparing positions.

## Build RetroChessBoardDebug

```bash
    cd plugins/RetroChess/debug
    qmake ..
    make 
```

Start the debugger with:

```bash
./release/RetroChessBoardDebug.exe
```

The debugger is independent of the RetroShare services and does not send
moves over the network. To reproduce a reported problem, copy the FEN from a
game/debug report, paste it into the **FEN position** field, select **Load FEN**,
and play the move that caused the issue. The event log records the result and
the resulting position hash.

The standalone debugger is an additional development tool. It does not replace
or disable invitations, network games, game history, or the normal RetroChess
game window.

# Screenshot:

![Screenshot](https://github.com/RetroShare/RetroChess/blob/main/screenshot/screenshot.png)

#  extra info
based on: https://github.com/Texas-C/RetroChess

based on: https://github.com/chozabu/RetroChess
