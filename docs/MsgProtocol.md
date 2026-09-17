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
