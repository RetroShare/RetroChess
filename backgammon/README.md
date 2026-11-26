# Backgammon in RetroChess

Backgammon and Long Nardy as an extra game of RetroChess, like `4connect/`.
Right-click a player in the RetroChess Players tab → *Invite to Backgammon*
→ Backgammon or Long Nardy and the match length.
Pending invitations are listed in *Available players*: *Accept* / *Reject*
for a received one, *Pending...* (click to cancel) for a sent one, the game
in the *Mode* column. A chess invitation from the same player comes first. Invitations from others
show a Backgammon toaster with *Accept* and/or open the game window (both
switchable in the Backgammon settings, General); the gear button in the game
window opens those settings. Backgammon toasters follow only the Backgammon
setting, not the chess one; RetroChess' toaster notifier just hands them to
RetroShare (it takes one notifier per plugin).

`core/`, `services/` and `gui/` are copied unchanged from the standalone
Backgammon plugin (`plugins/Backgammon`), without its main page and
Preferences page and traffic viewer. `gui/Backgammon_images.qrc` leaves out `settings.png`,
which RetroChess already has. `RetroChessBackgammon.h/.cpp` is the only
glue: `registerService`, `shutdown` and `addContactAction`.

Same protocol and service id ("BGMN") as the Backgammon plugin, so players of
both can play each other. Do not load the Backgammon plugin and RetroChess in
the same RetroShare: only one can own the service, the other stays inactive.

Rules tests live in the Backgammon plugin (`tests/core-test.cpp`).

Debugging uses the RetroChess debug window (RetroChess settings → Debug, or
Backgammon settings → Debug → *Debug window…*): Backgammon log lines in the
areas `backgammon-tunnel` and `backgammon-game` of the Traffic tab, its
tunnels and matches in the Tunnels and Contacts tabs. The Backgammon log
files and their switches are unchanged. Changes to the copied files for this
are marked with "RetroChess:".
