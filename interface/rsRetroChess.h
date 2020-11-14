/*******************************************************************************
 * interface/rsRetroChess.h                                                    *
 *                                                                             *
 * Copyright (C) 2020 RetroShare Team <retroshare.project@gmail.com>           *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify        *
 * it under the terms of the GNU Affero General Public License as              *
 * published by the Free Software Foundation, either version 3 of the          *
 * License, or (at your option) any later version.                             *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                *
 * GNU Affero General Public License for more details.                         *
 *                                                                             *
 * You should have received a copy of the GNU Affero General Public License    *
 * along with this program. If not, see <https://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/
/* this is a simple class to make it easy for any part of the plugin to call its services */
// interface class for p3RetroChess service

#pragma once

#include <stdint.h>
#include <string>
#include <list>
#include <vector>
#include <retroshare/rstypes.h>

#include <QVariantMap>
#include <QString>

class RsRetroChess ;
extern RsRetroChess *rsRetroChess;
 
static const uint32_t CONFIG_TYPE_RetroChess_PLUGIN 		= 0xe001 ;

class RsRetroChess
{
	public:

	virtual void ping_all() = 0;
	virtual void broadcast_paint(int x, int y) = 0;
	virtual void msg_all(std::string msg) = 0;
	virtual void chess_click(std::string peer_id, int col, int row, int count) = 0;
    virtual void player_leave(std::string peer_id) = 0;
    virtual void qvm_msg_peer(RsPeerId peerID, QVariantMap data) = 0;
	virtual void str_msg_peer(RsPeerId peerID, QString strdata) = 0;
	virtual void raw_msg_peer(RsPeerId peerID, std::string msg) = 0;
	virtual bool hasInviteFrom(RsPeerId peerID) = 0;
	virtual bool hasInviteTo(RsPeerId peerID) = 0;
	virtual void acceptedInvite(RsPeerId peerID) = 0;
	virtual void gotInvite(RsPeerId peerID) = 0;
	virtual void sendInvite(RsPeerId peerID) = 0;
};


