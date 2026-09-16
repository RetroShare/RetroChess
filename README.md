# RetroShare 0.6 Chess Plugin

This is a Chess plugin, to play with your RetroShare Friends & Contacts

# build & install:

put/clone `RetroChess` to `RetroShare/plugins/` recommend

	cd ${YOUR_DIR}/RetroShare/plugins/RetroChess/
	qmake ..
	make 

Copy your RetroChess.dll to "Data/extensions6" (Windows)
Then restart your RetroShare. You'll see a chess logo in your chat dialog's tool-bar or home's tool-bar.

# Usage:

Open **Chess Players** and add a player by GXS ID or from your RetroShare
contacts. Past opponents are imported from game history once; new opponents
are remembered when a game starts. Removing a chess contact does not remove
the underlying RetroShare contact.

The list keeps offline contacts and shows a coloured icon plus a status label:
Available, Playing, Busy, Checking, Unknown, or Offline / unreachable.
Right-click an available player to invite them, cancel a pending invitation,
or remove the contact. Existing invitations through chat remain supported.

Use **My chess identities** to enable local identities and select the identity
for new connections. Until configured, the first local identity is used.
Unchecking every identity disables new chess presence probes and responses.
The **Busy** checkbox advertises that you are unavailable for a new game.
Own identities cannot be added as opponents.

Presence uses versioned request/reply messages on the RetroChess GXS tunnel
service, independently of chat. At most four probes are outstanding. Successful
probes are repeated after 60 seconds, requests time out after 45 seconds, and
failures retry with a backoff up to ten minutes. A tunnel alone does not confirm
presence. Older plugins without the presence protocol may appear unreachable;
they can still be invited through the existing chat action. Last-seen times
are saved, but online status is checked again after restart.

The Rating column currently shows **Unrated**. Game history is saved, but an
Elo calculation and rating exchange have not been implemented.

## Presence regression checks

With the matching MinGW/Qt 5 tools on PATH, run `python tests/chess-presence.py`
and `powershell -File tests/pending-invite.ps1`. On Windows, use the MSYS2 shell
used for the plugin build. The presence test executes the production methods
against a fake transport and clock, covering reply validation, timeouts,
backoff, concurrency, self-exclusion, busy/playing responses, disabled
identities, contact persistence/removal, and preservation of games/invitations.

For an end-to-end check, run two RetroShare nodes with the updated plugin:
enable a chess identity on each, add the other identity, wait for Available,
toggle Busy, invite/accept/play, finish the game, disconnect/reconnect, and
restart to check that saved contacts remain. Verify that removing a contact
survives restart and that disabled identities no longer answer presence probes.

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
