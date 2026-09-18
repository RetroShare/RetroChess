Message includeing controls, invite/accept.

## GXS invitation replies

Invitations use the RetroChess service over a secured GXS tunnel; no distant
chat is required. `{"type":"chess_invite"}` prompts the receiver to Accept or
Reject in the chess page. Accept replies with `{"type":"chess_accept"}`;
Reject replies with `{"type":"chess_reject"}` on the same tunnel.

After the rejection is queued, the receiver clears the incoming invitation and
its UI actions. If queuing fails, the invitation remains available for retry.
The sender clears its outgoing invitation and displays "Invitation declined."
Unsolicited or duplicate rejection packets are ignored. A separate incoming
invitation from the same identity is preserved. The tunnel remains available.
Older versions that do not handle `chess_reject` will not display the decline.

## Leaderboard GXS Tunnel Synchronization

Leaderboard data is exchanged directly and gossiped across peers via secured GXS tunnels (Service ID `0xC4E5`):

1. `{"type":"leaderboard_receipt", "version":1, "game_id":"...", "white":"...", "black":"...", "result":"1-0|0-1|1/2-1/2", "signer":"...", "finished_at":123456}`:
   Published immediately upon rated match finish to active tunnels and gossiped to connected contacts.
2. `{"type":"leaderboard_sync", "version":1, "receipts":[...]}`:
   Sent in batches (up to 10 receipts per packet) when a tunnel becomes ready or upon request to synchronize known receipts.
3. `{"type":"leaderboard_sync_req", "version":1}`:
   Requests that the remote peer reply with their known receipts.

## Live Chess Spectator Mode (Watch Game)

Contacts can watch ongoing chess games between other contacts in real time without interrupting the players:

1. `{"type":"chess_watch_req", "version":1, "game_id":"..."}`:
   Sent by a spectator to a playing contact to request watching their live game. Handled silently in the background without user prompts or game interruption.
2. `{"type":"chess_watch_state", "version":1, "game_id":"...", "white_id":"...", "white_name":"...", "black_id":"...", "black_name":"...", "fen":"...", "sequence":12}`:
   Replied by the player with the current match details and board position (FEN + sequence number).
3. `{"type":"chess_watch_action", "version":1, "game_id":"...", "action":"move:..."}`:
   Broadcast to all active spectators whenever either player makes a move or performs a game action.
4. `{"type":"chess_watch_end", "version":1, "game_id":"...", "reason":"..."}`:
   Sent to spectators when the game concludes or a player leaves.
5. `{"type":"chess_watch_leave", "version":1, "game_id":"..."}`:
   Sent by a spectator when closing the watch window to unregister from live move relays.

# peer messge:
qvm peer message assembly in a `QVariantMap`, format as `key`-`value`, usage:

	QVariantMap map;
	map.insert( key_name, value );
	qvm_msg_peer( peerID, map);

`type`: message/package type, usage:

	QVairantMap map;
	map.insert("type", "type_name");

`type chat`: chat message, usage:

	map.insert("type", "chat");
	map.insert("message", message_str);

`type chessclick`: click message, usage:

	map.insert("type", "chessclick");
	map.insert("col", col_num);
	map.insert("row", row_num);
	map.insert("count", count);

`type player_status_message`: player status change message, 

	map.insert("type", "player_status_message");
	map.insert("player_status", "leave");

Another peer message is `raw_msg`
// not compelete
