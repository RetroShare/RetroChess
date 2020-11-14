Message includeing controls, invite/accept.

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
