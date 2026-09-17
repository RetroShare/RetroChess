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

### Leaderboard

The Leaderboard tab ranks GXS identities using Glicko-2 (initial rating 1500,
rating deviation 350, volatility 0.06; each confirmed game is a rating period).
It shows ratings, rating deviation, games, wins, draws, losses and last played.
Players remain provisional until they have ten games and RD is at most 110.

Completed GXS games exchange identity-signed result receipts over secure
GXS tunnels directly between players and through connected peers (gossip sync). Both
players must report the same game ID, colors and result before it counts.
Results and receipts are saved locally and synchronized over active GXS tunnels.
Conflicting claims exclude the game; repeated receipts cannot count it twice.
Rematches receive separate game IDs. Direct-peer games, aborted games and games
with older clients that do not exchange game IDs are unrated. The ledger
contains public player identities and game results. Ratings may change as
additional signed results arrive; they do not prevent collusion.

