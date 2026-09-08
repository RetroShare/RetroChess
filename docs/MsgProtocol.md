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
