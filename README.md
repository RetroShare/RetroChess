# RetroShare 0.6 Chess Plugin

This is a Chess plugin, to play with your RetroShare Friends & Contacts

# build & install:

put/clone `RetroChess` to `RetroShare/plugins/` recommend

	cd ${YOUR_DIR}/RetroShare/plugins/RetroChess/
	qmake ..
	make 

Copy your RetroChess.dll to "Data/extensions6" (Windows)
Then restart your RetroShare. You'll see a chess logo in your chat dialog's tool-bar or home's tool-bar.

# Usage

### Accessing RetroChess
After installing and restarting RetroShare:
- Click the **RetroChess** icon in the main RetroShare navigation sidebar or home toolbar.
- You can also launch chess games directly from any RetroShare chat dialog toolbar.

The plugin provides the dedicated tabs:

---

### 1. Chess Players & Invitations
- **Managing Contacts**: Add players by their GXS ID or import them from your RetroShare contacts list. Past opponents are automatically remembered. Removing a chess contact does not remove the underlying RetroShare contact.
- **Online Presence & Status**:
  - 🟢 **Available**: Online and ready to play. Right-click or click **Invite** to send a game challenge.
  - 🔵 **Playing**: Currently playing another chess match.
  - 🟠 **Busy**: Online, but has set their status to busy.
  - ⚪ **Offline / Unreachable**: Offline contact.
- **Chess Profile**: Click the **Chess profile** button to select your active identity for chess games. Check the **Busy** box if you wish to temporarily decline new game invites.

---

### 2. Playing Games & Game History
- **Live Match Features**:
  - Figurine chess notation with piece vector icons in the moves list table.
  - Move highlighting (last moves, legal move hints, and check indicators).
  - Customizable board themes, piece sets, board flip, and sounds in **Settings** (gear icon).
- **Game History Tab**:
  - Automatically saves every finished game with opponent results, and timestamps.
  - **Review**: Step forwards and backwards through moves in completed games.
  - **Export PGN**: Export games to standard Portable Game Notation (.pgn) for replay and analysis in external chess engines and tools.

---

### 3. Leaderboard & Ratings
- **Glicko-2 Rating System**:
  - Every player starts with a base rating of **1500** (Rating Deviation: 350, Volatility: 0.06).
  - Players remain *Provisional* until completing at least 10 games with an RD at or below 110.
  - Hovering over the **Rating** or **RD** column displays detailed tooltips explaining rating reliability and games played.
- **Decentralized Verification**:
  - Completed GXS games exchange cryptographically signed receipts between players over secure GXS tunnels.
  - Receipts synchronize automatically between players and connected peers (gossip sync) without central servers.
  - Conflicting claims are safely excluded, and duplicate receipts cannot count twice.

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

