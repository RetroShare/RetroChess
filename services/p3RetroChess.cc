/*******************************************************************************
 * services/p3RetroChess.cc                                                    *
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

#include "util/rsdir.h"
#include "retroshare/rsiface.h"
#include "pqi/pqibin.h"
#include "pqi/pqistore.h"
#include "pqi/p3linkmgr.h"
#include <serialiser/rsserial.h>
#include <rsitems/rsconfigitems.h>
//#include "retroshare/rsmsgs.h"

#include <sstream> // for std::istringstream

#include "services/p3RetroChess.h"
#include "services/rsRetroChessItems.h"

#include <sys/time.h>

#include "gui/RetroChessNotify.h"
#include <retroshare/rschats.h>
#include <qjsondocument.h>
#include <QUuid>
#include <QJsonObject>
#include <algorithm>


//#define DEBUG_RetroChess		1


/* DEFINE INTERFACE POINTER! */
RsRetroChess *rsRetroChess = NULL;



#ifdef WINDOWS_SYS
#include <time.h>
#include <sys/timeb.h>
#endif

p3RetroChess::p3RetroChess(RsPluginHandler *handler,RetroChessNotify *notifier)
	: RsPQIService(RS_SERVICE_TYPE_RetroChess_PLUGIN,0,handler), mRetroChessMtx("p3RetroChess"), mServiceControl(handler->getServiceControl()), mNotify(notifier), mGxsTunnels(NULL)
{
	addSerialType(new RsRetroChessSerialiser());


	//plugin default configuration

}
RsServiceInfo p3RetroChess::getServiceInfo()
{
	const std::string TURTLE_APP_NAME = "RetroChess";
	const uint16_t TURTLE_APP_MAJOR_VERSION  =       1;
	const uint16_t TURTLE_APP_MINOR_VERSION  =       0;
	const uint16_t TURTLE_MIN_MAJOR_VERSION  =       1;
	const uint16_t TURTLE_MIN_MINOR_VERSION  =       0;

	return RsServiceInfo(RS_SERVICE_TYPE_RetroChess_PLUGIN,
	                     TURTLE_APP_NAME,
	                     TURTLE_APP_MAJOR_VERSION,
	                     TURTLE_APP_MINOR_VERSION,
	                     TURTLE_MIN_MAJOR_VERSION,
	                     TURTLE_MIN_MINOR_VERSION);
}

int	p3RetroChess::tick()
{
#ifdef DEBUG_RetroChess
	std::cerr << "ticking p3RetroChess" << std::endl;
#endif
	handleGxsTick();
	closePendingGxsTunnels();
	retryPendingDistantChatInvites();
	reconnectInterruptedSessions();
	tickChessPresence();
	return 0;
}

void p3RetroChess::reconnectInterruptedSessions()
{
	const time_t now = time(NULL);
	std::vector<RsGxsId> reconnect;
	{
		RsStackMutex stack(mRetroChessMtx);
		for (const auto &entry : mGameSessions) {
			const RsRetroChessGameSession &session = entry.second;
			const RsGxsId id(session.endpointId.toStdString());
			if (!session.gxs || mActiveTunnels.count(id)
			        || mPendingTunnels.count(id)) continue;
			const time_t last = mLastSessionReconnect[entry.first];
			if (now - last < 15) continue;
			mLastSessionReconnect[entry.first] = now;
			reconnect.push_back(RsGxsId(session.endpointId.toStdString()));
		}
	}
	for (const RsGxsId &id : reconnect) requestGxsTunnel(id);
}

int	p3RetroChess::status()
{
	return 1;
}
void p3RetroChess::str_msg_peer(RsPeerId peerID, QString strdata)
{
	QVariantMap map;
	map.insert("type", "chat");
	map.insert("message", strdata);

	qvm_msg_peer(peerID,map);
}

void p3RetroChess::qvm_msg_peer(RsPeerId peerID, QVariantMap data)
{
	QJsonDocument jsondoc = QJsonDocument::fromVariant(data);
	std::string msg = jsondoc.toJson().toStdString();
	raw_msg_peer(peerID, msg);
}

void p3RetroChess::chess_click(std::string peer_id, int col, int row, int count)
{
	QVariantMap map;
	map.insert("type", "chessclick");
	map.insert("col", col);
	map.insert("row", row);
	map.insert("count", count);

	RsPeerId peerID = RsPeerId(peer_id);
	qvm_msg_peer(peerID,map);

}

void p3RetroChess::player_leave(std::string peer_id)
{
    QVariantMap map;
    map.insert("type", "player_status_message");
    map.insert("player_status","leave");

    RsPeerId peerID = RsPeerId(peer_id);
    qvm_msg_peer(peerID, map);
}

bool p3RetroChess::hasInviteFrom(RsPeerId peerID)
{
	RsStackMutex stack(mRetroChessMtx); /****** LOCKED MUTEX *******/
	return invitesFrom.find(peerID)!=invitesFrom.end();
}
bool p3RetroChess::hasInviteTo(RsPeerId peerID)
{
	RsStackMutex stack(mRetroChessMtx); /****** LOCKED MUTEX *******/
	return invitesTo.find(peerID)!=invitesTo.end();
}

void p3RetroChess::acceptedInvite(RsPeerId peerID)
{
	{
		RsStackMutex stack(mRetroChessMtx); /****** LOCKED MUTEX *******/
		std::set<RsPeerId>::iterator it =invitesTo.find(peerID);
		if (it != invitesTo.end())
		{
			invitesTo.erase(it);
		}

		it =invitesFrom.find(peerID);
		if (it != invitesFrom.end())
		{
			invitesFrom.erase(it);
		}
	}
	raw_msg_peer(peerID, "{\"type\":\"chess_accept\"}");
}

void p3RetroChess::clearInvite(RsPeerId peerID)
{
	RsStackMutex stack(mRetroChessMtx); /****** LOCKED MUTEX *******/
	invitesTo.erase(peerID);
	invitesFrom.erase(peerID);
}

void p3RetroChess::gotInvite(RsPeerId peerID)
{
	RsStackMutex stack(mRetroChessMtx); /****** LOCKED MUTEX *******/
	std::set<RsPeerId>::iterator it =invitesFrom.find(peerID);
	if (it == invitesFrom.end())
	{
		invitesFrom.insert(peerID);
	}
}
void p3RetroChess::sendInvite(RsPeerId peerID)
{
	{
		RsStackMutex stack(mRetroChessMtx); /****** LOCKED MUTEX *******/
		std::set<RsPeerId>::iterator it =invitesTo.find(peerID);
		if (it == invitesTo.end())
		{
			invitesTo.insert(peerID);
		}
	}
	raw_msg_peer(peerID, "{\"type\":\"chess_invite\"}");
}


void p3RetroChess::raw_msg_peer(RsPeerId peerID, std::string msg)
{
#ifdef DEBUG_RetroChess
	std::cout << "MSging: " << peerID.toStdString() << "\n";
	std::cout << "MSging: " << msg << "\n";
#endif
	/* create the packet */
	RsRetroChessDataItem *pingPkt = new RsRetroChessDataItem();
	pingPkt->PeerId(peerID);
	pingPkt->m_msg = msg;
	//pingPkt->mSeqNo = mCounter;
	//pingPkt->mPingTS = convertTsTo64bits(ts);

	//storePingAttempt(*it, ts, mCounter);

#ifdef DEBUG_RetroChess
	std::cerr << "p3RetroChess::msg_all() With Packet:";
	std::cerr << std::endl;
	pingPkt->print(std::cerr, 10);
#endif

	sendItem(pingPkt);
}

void p3RetroChess::msg_all(std::string msg)
{
	/* we ping our peers */
	//if(!mServiceControl)
	//    return ;

	//std::set<RsPeerId> onlineIds;
	std::list< RsPeerId > onlineIds;
	//    mServiceControl->getPeersConnected(getServiceInfo().mServiceType, onlineIds);
	rsPeers->getOnlineList(onlineIds);

#ifdef DEBUG_RetroChess
	std::cerr << "p3RetroChess::msg_all() @ts: " << ts;
	std::cerr << std::endl;
#endif

	std::cout << "READY TO BCast: " << onlineIds.size() << "\n";
	/* prepare packets */
	std::list<RsPeerId>::iterator it;
	for(it = onlineIds.begin(); it != onlineIds.end(); it++)
	{
		str_msg_peer(RsPeerId(*it),QString::fromStdString(msg));
	}
}

void p3RetroChess::ping_all()
{
	// Required override of RsRetroChess pure virtual — no-op.
}

void p3RetroChess::broadcast_paint(int x, int y)
{
	std::list< RsPeerId > onlineIds;
	//    mServiceControl->getPeersConnected(getServiceInfo().mServiceType, onlineIds);
	rsPeers->getOnlineList(onlineIds);

	std::cout << "READY TO PAINT: " << onlineIds.size() << "\n";
	/* prepare packets */
	std::list<RsPeerId>::iterator it;
	for(it = onlineIds.begin(); it != onlineIds.end(); it++)
	{

		std::cout << "painting to: " << (*it).toStdString() << "\n";
		QVariantMap map;
		map.insert("type", "paint");
		map.insert("x", x);
		map.insert("y", y);

		qvm_msg_peer(RsPeerId(*it),map);
		/* create the packet */
		//TODO send paint packets
	}
}

//TODO  mNotify->notifyReceivedPaint(item->PeerId(), item->x,item->y);



void p3RetroChess::handleData(RsRetroChessDataItem *item)
{
	RsStackMutex stack(mRetroChessMtx); /****** LOCKED MUTEX *******/

	// store the data in a queue.


	mNotify->notifyReceivedMsg(item->PeerId(), QString::fromStdString(item->m_msg));
}

bool	p3RetroChess::recvItem(RsItem *item)
{
#ifdef DEBUG_RetroChess
	std::cout << "recvItem type: " << item->PacketSubType() << "\n";
#endif
	/* pass to specific handler */
	bool keep = false ;

	switch(item->PacketSubType())
	{
	case RS_PKT_SUBTYPE_RetroChess_DATA:
		// handleData() only forwards the message string to the notifier and
		// does not take ownership, so the item must not be kept: keeping it
		// leaked one item per received message.
		if (RsRetroChessDataItem* chess_item = dynamic_cast<RsRetroChessDataItem*>(item)) {
			handleData(chess_item);
		}
		break;
	/*case RS_PKT_SUBTYPE_RetroChess_INVITE:
		if (invites.find(item->PeerId()!=invites.end())){
			invites.insert(item->PeerId());
		}
		mNotify->

		//keep = true ;
		break;*/

	default:
		break;
	}

	/* clean up */
	if(!keep)
		delete item;
	return true ;
}



RsTlvKeyValue p3RetroChess::push_int_value(const std::string& key,int value)
{
	RsTlvKeyValue kv ;
	kv.key = key ;
	rs_sprintf(kv.value, "%d", value);

	return kv ;
}
int p3RetroChess::pop_int_value(const std::string& s)
{
	std::istringstream is(s) ;

	int val ;
	is >> val ;

	return val ;
}

bool p3RetroChess::saveList(bool& cleanup, std::list<RsItem*>& lst)
{
	cleanup = true ;

	RsConfigKeyValueSet *vitem = new RsConfigKeyValueSet ;
	{
		RsStackMutex stack(mRetroChessMtx);
		for (const auto &entry : mChessContacts) {
			RsTlvKeyValue value;
			value.key = "CHESS_CONTACT:" + entry.first.toStdString();
			value.value = std::to_string(entry.second.lastSeen);
			vitem->tlvkvs.pairs.push_back(value);
		}
		if (mChessIdentitiesConfigured) {
			RsTlvKeyValue value;
			value.key = "CHESS_IDENTITIES";
			QStringList ids;
			for (const auto &id : mChessIdentities) ids << QString::fromStdString(id.toStdString());
			value.value = ids.join(',').toStdString();
			vitem->tlvkvs.pairs.push_back(value);
		}
		RsTlvKeyValue preferred;
		preferred.key = "CHESS_PREFERRED";
		preferred.value = mPreferredChessIdentity.toStdString();
		vitem->tlvkvs.pairs.push_back(preferred);
		RsTlvKeyValue busy;
		busy.key = "CHESS_BUSY";
		busy.value = mChessBusy ? "1" : "0";
		vitem->tlvkvs.pairs.push_back(busy);
		for (const auto &entry : mGameSessions) {
			const RsRetroChessGameSession &session = entry.second;
			QVariantMap data;
			data.insert("endpoint", session.endpointId);
			data.insert("localIdentity", session.localIdentityId);
			data.insert("gxs", session.gxs);
			data.insert("color", session.localColor);
			data.insert("fen", session.fen);
			data.insert("sequence", session.moveSequence);
			data.insert("interrupted", true);
			RsTlvKeyValue value;
			value.key = "GAME_SESSION:" + entry.first;
			value.value = QJsonDocument::fromVariant(data)
			        .toJson(QJsonDocument::Compact).toStdString();
			vitem->tlvkvs.pairs.push_back(value);
		}
	}

	lst.push_back(vitem) ;

	return true ;
}
bool p3RetroChess::loadList(std::list<RsItem*>& load)
{
	for (RsItem *item : load) {
		if (RsConfigKeyValueSet *values = dynamic_cast<RsConfigKeyValueSet *>(item)) {
			for (const RsTlvKeyValue &value : values->tlvkvs.pairs) {
				if (value.key.compare(0, 14, "CHESS_CONTACT:") == 0) {
					const RsGxsId id(value.key.substr(14));
					if (!id.isNull()) {
						RsStackMutex stack(mRetroChessMtx);
						mChessContacts[id].lastSeen = QString::fromStdString(value.value).toLongLong();
					}
					continue;
				}
				if (value.key == "CHESS_IDENTITIES") {
					RsStackMutex stack(mRetroChessMtx);
					mChessIdentitiesConfigured = true;
					mChessIdentities.clear();
					for (const QString &text : QString::fromStdString(value.value).split(',')) {
						RsGxsId id(text.toStdString());
						if (!id.isNull()) mChessIdentities.insert(id);
					}
					continue;
				}
				if (value.key == "CHESS_PREFERRED" || value.key == "CHESS_BUSY") {
					RsStackMutex stack(mRetroChessMtx);
					if (value.key == "CHESS_PREFERRED") mPreferredChessIdentity = RsGxsId(value.value);
					else mChessBusy = value.value == "1";
					continue;
				}
				if (value.key.compare(0, 13, "GAME_SESSION:") != 0) continue;
				const QVariantMap data = QJsonDocument::fromJson(
				        QByteArray::fromStdString(value.value)).toVariant().toMap();
				RsRetroChessGameSession session;
				session.endpointId = data.value("endpoint").toString();
				session.localIdentityId = data.value("localIdentity").toString();
				session.gxs = data.value("gxs").toBool();
				session.localColor = data.value("color").toInt();
				session.fen = data.value("fen").toString();
				session.moveSequence = data.value("sequence").toUInt();
				session.interrupted = true;
				if (!session.endpointId.isEmpty()) {
					RsStackMutex stack(mRetroChessMtx);
					mGameSessions[session.endpointId.toStdString()] = session;
					if (session.gxs && !session.localIdentityId.isEmpty())
						mOwnGxsIdByPeer[RsGxsId(session.endpointId.toStdString())]
						        = RsGxsId(session.localIdentityId.toStdString());
				}
			}
		}
		delete item;
	}
	load.clear();
	return true ;
}

void p3RetroChess::registerGameSession(const RsRetroChessGameSession &session)
{
	if (session.endpointId.isEmpty()) return;
	if (session.gxs) addChessContact(RsGxsId(session.endpointId.toStdString()));
	{
		RsStackMutex stack(mRetroChessMtx);
		mGameSessions[session.endpointId.toStdString()] = session;
		if (session.gxs && !session.localIdentityId.isEmpty())
			mOwnGxsIdByPeer[RsGxsId(session.endpointId.toStdString())]
			        = RsGxsId(session.localIdentityId.toStdString());
	}
	IndicateConfigChanged();
}

void p3RetroChess::updateGameSession(
        const QString &endpointId, const QString &fen, uint32_t moveSequence,
        int lastFromTile, int lastToTile, const QStringList &moveHistory)
{
	std::vector<RsGxsTunnelId> spectatorTunnels;
	RsRetroChessGameSession activeSession;
	bool found = false;
	{
		RsStackMutex stack(mRetroChessMtx);
		auto it = mGameSessions.find(endpointId.toStdString());
		if (it == mGameSessions.end()) return;
		it->second.fen = fen;
		it->second.moveSequence = moveSequence;
		it->second.lastFromTile = lastFromTile;
		it->second.lastToTile = lastToTile;
		it->second.moveHistory = moveHistory;
		it->second.interrupted = false;
		activeSession = it->second;
		found = true;

		auto specIt = mSpectatorsByGame.find(endpointId.toStdString());
		if (specIt != mSpectatorsByGame.end()) {
			for (const auto &specId : specIt->second) {
				auto tIt = mActiveTunnels.find(specId);
				if (tIt != mActiveTunnels.end())
					spectatorTunnels.push_back(tIt->second);
			}
		}
	}
	if (found && !spectatorTunnels.empty() && mGxsTunnels) {
		QString whiteId, whiteName, blackId, blackName;
		const QString localId = activeSession.localIdentityId;
		const QString oppId = activeSession.endpointId;

		QString localName = "Player";
		RsIdentityDetails localDetails;
		if (rsIdentity && rsIdentity->getIdDetails(RsGxsId(localId.toStdString()), localDetails) && !localDetails.mNickname.empty())
			localName = QString::fromUtf8(localDetails.mNickname.c_str());

		QString oppName = "Opponent";
		RsIdentityDetails oppDetails;
		if (rsIdentity && rsIdentity->getIdDetails(RsGxsId(oppId.toStdString()), oppDetails) && !oppDetails.mNickname.empty())
			oppName = QString::fromUtf8(oppDetails.mNickname.c_str());

		if (activeSession.localColor == 0) {
			whiteId = localId; whiteName = localName;
			blackId = oppId; blackName = oppName;
		} else {
			whiteId = oppId; whiteName = oppName;
			blackId = localId; blackName = localName;
		}

		QVariantMap reply;
		reply["type"] = "chess_watch_state";
		reply["version"] = 1;
		reply["game_id"] = endpointId;
		reply["white_id"] = whiteId;
		reply["white_name"] = whiteName;
		reply["black_id"] = blackId;
		reply["black_name"] = blackName;
		reply["fen"] = fen;
		reply["sequence"] = static_cast<int>(moveSequence);
		reply["last_from"] = lastFromTile;
		reply["last_to"] = lastToTile;
		QVariantList movesList;
		for (const auto &m : moveHistory) movesList.append(m);
		reply["moves"] = movesList;

		const QByteArray replyBytes = QJsonDocument::fromVariant(reply).toJson(QJsonDocument::Compact);
		for (const auto &tId : spectatorTunnels) {
			mGxsTunnels->sendData(tId, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
			                      reinterpret_cast<const uint8_t*>(replyBytes.constData()), replyBytes.size());
		}
	}
	IndicateConfigChanged();
}

void p3RetroChess::unregisterGameSession(const QString &endpointId)
{
	bool removed;
	std::vector<RsGxsTunnelId> spectatorTunnels;
	{
		RsStackMutex stack(mRetroChessMtx);
		removed = mGameSessions.erase(endpointId.toStdString()) > 0;
		mLastSessionReconnect.erase(endpointId.toStdString());
		auto specIt = mSpectatorsByGame.find(endpointId.toStdString());
		if (specIt != mSpectatorsByGame.end()) {
			for (const auto &specId : specIt->second) {
				auto tIt = mActiveTunnels.find(specId);
				if (tIt != mActiveTunnels.end())
					spectatorTunnels.push_back(tIt->second);
			}
			mSpectatorsByGame.erase(specIt);
		}
	}
	if (!spectatorTunnels.empty() && mGxsTunnels) {
		QVariantMap endMsg;
		endMsg["type"] = "chess_watch_end";
		endMsg["version"] = 1;
		endMsg["game_id"] = endpointId;
		endMsg["reason"] = "Game ended";
		const QByteArray bytes = QJsonDocument::fromVariant(endMsg).toJson(QJsonDocument::Compact);
		for (const auto &tId : spectatorTunnels) {
			mGxsTunnels->sendData(tId, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
			                      reinterpret_cast<const uint8_t*>(bytes.constData()), bytes.size());
		}
	}
	if (removed) IndicateConfigChanged();
}

std::vector<RsRetroChessGameSession> p3RetroChess::gameSessions()
{
	RsStackMutex stack(mRetroChessMtx);
	std::vector<RsRetroChessGameSession> result;
	for (const auto &entry : mGameSessions) result.push_back(entry.second);
	return result;
}

std::vector<RsRetroChessAvailablePeer> p3RetroChess::availableChessPeers()
{
    std::vector<RsRetroChessAvailablePeer> result;
    RsStackMutex stack(mRetroChessMtx);
    std::set<RsGxsId> identities;
    for (const auto &entry : mChessContacts) identities.insert(entry.first);
    identities.insert(mInvitesToGxs.begin(), mInvitesToGxs.end());
    identities.insert(mInvitesFromGxs.begin(), mInvitesFromGxs.end());
    for (const auto &id : identities) {
        RsRetroChessAvailablePeer peer(QString::fromStdString(id.toStdString()),
                true, mActiveTunnels.count(id) != 0);
        auto contact = mChessContacts.find(id);
        if (contact != mChessContacts.end()) {
            peer.savedContact = true;
            peer.status = contact->second.status;
            peer.lastSeen = contact->second.lastSeen;
            peer.opponentId = contact->second.opponentId;
            peer.opponentName = contact->second.opponentName;
            peer.gameId = contact->second.gameId;
            peer.timeControl = contact->second.seekTimeControl;
            peer.seeking = contact->second.seeking;
        }
        result.push_back(peer);
    }
    return result;
}


bool p3RetroChess::addChessContact(const RsGxsId &id)
{
    if (id.isNull() || !rsIdentity || rsIdentity->isOwnId(id)) return false;
    bool added;
    {
        RsStackMutex stack(mRetroChessMtx);
        added = mChessContacts.emplace(id, ChessContact()).second;
    }
    if (added) {
        IndicateConfigChanged();
        mNotify->notifyAvailablePeersChanged();
    }
    return true;
}

void p3RetroChess::removeChessContact(const RsGxsId &id)
{
    RsGxsTunnelId close;
    {
        RsStackMutex stack(mRetroChessMtx);
        mChessContacts.erase(id);
        if (!mGameSessions.count(id.toStdString()) && !mInvitesToGxs.count(id)
                && !mInvitesFromGxs.count(id)) {
            auto active = mActiveTunnels.find(id);
            auto pending = mPendingTunnels.find(id);
            if (active != mActiveTunnels.end()) close = active->second;
            else if (pending != mPendingTunnels.end()) close = pending->second;
            mActiveTunnels.erase(id);
            mPendingTunnels.erase(id);
            mTunnelToGxsIdMap.erase(close);
            mOwnGxsIdByPeer.erase(id);
        }
    }
    if (mGxsTunnels && !close.isNull()) mGxsTunnels->closeExistingTunnel(close, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID);
    IndicateConfigChanged();
    mNotify->notifyAvailablePeersChanged();
}

std::list<RsGxsId> p3RetroChess::chessIdentities()
{
    std::list<RsGxsId> own;
    if (rsIdentity) rsIdentity->getOwnIds(own);
    RsStackMutex stack(mRetroChessMtx);
    // Preserve the old first-identity default until the user configures this.
    if (!mChessIdentitiesConfigured) {
        if (own.size() > 1) own.erase(++own.begin(), own.end());
        return own;
    }
    own.remove_if([this](const RsGxsId &id) { return !mChessIdentities.count(id); });
    return own;
}

RsGxsId p3RetroChess::preferredChessIdentity()
{
    const auto ids = chessIdentities();
    RsStackMutex stack(mRetroChessMtx);
    if (std::find(ids.begin(), ids.end(), mPreferredChessIdentity) != ids.end()) return mPreferredChessIdentity;
    return ids.empty() ? RsGxsId() : ids.front();
}

bool p3RetroChess::chessIdentityEnabled(const RsGxsId &id)
{
    const auto ids = chessIdentities();
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

RsGxsId p3RetroChess::selectChessIdentity(const RsGxsId &peer)
{
    RsGxsId previous;
    {
        RsStackMutex stack(mRetroChessMtx);
        auto it = mOwnGxsIdByPeer.find(peer);
        if (it != mOwnGxsIdByPeer.end()) previous = it->second;
    }
    return chessIdentityEnabled(previous) ? previous : preferredChessIdentity();
}

void p3RetroChess::setChessIdentities(const std::list<RsGxsId> &ids, const RsGxsId &preferred)
{
    std::set<RsGxsId> valid;
    for (const auto &id : ids) if (rsIdentity && rsIdentity->isOwnId(id)) valid.insert(id);
    std::vector<RsGxsTunnelId> close;
    {
        RsStackMutex stack(mRetroChessMtx);
        mChessIdentities = valid;
        mChessIdentitiesConfigured = true;
        mPreferredChessIdentity = valid.count(preferred) ? preferred : (valid.empty() ? RsGxsId() : *valid.begin());
        for (auto &entry : mChessContacts) {
            entry.second.nextProbe = 0;
            entry.second.deadline = 0;
            entry.second.nonce.clear();
            entry.second.status = "unknown";
        }
        // Keep in-progress games on their original identity, but rebuild idle
        // connections with the chosen identity on the next presence tick.
        for (auto it = mOwnGxsIdByPeer.begin(); it != mOwnGxsIdByPeer.end();) {
            const RsGxsId peer = it->first;
            if (mGameSessions.count(peer.toStdString()) || mInvitesToGxs.count(peer) || mInvitesFromGxs.count(peer)) { ++it; continue; }
            auto active = mActiveTunnels.find(peer);
            auto pending = mPendingTunnels.find(peer);
            if (active != mActiveTunnels.end()) close.push_back(active->second);
            else if (pending != mPendingTunnels.end()) close.push_back(pending->second);
            mActiveTunnels.erase(peer);
            mPendingTunnels.erase(peer);
            it = mOwnGxsIdByPeer.erase(it);
        }
        for (const auto &tunnel : close) mTunnelToGxsIdMap.erase(tunnel);
    }
    for (const auto &tunnel : close) if (mGxsTunnels) mGxsTunnels->closeExistingTunnel(tunnel, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID);
    IndicateConfigChanged();
    mNotify->notifyAvailablePeersChanged();
}

bool p3RetroChess::chessBusy()
{
    RsStackMutex stack(mRetroChessMtx);
    return mChessBusy;
}

void p3RetroChess::setChessBusy(bool busy)
{
    {
        RsStackMutex stack(mRetroChessMtx);
        mChessBusy = busy;
    }
    IndicateConfigChanged();
}

void p3RetroChess::tickChessPresence()
{
    if (!mGxsTunnels || !rsIdentity) return;
    const time_t now = time(nullptr);
    const bool enabled = !preferredChessIdentity().isNull();
    std::list<RsGxsId> ownIds;
    rsIdentity->getOwnIds(ownIds);
    std::vector<RsGxsId> open;
    std::vector<RsGxsTunnelId> close;
    std::vector<std::pair<RsGxsTunnelId, QByteArray>> sends;
    bool changed = false;
    {
        RsStackMutex stack(mRetroChessMtx);
        unsigned int inFlight = 0;
        // Bound expensive tunnel discovery, not heartbeats on working tunnels.
        for (const auto &entry : mChessContacts)
            if (entry.second.deadline && !mActiveTunnels.count(entry.first)) ++inFlight;
        for (auto &entry : mChessContacts) {
            const RsGxsId &id = entry.first;
            ChessContact &contact = entry.second;
            if (std::find(ownIds.begin(), ownIds.end(), id) != ownIds.end()) continue;
            if (contact.deadline && now >= contact.deadline) {
                contact.deadline = 0;
                contact.nonce.clear();
                contact.status = "offline";
                contact.seeking = false;
                contact.seekTimeControl = ChessTimeControl{};
                contact.failures = std::min(contact.failures + 1, 4u);
                // Keep offline discovery responsive instead of backing off for eight minutes.
                contact.nextProbe = now + std::min(60u, 15u << (contact.failures - 1));
                if (!mActiveTunnels.count(id)) --inFlight;
                changed = true;
                // Presence failures must never tear down an invitation, game, or watch request.
                if (!mGameSessions.count(id.toStdString()) && !mInvitesToGxs.count(id)
                        && !mInvitesFromGxs.count(id) && !mPendingWatchRequests.count(id)) {
                    auto pending = mPendingTunnels.find(id);
                    auto active = mActiveTunnels.find(id);
                    RsGxsTunnelId tunnel;
                    if (active != mActiveTunnels.end()) tunnel = active->second;
                    else if (pending != mPendingTunnels.end()) tunnel = pending->second;
                    if (!tunnel.isNull()) {
                        close.push_back(tunnel);
                        mPendingTunnels.erase(id);
                        mActiveTunnels.erase(id);
                        mTunnelToGxsIdMap.erase(tunnel);
                        mOwnGxsIdByPeer.erase(id);
                    }
                }
            }
            if (contact.lastSeen && now - contact.lastSeen > 120
                    && (contact.status == "available" || contact.status == "playing" || contact.status == "busy")) {
                contact.status = "offline";
                contact.opponentId.clear();
                contact.opponentName.clear();
                contact.gameId.clear();
                changed = true;
            }
            if (enabled && !contact.deadline && now >= contact.nextProbe
                    && (mActiveTunnels.count(id) || inFlight < 4)) {
                // GXS discovery and its key exchange need their own connection budget.
                contact.deadline = now + 120;
                contact.nonce.clear();
                if (!mActiveTunnels.count(id)) ++inFlight;
                if (contact.status == "unknown" || contact.status == "offline") {
                    contact.status = "checking";
                    changed = true;
                }
                if (!mActiveTunnels.count(id) && !mPendingTunnels.count(id)) open.push_back(id);
            }
            auto active = mActiveTunnels.find(id);
            if (enabled && contact.deadline && contact.nonce.isEmpty() && active != mActiveTunnels.end()) {
                contact.nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
                contact.probeTunnel = active->second;
                // Start the reply timeout when the probe is actually sent, not
                // when the asynchronous tunnel connection was requested.
                contact.deadline = now + 45;
                QVariantMap message;
                message["type"] = "chess_presence_request";
                message["version"] = 1;
                message["nonce"] = contact.nonce;
                sends.push_back({active->second, QJsonDocument::fromVariant(message).toJson(QJsonDocument::Compact)});
            }
        }
    }
    for (const auto &tunnel : close) mGxsTunnels->closeExistingTunnel(tunnel, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID);
    for (const auto &id : open) requestGxsTunnel(id);
    for (const auto &send : sends) mGxsTunnels->sendData(send.first, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
            reinterpret_cast<const uint8_t*>(send.second.constData()), send.second.size());
    if (changed) mNotify->notifyAvailablePeersChanged();
}

bool p3RetroChess::handleChessPresence(const RsGxsId &sender, const RsGxsTunnelId &tunnel, const QVariantMap &message)
{
    const QString type = message.value("type").toString();
    if (type != "chess_presence_request" && type != "chess_presence_reply") return false;
    const QString nonce = message.value("nonce").toString();
    if (message.value("version").toInt() != 1 || nonce.isEmpty() || nonce.size() > 64) return true;
    RsGxsTunnelService::GxsTunnelInfo info;
    if (!mGxsTunnels || !mGxsTunnels->getTunnelInfo(tunnel, info)
            || !chessIdentityEnabled(info.source_gxs_id)) return true;
    if (type == "chess_presence_request") {
        QVariantMap reply;
        reply["type"] = "chess_presence_reply";
        reply["version"] = 1;
        reply["nonce"] = nonce;
        QString state = "available";
        QString opponentId;
        QString opponentName;
        QString gameId;
        QByteArray reciprocalProbe;
        {
            RsStackMutex stack(mRetroChessMtx);
            if (mChessBusy) state = "busy";
            reply["seeking"] = mLobbySeekActive;
            if (mLobbySeekActive) reply["tc"] = mLobbySeek.toNetString();
            for (const auto &entry : mGameSessions) {
                if (entry.second.gxs && entry.second.localIdentityId == QString::fromStdString(info.source_gxs_id.toStdString())) {
                    state = "playing";
                    opponentId = entry.second.endpointId;
                    gameId = entry.second.gameId;
                    if (gameId.isEmpty()) {
                        auto git = mGameIdByPeer.find(RsGxsId(opponentId.toStdString()));
                        if (git != mGameIdByPeer.end()) gameId = git->second;
                    }
                    break;
                }
            }
            if (!opponentId.isEmpty() && rsIdentity) {
                RsIdentityDetails oppDetails;
                if (rsIdentity->getIdDetails(RsGxsId(opponentId.toStdString()), oppDetails) && !oppDetails.mNickname.empty()) {
                    opponentName = QString::fromUtf8(oppDetails.mNickname.c_str());
                }
            }
            // A saved contact reaching us has a working tunnel already. Probe
            // back on it now instead of waiting through an offline retry delay.
            // Still require a nonce-matched reply before displaying availability.
            auto contactIt = mChessContacts.find(sender);
            auto active = mActiveTunnels.find(sender);
            auto pending = mPendingTunnels.find(sender);
            auto own = mOwnGxsIdByPeer.find(sender);
            const bool sameIdentity = own == mOwnGxsIdByPeer.end() || own->second == info.source_gxs_id;
            const bool sameTunnel = (active == mActiveTunnels.end() || active->second == tunnel)
                    && (pending == mPendingTunnels.end() || pending->second == tunnel);
            if (contactIt != mChessContacts.end() && sameTunnel && sameIdentity) {
                ChessContact &contact = contactIt->second;
                const time_t now = time(nullptr);
                const bool awaitingReply = contact.deadline > now && !contact.nonce.isEmpty();
                if (!awaitingReply && (contact.deadline || now >= contact.nextProbe
                        || contact.status == "offline" || contact.status == "checking"
                        || contact.status == "unknown")) {
                    mActiveTunnels[sender] = tunnel;
                    mPendingTunnels.erase(sender);
                    mOwnGxsIdByPeer[sender] = info.source_gxs_id;
                    contact.deadline = now + 45;
                    contact.nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
                    contact.probeTunnel = tunnel;
                    if (contact.status == "offline" || contact.status == "unknown")
                        contact.status = "checking";
                    QVariantMap probe;
                    probe["type"] = "chess_presence_request";
                    probe["version"] = 1;
                    probe["nonce"] = contact.nonce;
                    reciprocalProbe = QJsonDocument::fromVariant(probe).toJson(QJsonDocument::Compact);
                }
            }
        }
        reply["status"] = state;
        if (state != "available") {
            reply["seeking"] = false;
            reply.remove("tc");
        }
        if (state == "playing" && !opponentId.isEmpty()) {
            reply["opponent_id"] = opponentId;
            if (!opponentName.isEmpty()) reply["opponent_name"] = opponentName;
            if (!gameId.isEmpty()) reply["game_id"] = gameId;
        }
        const QByteArray bytes = QJsonDocument::fromVariant(reply).toJson(QJsonDocument::Compact);
        mGxsTunnels->sendData(tunnel, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                reinterpret_cast<const uint8_t*>(bytes.constData()), bytes.size());
        if (!reciprocalProbe.isEmpty()) {
            mGxsTunnels->sendData(tunnel, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                    reinterpret_cast<const uint8_t*>(reciprocalProbe.constData()), reciprocalProbe.size());
            mNotify->notifyAvailablePeersChanged();
        }
    } else {
        const QString state = message.value("status").toString();
        if (state != "available" && state != "playing" && state != "busy") return true;
        const QString opponentId = (state == "playing") ? message.value("opponent_id").toString() : QString();
        QString opponentName = (state == "playing") ? message.value("opponent_name").toString() : QString();
        const QString gameId = (state == "playing") ? message.value("game_id").toString() : QString();
        if (opponentName.isEmpty() && !opponentId.isEmpty() && rsIdentity) {
            RsIdentityDetails oppDetails;
            if (rsIdentity->getIdDetails(RsGxsId(opponentId.toStdString()), oppDetails) && !oppDetails.mNickname.empty()) {
                opponentName = QString::fromUtf8(oppDetails.mNickname.c_str());
            }
        }
        {
            RsStackMutex stack(mRetroChessMtx);
            auto it = mChessContacts.find(sender);
            if (it == mChessContacts.end() || !it->second.deadline || it->second.deadline <= time(nullptr)
                    || it->second.nonce != nonce || it->second.probeTunnel != tunnel) return true;
            ChessContact &contact = it->second;
            contact.status = state;
            contact.seeking = state == "available" && message.value("seeking").toBool();
            contact.seekTimeControl = contact.seeking
                    ? ChessTimeControl::fromNetString(message.value("tc").toString()) : ChessTimeControl{};
            contact.opponentId = opponentId;
            contact.opponentName = opponentName;
            contact.gameId = gameId;
            contact.lastSeen = time(nullptr);
            contact.nextProbe = contact.lastSeen + 60;
            contact.deadline = 0;
            contact.nonce.clear();
            contact.failures = 0;
        }
        IndicateConfigChanged();
        mNotify->notifyAvailablePeersChanged();
    }
    return true;
}

RsSerialiser *p3RetroChess::setupSerialiser()
{
	RsSerialiser *rsSerialiser = new RsSerialiser();
	rsSerialiser->addSerialType(new RsRetroChessSerialiser());
	rsSerialiser->addSerialType(new RsGeneralConfigSerialiser());

	return rsSerialiser ;
}

void p3RetroChess::chess_click_gxs(const RsGxsId &gxs_id, int col, int row, int count)
{
    RsGxsTunnelId tunnel_id;
    {
        RsStackMutex stack(mRetroChessMtx); /****** LOCKED MUTEX *******/
        auto it = mActiveTunnels.find(gxs_id);
        if (it != mActiveTunnels.end())
            tunnel_id = it->second;
    }

    if (tunnel_id.isNull()) {
        // Tunnel not ready, try to re-open. Called unlocked: sendGxsInvite
        // takes the mutex itself and RsMutex is not recursive.
        sendGxsInvite(gxs_id);
        return;
    }

    // sendData() copies the buffer, so a plain string is all that is needed.
    // The RsRetroChessDataItem previously allocated here was never freed,
    // leaking one item per move sent.
    const std::string msg = QString("%1,%2,%3").arg(col).arg(row).arg(count).toStdString();
    mGxsTunnels->sendData(tunnel_id, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID, (const uint8_t*)msg.c_str(), msg.size());
}

void p3RetroChess::requestGxsTunnel(const RsGxsId &gxsId)
{
    // Check if we already have a tunnel
    bool active = false;
    bool pending = false;
    {
        RsStackMutex stack(mRetroChessMtx);
        active = mActiveTunnels.count(gxsId) > 0;
        pending = mPendingTunnels.count(gxsId) > 0;
    }
    if (active) {
        mNotify->notifyGxsTunnelReady(gxsId);
        return;
    }
    if (pending) {
        return;
    }
    // Otherwise, start the async tunnel request
    this->sendGxsInvite(gxsId);
}

void p3RetroChess::sendGxsInvite(const RsGxsId &to_gxs_id)
{
    if (!mGxsTunnels) return;

    if (to_gxs_id.isNull() || !rsIdentity || rsIdentity->isOwnId(to_gxs_id)) return;
    {
        RsStackMutex stack(mRetroChessMtx);
        if (mPendingTunnels.count(to_gxs_id) > 0 || mActiveTunnels.count(to_gxs_id) > 0) return;
    }
    const RsGxsId from_gxs_id = selectChessIdentity(to_gxs_id);
    if (from_gxs_id.isNull()) return;

    RsGxsTunnelId tunnel_id;
    uint32_t error_code;

    // Open a tunnel using mGxsTunnel (Async Request)
    if (mGxsTunnels->requestSecuredTunnel(
            to_gxs_id, from_gxs_id, tunnel_id,
            RETRO_CHESS_GXS_TUNNEL_SERVICE_ID, error_code))
    {
        {
            RsStackMutex stack(mRetroChessMtx);
            mPendingTunnels[to_gxs_id] = tunnel_id;
            mOwnGxsIdByPeer[to_gxs_id] = from_gxs_id;
        }
        mNotify->notifyAvailablePeersChanged();
    }
}

bool p3RetroChess::sendInviteToGxs(const RsGxsId &gxsId, bool joinOpenGame)
{
	const RsGxsId ownId = selectChessIdentity(gxsId);
	if (ownId.isNull()) return false;
	return doSendInviteOverGxs(gxsId, ownId, joinOpenGame);
}

bool p3RetroChess::isJoinRequestFromGxs(const RsGxsId &gxsId)
{
    RsStackMutex stack(mRetroChessMtx);
    return mInvitesFromGxs.count(gxsId) && mJoinRequestsFromGxs.count(gxsId);
}

bool p3RetroChess::hasInviteToGxs(const RsGxsId &gxsId)
{
    RsStackMutex stack(mRetroChessMtx);
    return mInvitesToGxs.count(gxsId) != 0;
}

bool p3RetroChess::cancelInviteToGxs(const RsGxsId &gxsId)
{
    RsGxsTunnelId tunnelId;
    {
        RsStackMutex stack(mRetroChessMtx);
        if (mInvitesToGxs.count(gxsId) == 0) return false;
        auto pending = mPendingGxsInvites.find(gxsId);
        if (pending != mPendingGxsInvites.end()
                && QJsonDocument::fromJson(QByteArray::fromStdString(pending->second)).object().value("type").toString() == "chess_invite") {
            mPendingGxsInvites.erase(pending);
            mInvitesToGxs.erase(gxsId);
        } else {
            auto active = mActiveTunnels.find(gxsId);
            if (active == mActiveTunnels.end() || !mGxsTunnels) return false;
            tunnelId = active->second;
        }
    }
    if (!tunnelId.isNull()) {
        const std::string message = "{\"type\":\"chess_cancel\"}";
        if (!mGxsTunnels->sendData(tunnelId, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                reinterpret_cast<const uint8_t*>(message.data()), message.size()))
            return false;
        RsStackMutex stack(mRetroChessMtx);
        if (mInvitesToGxs.erase(gxsId) == 0) return false;
    }
    mNotify->notifyAvailablePeersChanged();
    return true;
}

void p3RetroChess::acceptedInviteGxs(const RsGxsId &gxsId)
{
    std::cout << "Chess: acceptedInviteGxs from " << gxsId << std::endl;

    RsGxsTunnelId activeTunnel;
    {
        RsStackMutex stack(mRetroChessMtx);
        mInvitesFromGxs.erase(gxsId);
        auto it = mActiveTunnels.find(gxsId);
        if (it != mActiveTunnels.end())
            activeTunnel = it->second;
        else {
            // Client side: we initiated the invite but the tunnel isn't in
            // mActiveTunnels yet. Queue accept for when it becomes CAN_TALK.
            QJsonObject acceptObj{{"type", "chess_accept"}, {"game_id", mGameIdByPeer[gxsId]}};
            auto tcIt = mInviteTimeControlByPeer.find(gxsId);
            if (tcIt != mInviteTimeControlByPeer.end() && !tcIt->second.unlimited)
                acceptObj["tc"] = tcIt->second.toNetString();
            mPendingGxsInvites[gxsId] = QJsonDocument(acceptObj).toJson(QJsonDocument::Compact).toStdString();
        }
    }

    if (!activeTunnel.isNull()) {
        // Tunnel already active (server side — invite arrived over it).
        // Send chess_accept immediately, outside the mutex.
        if (!mGxsTunnels) return;
        QJsonObject acceptObj{{"type", "chess_accept"}, {"game_id", gameIdForPeer(gxsId)}};
        {
            RsStackMutex stack(mRetroChessMtx);
            auto tcIt = mInviteTimeControlByPeer.find(gxsId);
            if (tcIt != mInviteTimeControlByPeer.end() && !tcIt->second.unlimited)
                acceptObj["tc"] = tcIt->second.toNetString();
        }
        const std::string accept = QJsonDocument(acceptObj).toJson(QJsonDocument::Compact).toStdString();
        std::cout << "Chess: Sending chess_accept over tunnel " << activeTunnel << std::endl;
        mGxsTunnels->sendData(activeTunnel, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                              (const uint8_t*)accept.c_str(), accept.size());
    } else {
        requestGxsTunnel(gxsId);
    }
}

bool p3RetroChess::rejectedInviteGxs(const RsGxsId &gxsId)
{
    RsGxsTunnelId tunnelId;
    {
        RsStackMutex stack(mRetroChessMtx);
        if (mInvitesFromGxs.count(gxsId) == 0) return false;
        auto it = mActiveTunnels.find(gxsId);
        if (it == mActiveTunnels.end() || !mGxsTunnels) return false;
        tunnelId = it->second;
    }
    const std::string reply = "{\"type\":\"chess_reject\"}";
    // Keep the invitation available for retry if the transport cannot queue it.
    if (!mGxsTunnels->sendData(tunnelId, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
            reinterpret_cast<const uint8_t*>(reply.data()), reply.size()))
        return false;
    clearInviteGxs(gxsId);
    mNotify->notifyChessInviteClearedGxs(gxsId);
    return true;
}

void p3RetroChess::clearInviteGxs(const RsGxsId &gxsId)
{
    RsStackMutex stack(mRetroChessMtx);
    mInvitesFromGxs.erase(gxsId);
}

bool p3RetroChess::sendRematchGxs(const RsGxsId &gxsId, int localColor)
{
    RsGxsTunnelId tunnelId;
    {
        RsStackMutex stack(mRetroChessMtx);
        auto it = mActiveTunnels.find(gxsId);
        if (it == mActiveTunnels.end() || !mGxsTunnels)
            return false;
        tunnelId = it->second;
    }

    QVariantMap map;
    map.insert("type", "rematch");
    map.insert("color", localColor);
    map.insert("game_id", gameIdForPeer(gxsId));
    const std::string message = QJsonDocument::fromVariant(map).toJson(QJsonDocument::Compact).toStdString();
    return mGxsTunnels->sendData(
            tunnelId, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
            reinterpret_cast<const uint8_t*>(message.data()), message.size());
}

bool p3RetroChess::sendGameActionGxs(const RsGxsId &gxsId, const std::string &action)
{
    RsGxsTunnelId tunnelId;
    std::vector<RsGxsTunnelId> spectatorTunnels;
    {
        RsStackMutex stack(mRetroChessMtx);
        auto it = mActiveTunnels.find(gxsId);
        if (it == mActiveTunnels.end() || !mGxsTunnels)
            return false;
        tunnelId = it->second;

        auto specIt = mSpectatorsByGame.find(gxsId.toStdString());
        if (specIt != mSpectatorsByGame.end()) {
            for (const auto &specId : specIt->second) {
                auto tIt = mActiveTunnels.find(specId);
                if (tIt != mActiveTunnels.end())
                    spectatorTunnels.push_back(tIt->second);
            }
        }
    }
    QVariantMap map;
    map.insert("type", "game_action");
    map.insert("action", QString::fromStdString(action));
    const QByteArray message = QJsonDocument::fromVariant(map).toJson(QJsonDocument::Compact);
    bool sent = mGxsTunnels->sendData(
            tunnelId, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
            reinterpret_cast<const uint8_t*>(message.constData()), message.size());

    if (!spectatorTunnels.empty() && mGxsTunnels) {
        QVariantMap specMap;
        specMap.insert("type", "chess_watch_action");
        specMap.insert("version", 1);
        specMap.insert("game_id", QString::fromStdString(gxsId.toStdString()));
        specMap.insert("action", QString::fromStdString(action));
        const QByteArray specMsg = QJsonDocument::fromVariant(specMap).toJson(QJsonDocument::Compact);
        for (const auto &sTunnel : spectatorTunnels) {
            mGxsTunnels->sendData(sTunnel, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                                  reinterpret_cast<const uint8_t*>(specMsg.constData()), specMsg.size());
        }
    }

    return sent;
}

bool p3RetroChess::sendWatchRequestGxs(const RsGxsId &hostPlayerId, const QString &gameKey)
{
    if (hostPlayerId.isNull() || !mGxsTunnels) return false;

    RsGxsTunnelId tunnelId;
    {
        RsStackMutex stack(mRetroChessMtx);
        auto it = mActiveTunnels.find(hostPlayerId);
        if (it != mActiveTunnels.end()) {
            tunnelId = it->second;
        } else {
            mPendingWatchRequests[hostPlayerId] = gameKey;
        }
    }

    if (!tunnelId.isNull()) {
        std::cout << "Chess: Sending watch request to " << hostPlayerId << " over active tunnel " << tunnelId
                  << " for game " << gameKey.toStdString() << std::endl;
        QVariantMap req;
        req["type"] = "chess_watch_req";
        req["version"] = 1;
        req["game_id"] = gameKey;
        const QByteArray bytes = QJsonDocument::fromVariant(req).toJson(QJsonDocument::Compact);
        return mGxsTunnels->sendData(tunnelId, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                                     reinterpret_cast<const uint8_t*>(bytes.constData()), bytes.size());
    }

    std::cout << "Chess: Queueing watch request and requesting tunnel for host " << hostPlayerId
              << " for game " << gameKey.toStdString() << std::endl;
    // Tunnel not active yet: request it
    requestGxsTunnel(hostPlayerId);
    return true;
}

void p3RetroChess::sendWatchLeaveGxs(const RsGxsId &hostPlayerId, const QString &gameKey)
{
    if (hostPlayerId.isNull() || !mGxsTunnels) return;

    RsGxsTunnelId tunnelId;
    {
        RsStackMutex stack(mRetroChessMtx);
        mPendingWatchRequests.erase(hostPlayerId);
        auto it = mActiveTunnels.find(hostPlayerId);
        if (it != mActiveTunnels.end())
            tunnelId = it->second;
    }

    if (!tunnelId.isNull()) {
        QVariantMap msg;
        msg["type"] = "chess_watch_leave";
        msg["version"] = 1;
        msg["game_id"] = gameKey;
        const QByteArray bytes = QJsonDocument::fromVariant(msg).toJson(QJsonDocument::Compact);
        mGxsTunnels->sendData(tunnelId, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                              reinterpret_cast<const uint8_t*>(bytes.constData()), bytes.size());
    }
}

bool p3RetroChess::sendLeaderboardDataGxs(const RsGxsId &gxsId, const QByteArray &data)
{
    RsGxsTunnelId tunnelId;
    {
        RsStackMutex stack(mRetroChessMtx);
        auto it = mActiveTunnels.find(gxsId);
        if (it == mActiveTunnels.end() || !mGxsTunnels)
            return false;
        tunnelId = it->second;
    }
    return mGxsTunnels->sendData(
            tunnelId, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
            reinterpret_cast<const uint8_t*>(data.constData()), data.size());
}

void p3RetroChess::broadcastLeaderboardDataGxs(const QByteArray &data)
{
    std::vector<RsGxsTunnelId> tunnels;
    {
        RsStackMutex stack(mRetroChessMtx);
        if (!mGxsTunnels) return;
        for (const auto &entry : mActiveTunnels)
            tunnels.push_back(entry.second);
    }
    for (const auto &tunnelId : tunnels)
        mGxsTunnels->sendData(
                tunnelId, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                reinterpret_cast<const uint8_t*>(data.constData()), data.size());
}

std::vector<RsGxsId> p3RetroChess::activeGxsTunnels()
{
    RsStackMutex stack(mRetroChessMtx);
    std::vector<RsGxsId> peers;
    for (const auto &entry : mActiveTunnels)
        peers.push_back(entry.first);
    return peers;
}

bool p3RetroChess::hasInviteFromGxs(const RsGxsId &gxsId)
{
    RsStackMutex stack(mRetroChessMtx);
    return mInvitesFromGxs.find(gxsId) != mInvitesFromGxs.end();
}

RsGxsId p3RetroChess::ownGxsIdForPeer(const RsGxsId &gxsId)
{
    RsStackMutex stack(mRetroChessMtx);
    auto it = mOwnGxsIdByPeer.find(gxsId);
    return it == mOwnGxsIdByPeer.end() ? RsGxsId() : it->second;
}

QString p3RetroChess::gameIdForPeer(const RsGxsId &gxsId)
{
    RsStackMutex stack(mRetroChessMtx);
    auto it = mGameIdByPeer.find(gxsId);
    return it == mGameIdByPeer.end() ? QString() : it->second;
}

void p3RetroChess::startNewGameIdForPeer(const RsGxsId &gxsId)
{
    RsStackMutex stack(mRetroChessMtx);
    mGameIdByPeer[gxsId] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    mRematchIdByPeer[gxsId] = mGameIdByPeer[gxsId];
}

bool p3RetroChess::sendInvite_chat(const ChatId &chatId)
{
    // For a peer (non-GXS) chat: use the legacy sendInvite path
    if (chatId.isPeerId()) {
        sendInvite(chatId.toPeerId());
        return true;
    }

    if (!chatId.isDistantChatId()) {
        std::cerr << "Chess: sendInvite_chat: unknown ChatId type" << std::endl;
        return false;
    }

    DistantChatPeerInfo info;
    if (!rsChats->getDistantChatStatus(chatId.toDistantChatId(), info)
        || info.to_id.isNull() || info.own_id.isNull())
    {
        // Distant chat tunnel not established yet (no messages exchanged).
        // Queue this chatId and retry every tick() until the tunnel is ready.
        std::cout << "Chess: sendInvite_chat: distant chat not ready yet, queuing retry for "
                  << chatId.toStdString() << std::endl;
        RsStackMutex stack(mRetroChessMtx);
        // Only add once; don't reset timestamps on duplicate clicks
        if (mPendingDistantChatInvites.find(chatId.toDistantChatId()) == mPendingDistantChatInvites.end()) {
            PendingDistantInvite pending;
            pending.queuedTS = time(NULL);
            pending.lastTryTS = pending.queuedTS;
            mPendingDistantChatInvites[chatId.toDistantChatId()] = pending;
        }
        return true;
    }

    return doSendInviteOverGxs(info.to_id, info.own_id);
}

bool p3RetroChess::doSendInviteOverGxs(const RsGxsId &toId, const RsGxsId &ownId, bool joinOpenGame)
{
    if (!rsIdentity || toId.isNull() || rsIdentity->isOwnId(toId) || !chessIdentityEnabled(ownId)) return false;
    {
        RsStackMutex stack(mRetroChessMtx);
        if (mChessBusy) {
            std::cerr << "Chess: refusing distant invitation while busy" << std::endl;
            return false;
        }
    }
    std::cout << "Chess: doSendInviteOverGxs: to=" << toId << " from=" << ownId << std::endl;

    if (!mGxsTunnels) {
        std::cerr << "Chess: doSendInviteOverGxs: mGxsTunnels is NULL!" << std::endl;
        return false;
    }

    RsGxsTunnelId activeTunnel;
    {
        RsStackMutex stack(mRetroChessMtx);
        mOwnGxsIdByPeer[toId] = ownId;
        auto activeIt = mActiveTunnels.find(toId);
        if (activeIt != mActiveTunnels.end()) activeTunnel = activeIt->second;
    }

    startNewGameIdForPeer(toId);
    { RsStackMutex stack(mRetroChessMtx); mRematchIdByPeer.erase(toId); }
    QJsonObject inviteJson{{"type", "chess_invite"}, {"game_id", gameIdForPeer(toId)}};
    // Preserve the purpose independently of the time control: unlimited open
    // games can be joined too.
    inviteJson["join_open_game"] = joinOpenGame;
    {
        RsStackMutex stack(mRetroChessMtx);
        // Normal invitations always start an unlimited game, independently of
        // any advertised seek or previous invitation to this peer. Keep an
        // explicit entry so timeControlForPeer cannot fall back to their seek.
        if (!joinOpenGame)
            mInviteTimeControlByPeer[toId] = ChessTimeControl{};
        auto tcIt = mInviteTimeControlByPeer.find(toId);
        if (tcIt != mInviteTimeControlByPeer.end() && !tcIt->second.unlimited)
            inviteJson["tc"] = tcIt->second.toNetString();
    }
    const std::string invite = QJsonDocument(inviteJson).toJson(QJsonDocument::Compact).toStdString();
    if (!activeTunnel.isNull()) {
        if (mGxsTunnels->sendData(
                    activeTunnel, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                    reinterpret_cast<const uint8_t*>(invite.data()), invite.size()))
        {
            RsStackMutex stack(mRetroChessMtx);
            mInvitesToGxs.insert(toId);
            return true;
        }

        // A game may have closed before the tunnel-status callback reaches us.
        // Never lose a new invitation by continuing to trust that stale entry.
        std::cerr << "Chess: active tunnel is stale; opening a new tunnel for "
                  << toId << std::endl;
        RsStackMutex stack(mRetroChessMtx);
        auto activeIt = mActiveTunnels.find(toId);
        if (activeIt != mActiveTunnels.end() && activeIt->second == activeTunnel)
            mActiveTunnels.erase(activeIt);
        mTunnelToGxsIdMap.erase(activeTunnel);
    }

    {
        RsStackMutex stack(mRetroChessMtx);
        // A tunnel request is already progressing. Keep it and its queued
        // invitation instead of replacing the tunnel ID on every button click.
        if (mPendingTunnels.find(toId) != mPendingTunnels.end()) {
            mPendingGxsInvites[toId] = invite;
            mInvitesToGxs.insert(toId);
            return true;
        }
    }

    RsGxsTunnelId tunnelId;
    uint32_t error_code = 0;
    if (mGxsTunnels->requestSecuredTunnel(toId, ownId, tunnelId,
                                          RETRO_CHESS_GXS_TUNNEL_SERVICE_ID, error_code))
    {
        {
            RsStackMutex stack(mRetroChessMtx);
            mPendingTunnels[toId] = tunnelId;
            mPendingGxsInvites[toId] = invite;
            mInvitesToGxs.insert(toId);
            std::cout << "Chess: Tunnel requested (id=" << tunnelId << "), invite queued for " << toId << std::endl;
        }
        mNotify->notifyAvailablePeersChanged();
        return true;
    } else {
        std::cerr << "Chess: doSendInviteOverGxs: requestSecuredTunnel failed, error=" << error_code << std::endl;
        return false;
    }
}

void p3RetroChess::retryPendingDistantChatInvites()
{
    // Stop retrying an invite whose distant chat never comes up. Without a
    // deadline a single click on the chess button of an offline identity
    // meant a retry every 2 seconds for the whole session.
    static const time_t GIVE_UP_AFTER_SECS = 600;

    std::map<DistantChatPeerId, PendingDistantInvite> pending;
    {
        RsStackMutex stack(mRetroChessMtx);
        pending = mPendingDistantChatInvites;
    }

    if (pending.empty()) return;

    time_t now = time(NULL);
    for (auto it = pending.begin(); it != pending.end(); ++it) {
        if (now - it->second.queuedTS > GIVE_UP_AFTER_SECS) {
            std::cerr << "Chess: giving up on distant chat invite for "
                      << it->first << " (tunnel never came up)" << std::endl;
            RsStackMutex stack(mRetroChessMtx);
            mPendingDistantChatInvites.erase(it->first);
            continue;
        }

        // Throttle: only retry every 2 seconds
        if (now - it->second.lastTryTS < 2) continue;

        DistantChatPeerInfo info;
        if (rsChats->getDistantChatStatus(it->first, info)
            && !info.to_id.isNull() && !info.own_id.isNull())
        {
            std::cout << "Chess: Retry succeeded for distant chat "
                      << it->first << " -> GXS " << info.to_id << std::endl;
            {
                RsStackMutex stack(mRetroChessMtx);
                mPendingDistantChatInvites.erase(it->first);
            }
            // Now we have valid GXS IDs — send the invite
            if (!doSendInviteOverGxs(info.to_id, info.own_id)) {
                std::cerr << "Chess: retry resolved the distant chat but failed to queue the invite" << std::endl;
            }
        } else {
            // Still not ready — update timestamp so we wait another 2 seconds
            RsStackMutex stack(mRetroChessMtx);
            auto mit = mPendingDistantChatInvites.find(it->first);
            if (mit != mPendingDistantChatInvites.end())
                mit->second.lastTryTS = now;
        }
    }
}


void p3RetroChess::handleGxsTick()
{
    // Runs on the service tick thread while the GUI thread mutates the same
    // maps under mRetroChessMtx, so every map access here must be locked too.
    // Tunnel-service calls and notifications happen outside the mutex.
    if (!mGxsTunnels) return;

    std::map<RsGxsId, RsGxsTunnelId> pending;
    {
        RsStackMutex stack(mRetroChessMtx); /****** LOCKED MUTEX *******/
        pending = mPendingTunnels;
    }
    if (pending.empty()) return;

    std::list<std::pair<RsGxsTunnelId, std::string> > flushes;
    std::list<RsGxsId> ready;
    bool pendingChanged = false;

    for (auto it = pending.begin(); it != pending.end(); ++it) {
        RsGxsTunnelService::GxsTunnelInfo tinfo;
        if (!mGxsTunnels->getTunnelInfo(it->second, tinfo))
            continue;

        // Check if the tunnel is "Connected" (CAN_TALK)
        if (tinfo.tunnel_status == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_CAN_TALK) {
            RsStackMutex stack(mRetroChessMtx); /****** LOCKED MUTEX *******/
            auto pit = mPendingTunnels.find(it->first);
            if (pit == mPendingTunnels.end() || pit->second != it->second)
                continue; // changed concurrently, handle on next tick
            mActiveTunnels[it->first] = it->second;
            mPendingTunnels.erase(pit);
            pendingChanged = true;

            // Flush any queued invite for this peer
            auto inviteIt = mPendingGxsInvites.find(it->first);
            if (inviteIt != mPendingGxsInvites.end()) {
                flushes.push_back(std::make_pair(it->second, inviteIt->second));
                mPendingGxsInvites.erase(inviteIt);
            }
            auto watchIt = mPendingWatchRequests.find(it->first);
            if (watchIt != mPendingWatchRequests.end()) {
                std::cout << "Chess: Tunnel ready, flushing queued watch request to " << it->first
                          << " for game " << watchIt->second.toStdString() << std::endl;
                QVariantMap req;
                req["type"] = "chess_watch_req";
                req["version"] = 1;
                req["game_id"] = watchIt->second;
                const std::string reqMsg = QJsonDocument::fromVariant(req).toJson(QJsonDocument::Compact).toStdString();
                flushes.push_back(std::make_pair(it->second, reqMsg));
                mPendingWatchRequests.erase(watchIt);
            }
            ready.push_back(it->first);
        }
        // TUNNEL_DN is also the engine's initial, still-connecting state.
        // Keep the invitation queued until CAN_TALK; only an explicit remote
        // close cancels a pending connection.
        else if (tinfo.tunnel_status == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_REMOTELY_CLOSED) {
            RsStackMutex stack(mRetroChessMtx); /****** LOCKED MUTEX *******/
            auto pit = mPendingTunnels.find(it->first);
            if (pit != mPendingTunnels.end() && pit->second == it->second) {
                mPendingGxsInvites.erase(it->first); // discard queued invite
                mInvitesToGxs.erase(it->first);
                mPendingTunnels.erase(pit);
                pendingChanged = true;
            }
        }
    }

    for (auto it = flushes.begin(); it != flushes.end(); ++it) {
        std::cout << "Chess: Tunnel ready, flushing queued invite" << std::endl;
        mGxsTunnels->sendData(it->first, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                              (const uint8_t*)it->second.c_str(), it->second.size());
        // Cancellation can race with an invite already copied into flushes.
        // Send cancellation after that invite so it cannot remain pending remotely.
        if (QJsonDocument::fromJson(QByteArray::fromStdString(it->second)).object().value("type").toString() == "chess_invite") {
            bool cancelled = false;
            {
                RsStackMutex stack(mRetroChessMtx);
                for (const auto &entry : pending)
                    if (entry.second == it->first)
                        cancelled = mInvitesToGxs.count(entry.first) == 0;
            }
            if (cancelled) {
                const std::string cancel = "{\"type\":\"chess_cancel\"}";
                mGxsTunnels->sendData(it->first, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                        reinterpret_cast<const uint8_t*>(cancel.data()), cancel.size());
            }
        }
    }
    for (auto it = ready.begin(); it != ready.end(); ++it)
        mNotify->notifyGxsTunnelReady(*it);
    if (!ready.empty() || pendingChanged)
        mNotify->notifyAvailablePeersChanged();
}


void p3RetroChess::handleRawData(const RsGxsId& gxs_id,
                                 const RsGxsTunnelId& tunnel_id,
                                 bool /*am_I_client_side*/,
                                 const uint8_t *data,
                                 uint32_t data_size)
{
    // Resolve sender: first try the acceptDataFromPeer-populated map, then the passed gxs_id
    RsGxsId sender_id;
    {
        RsStackMutex stack(mRetroChessMtx);
        auto it = mTunnelToGxsIdMap.find(tunnel_id);
        if (it != mTunnelToGxsIdMap.end())
            sender_id = it->second;
    }
    if (sender_id.isNull() && !gxs_id.isNull())
        sender_id = gxs_id;

    if (sender_id.isNull()) {
        std::cerr << "Chess::handleRawData: unknown tunnel " << tunnel_id << std::endl;
        return;
    }

    // All messages are JSON
#ifdef DEBUG_RetroChess
    std::cout << "Chess::handleRawData: received from " << sender_id << ": "
              << std::string((const char*)data, data_size) << std::endl;
#endif

    QJsonDocument jsondoc = QJsonDocument::fromJson(QByteArray((const char*)data, data_size));
    QVariantMap map = jsondoc.toVariant().toMap();
    QString type = map.value("type").toString();
    if (handleChessPresence(sender_id, tunnel_id, map)) return;

    if (type == "chess_invite") {
        RsGxsTunnelService::GxsTunnelInfo info;
        if (!mGxsTunnels || !mGxsTunnels->getTunnelInfo(tunnel_id, info)
                || !chessIdentityEnabled(info.source_gxs_id)) return;
        {
            RsStackMutex stack(mRetroChessMtx);
            if (mChessBusy) {
                std::cerr << "Chess: ignoring distant invitation while busy" << std::endl;
                QJsonObject busyJson{{"type", "chess_busy"}};
                const QByteArray busy = QJsonDocument(busyJson).toJson(QJsonDocument::Compact);
                mGxsTunnels->sendData(tunnel_id, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                                      reinterpret_cast<const uint8_t*>(busy.constData()), busy.size());
                return;
            }
        }
        std::cout << "Chess: Received invite from GXS " << sender_id << std::endl;
        {
            RsStackMutex stack(mRetroChessMtx);
            auto game = mGameSessions.find(sender_id.toStdString());
            if (game != mGameSessions.end()
                    && game->second.localIdentityId != QString::fromStdString(info.source_gxs_id.toStdString())) return;
            // Remember the identity and tunnel chosen for this invitation.
            mOwnGxsIdByPeer[sender_id] = info.source_gxs_id;
            mActiveTunnels[sender_id] = tunnel_id;
            mGameIdByPeer[sender_id] = map.value("game_id").toString();
            mInviteTimeControlByPeer[sender_id] = ChessTimeControl::fromNetString(map.value("tc").toString());
            mJoinRequestsFromGxs.erase(sender_id);
            if (map.value("join_open_game").toBool()) mJoinRequestsFromGxs.insert(sender_id);
            mInvitesFromGxs.insert(sender_id);
        }
        // A new invite packet is also a refresh of an existing pending invite.
        // Always notify the UI so the toaster and chat action reappear.
        mNotify->notifyChessInviteGxs(sender_id);

    } else if (type == "chess_busy") {
        mNotify->notifyChessBusyGxs(sender_id);
    } else if (type == "chess_accept") {
        // Only honour an accept for an invitation we actually sent, like
        // hasInviteTo() on the direct-peer path. Otherwise any identity able
        // to open a tunnel could spawn game windows at will.
        bool invited;
        {
            RsStackMutex stack(mRetroChessMtx);
            invited = mInvitesToGxs.erase(sender_id) > 0;
            if (invited) {
                mGameIdByPeer[sender_id] = map.value("game_id").toString();
                if (map.contains("tc")) {
                    const QString tcStr = map.value("tc").toString();
                    mInviteTimeControlByPeer[sender_id] = ChessTimeControl::fromNetString(tcStr);
                }
            }
        }
        if (!invited) {
            std::cerr << "Chess: Ignoring chess_accept from " << sender_id
                      << " (no pending invitation)" << std::endl;
            return;
        }
        std::cout << "Chess: Received accept from GXS " << sender_id << std::endl;
        mNotify->notifyChessAcceptedGxs(sender_id);

    } else if (type == "chess_cancel") {
        {
            RsStackMutex stack(mRetroChessMtx);
            if (mInvitesFromGxs.erase(sender_id) == 0) return;
        }
        mNotify->notifyChessInviteClearedGxs(sender_id);

    } else if (type == "chess_reject") {
        {
            RsStackMutex stack(mRetroChessMtx);
            // Ignore unsolicited or duplicate replies. A rejection only
            // resolves our outgoing invite, not a crossed incoming invite.
            if (mInvitesToGxs.erase(sender_id) == 0) return;
            auto pending = mPendingGxsInvites.find(sender_id);
            if (pending != mPendingGxsInvites.end()
                    && QJsonDocument::fromJson(QByteArray::fromStdString(pending->second)).object().value("type").toString() == "chess_invite")
                mPendingGxsInvites.erase(pending);
        }
        mNotify->notifyChessRejectedGxs(sender_id);
        mNotify->notifyAvailablePeersChanged();

    } else if (type == "player_leave") {
        std::cout << "Chess: Remote GXS player left " << sender_id << std::endl;
        {
            RsStackMutex stack(mRetroChessMtx);
            mInvitesFromGxs.erase(sender_id);
        }
        mNotify->notifyChessPlayerLeftGxs(sender_id);

    } else if (type == "rematch") {
        const int remoteColor = map.value("color").toInt();
        {
            RsStackMutex stack(mRetroChessMtx);
            QString incoming = map.value("game_id").toString();
            auto pending = mRematchIdByPeer.find(sender_id);
            if (pending != mRematchIdByPeer.end() && !incoming.isEmpty())
                incoming = std::min(incoming, pending->second);
            mGameIdByPeer[sender_id] = incoming;
            mRematchIdByPeer.erase(sender_id);
        }
        std::cout << "Chess: Remote GXS player requested a rematch " << sender_id << std::endl;
        mNotify->notifyChessRematchGxs(sender_id, remoteColor);

    } else if (type == "game_action") {
        const QString action = map.value("action").toString();
        mNotify->notifyChessGameActionGxs(sender_id, action);

        std::vector<RsGxsTunnelId> spectatorTunnels;
        {
            RsStackMutex stack(mRetroChessMtx);
            auto specIt = mSpectatorsByGame.find(sender_id.toStdString());
            if (specIt != mSpectatorsByGame.end()) {
                for (const auto &specId : specIt->second) {
                    auto tIt = mActiveTunnels.find(specId);
                    if (tIt != mActiveTunnels.end())
                        spectatorTunnels.push_back(tIt->second);
                }
            }
        }
        if (!spectatorTunnels.empty() && mGxsTunnels) {
            QVariantMap specMap;
            specMap.insert("type", "chess_watch_action");
            specMap.insert("version", 1);
            specMap.insert("game_id", QString::fromStdString(sender_id.toStdString()));
            specMap.insert("action", action);
            const QByteArray specMsg = QJsonDocument::fromVariant(specMap).toJson(QJsonDocument::Compact);
            for (const auto &sTunnel : spectatorTunnels) {
                mGxsTunnels->sendData(sTunnel, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                                      reinterpret_cast<const uint8_t*>(specMsg.constData()), specMsg.size());
            }
        }

    } else if (type == "chess_seek") {
        // A peer is advertising (or withdrawing) a game seek with a given time control.
        // "tc" field: compact net string e.g. "5+3" or "unlimited". Absent means cancel.
        const QString tcStr = map.value("tc").toString();
        const ChessTimeControl tc = ChessTimeControl::fromNetString(
            tcStr.isEmpty() ? QStringLiteral("unlimited") : tcStr);
        {
            RsStackMutex stack(mRetroChessMtx);
            auto it = mChessContacts.find(sender_id);
            if (it != mChessContacts.end()) {
                it->second.seekTimeControl = tc;
                it->second.seeking = map.value("seeking", !tc.unlimited).toBool();
            }
        }
        mNotify->notifyAvailablePeersChanged();
    } else if (type == "chess_watch_req") {
        const QString reqGameId = map.value("game_id").toString();
        RsRetroChessGameSession activeSession;
        bool foundSession = false;
        std::string sessionEndpoint;

        {
            RsStackMutex stack(mRetroChessMtx);
            for (const auto &entry : mGameSessions) {
                if (entry.second.gxs) {
                    const QString oppIdStr = QString::fromStdString(entry.second.endpointId.toStdString());
                    const QString localIdStr = entry.second.localIdentityId;
                    if (reqGameId.isEmpty() || reqGameId.contains(oppIdStr) || reqGameId.contains(localIdStr)
                            || (!entry.second.gameId.isEmpty() && reqGameId == entry.second.gameId)) {
                        activeSession = entry.second;
                        sessionEndpoint = entry.first;
                        foundSession = true;
                        break;
                    }
                }
            }
            if (foundSession) {
                mSpectatorsByGame[sessionEndpoint].insert(sender_id);
                mActiveTunnels[sender_id] = tunnel_id;
            }
        }

        std::cout << "Chess: Received chess_watch_req from " << sender_id
                  << " for game " << reqGameId.toStdString()
                  << " (foundSession=" << (foundSession ? "true" : "false") << ")" << std::endl;

        if (foundSession && mGxsTunnels) {
            QString whiteId, whiteName, blackId, blackName;
            const QString localId = activeSession.localIdentityId;
            const QString oppId = activeSession.endpointId;

            QString localName = "Player";
            RsIdentityDetails localDetails;
            if (rsIdentity && rsIdentity->getIdDetails(RsGxsId(localId.toStdString()), localDetails) && !localDetails.mNickname.empty())
                localName = QString::fromUtf8(localDetails.mNickname.c_str());

            QString oppName = "Opponent";
            RsIdentityDetails oppDetails;
            if (rsIdentity && rsIdentity->getIdDetails(RsGxsId(oppId.toStdString()), oppDetails) && !oppDetails.mNickname.empty())
                oppName = QString::fromUtf8(oppDetails.mNickname.c_str());

            if (activeSession.localColor == 0) {
                whiteId = localId; whiteName = localName;
                blackId = oppId; blackName = oppName;
            } else {
                whiteId = oppId; whiteName = oppName;
                blackId = localId; blackName = localName;
            }

            QVariantMap reply;
            reply["type"] = "chess_watch_state";
            reply["version"] = 1;
            reply["game_id"] = reqGameId.isEmpty() ? QString::fromStdString(sessionEndpoint) : reqGameId;
            reply["white_id"] = whiteId;
            reply["white_name"] = whiteName;
            reply["black_id"] = blackId;
            reply["black_name"] = blackName;
            reply["fen"] = activeSession.fen;
            reply["sequence"] = static_cast<int>(activeSession.moveSequence);
            reply["last_from"] = activeSession.lastFromTile;
            reply["last_to"] = activeSession.lastToTile;
            QVariantList movesList;
            for (const auto &m : activeSession.moveHistory) movesList.append(m);
            reply["moves"] = movesList;

            std::cout << "Chess: Sending chess_watch_state to spectator " << sender_id
                      << " (FEN: " << activeSession.fen.toStdString()
                      << ", lastMove: " << activeSession.lastFromTile << "->" << activeSession.lastToTile
                      << ", movesCount: " << activeSession.moveHistory.size() << ")" << std::endl;

            const QByteArray replyBytes = QJsonDocument::fromVariant(reply).toJson(QJsonDocument::Compact);
            mGxsTunnels->sendData(tunnel_id, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                                  reinterpret_cast<const uint8_t*>(replyBytes.constData()), replyBytes.size());
        } else if (mGxsTunnels) {
            QVariantMap failReply;
            failReply["type"] = "chess_watch_end";
            failReply["version"] = 1;
            failReply["game_id"] = reqGameId;
            failReply["reason"] = "Game not active";
            const QByteArray failBytes = QJsonDocument::fromVariant(failReply).toJson(QJsonDocument::Compact);
            mGxsTunnels->sendData(tunnel_id, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                                  reinterpret_cast<const uint8_t*>(failBytes.constData()), failBytes.size());
        }

    } else if (type == "chess_watch_state") {
        const QString gameId = map.value("game_id").toString();
        const QString whiteId = map.value("white_id").toString();
        const QString whiteName = map.value("white_name").toString();
        const QString blackId = map.value("black_id").toString();
        const QString blackName = map.value("black_name").toString();
        const QString fen = map.value("fen").toString();
        const int sequence = map.value("sequence").toInt();
        const int lastFrom = map.contains("last_from") ? map.value("last_from").toInt() : -1;
        const int lastTo = map.contains("last_to") ? map.value("last_to").toInt() : -1;
        QStringList moves;
        if (map.contains("moves")) {
            for (const auto &v : map.value("moves").toList())
                moves.append(v.toString());
        }
        std::cout << "Chess: Received chess_watch_state from " << sender_id
                  << " for game " << gameId.toStdString() << " (FEN=" << fen.toStdString()
                  << ", lastMove=" << lastFrom << "->" << lastTo
                  << ", movesCount=" << moves.size() << ")" << std::endl;
        mNotify->notifyChessWatchState(sender_id, gameId, whiteId, whiteName, blackId, blackName, fen, sequence, lastFrom, lastTo, moves);

    } else if (type == "chess_watch_action") {
        const QString gameId = map.value("game_id").toString();
        const QString action = map.value("action").toString();
        mNotify->notifyChessWatchAction(sender_id, gameId, action);

    } else if (type == "chess_watch_end") {
        const QString gameId = map.value("game_id").toString();
        const QString reason = map.value("reason").toString();
        mNotify->notifyChessWatchEnd(sender_id, gameId, reason);

    } else if (type == "chess_watch_leave") {
        const QString gameId = map.value("game_id").toString();
        RsStackMutex stack(mRetroChessMtx);
        for (auto &entry : mSpectatorsByGame) {
            if (gameId.isEmpty() || gameId.contains(QString::fromStdString(entry.first))) {
                entry.second.erase(sender_id);
            }
        }

    } else if (type == "leaderboard_receipt" || type == "leaderboard_sync" || type == "leaderboard_sync_req") {
        mNotify->notifyLeaderboardDataGxs(sender_id, QByteArray((const char*)data, data_size));

    } else {
        // Chess move: format "col,row,count"
        QStringList parts = QString::fromUtf8((const char*)data, data_size).split(",");
        if (parts.size() == 3) {
            int col   = parts[0].toInt();
            int row   = parts[1].toInt();
            int count = parts[2].toInt();
            mNotify->notifyChessMoveGxs(sender_id, col, row, count);
        } else {
            std::cerr << "Chess: Unknown message type '" << type.toStdString() << "' ignored" << std::endl;
        }
    }
}

void p3RetroChess::player_leave_gxs(const RsGxsId &gxs_id) {
    RsGxsTunnelId tunnelId;
    std::vector<RsGxsTunnelId> spectatorTunnels;
    {
        RsStackMutex stack(mRetroChessMtx);
        auto it = mActiveTunnels.find(gxs_id);
        if (it != mActiveTunnels.end()) tunnelId = it->second;

        auto specIt = mSpectatorsByGame.find(gxs_id.toStdString());
        if (specIt != mSpectatorsByGame.end()) {
            for (const auto &specId : specIt->second) {
                auto tIt = mActiveTunnels.find(specId);
                if (tIt != mActiveTunnels.end())
                    spectatorTunnels.push_back(tIt->second);
            }
            mSpectatorsByGame.erase(specIt);
        }
    }
    if (tunnelId.isNull() || !mGxsTunnels) return;

    const std::string leave = "{\"type\":\"player_leave\"}";
    if (mGxsTunnels->sendData(tunnelId, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                              (const uint8_t*)leave.c_str(), leave.size())) {
        RsStackMutex stack(mRetroChessMtx);
        mPendingGxsCloses[gxs_id] = time(NULL) + 2;
    }

    if (!spectatorTunnels.empty()) {
        QVariantMap endMsg;
        endMsg["type"] = "chess_watch_end";
        endMsg["version"] = 1;
        endMsg["game_id"] = QString::fromStdString(gxs_id.toStdString());
        endMsg["reason"] = "Player left";
        const QByteArray bytes = QJsonDocument::fromVariant(endMsg).toJson(QJsonDocument::Compact);
        for (const auto &tId : spectatorTunnels) {
            mGxsTunnels->sendData(tId, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                                  reinterpret_cast<const uint8_t*>(bytes.constData()), bytes.size());
        }
    }
}

void p3RetroChess::closePendingGxsTunnels()
{
    std::vector<RsGxsTunnelId> tunnelsToClose;
    const time_t now = time(NULL);
    {
        RsStackMutex stack(mRetroChessMtx);
        for (auto it = mPendingGxsCloses.begin(); it != mPendingGxsCloses.end();) {
            if (it->second > now) { ++it; continue; }
            auto activeIt = mActiveTunnels.find(it->first);
            if (activeIt != mActiveTunnels.end()) {
                tunnelsToClose.push_back(activeIt->second);
                mActiveTunnels.erase(activeIt);
            }
            it = mPendingGxsCloses.erase(it);
        }
    }
    for (const RsGxsTunnelId &tunnelId : tunnelsToClose)
        mGxsTunnels->closeExistingTunnel(tunnelId, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID);
}

RsGxsId p3RetroChess::findGxsIdByTunnel(const RsGxsTunnelId& tunnel_id)
{
    std::map<RsGxsId, RsGxsTunnelId>::iterator it;
    for (it = mActiveTunnels.begin(); it != mActiveTunnels.end(); ++it) {
        if (it->second == tunnel_id) return it->first;
    }
    return RsGxsId();
}

// services/p3RetroChess.cc

void p3RetroChess::notifyTunnelStatus(const RsGxsTunnelId& tunnel_id, uint32_t tunnel_status)
{
    // React to tunnel being closed or going down
    if (tunnel_status == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_REMOTELY_CLOSED ||
        tunnel_status == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_TUNNEL_DN)
    {
        RsGxsId gxs_id;
        {
            RsStackMutex stack(mRetroChessMtx);
            // Search active tunnels for the closed tunnel
            for (auto it = mActiveTunnels.begin(); it != mActiveTunnels.end(); ++it) {
                if (it->second == tunnel_id) {
                    gxs_id = it->first;
                    mActiveTunnels.erase(it);
                    break;
                }
            }
            // Also clean up mapping
            mTunnelToGxsIdMap.erase(tunnel_id);
            if (!gxs_id.isNull()) {
                auto contact = mChessContacts.find(gxs_id);
                if (contact != mChessContacts.end()) {
                    contact->second.status = "offline";
                    contact->second.seeking = false;
                    contact->second.seekTimeControl = ChessTimeControl{};
                    contact->second.deadline = 0;
                    contact->second.nonce.clear();
                    contact->second.nextProbe = time(nullptr) + 15;
                }
                mInvitesFromGxs.erase(gxs_id);
                mJoinRequestsFromGxs.erase(gxs_id);
                mInvitesToGxs.erase(gxs_id);
                mPendingGxsInvites.erase(gxs_id);
                mPendingGxsCloses.erase(gxs_id);
                // Re-populated on the next invite; without this the map grew
                // for the whole session.
                mOwnGxsIdByPeer.erase(gxs_id);
            }
        }
        if (!gxs_id.isNull()) {
            std::cout << "Chess: Tunnel closed for GXS " << gxs_id << std::endl;
            mNotify->notifyGxsTunnelClosed(gxs_id);
            mNotify->notifyAvailablePeersChanged();
        }
    }
}

void p3RetroChess::receiveData(const RsGxsTunnelId& id, unsigned char *data, uint32_t data_size)
{
    // Consume denied packets here as well: this core transfers buffer ownership
    // to the client and does not free a packet rejected by acceptDataFromPeer.
    RsGxsTunnelService::GxsTunnelInfo info;
    bool allowed = mGxsTunnels && mGxsTunnels->getTunnelInfo(id, info)
            && rsIdentity && !rsIdentity->isOwnId(info.destination_gxs_id);
    if (allowed && !chessIdentityEnabled(info.source_gxs_id)) {
        RsStackMutex stack(mRetroChessMtx);
        auto game = mGameSessions.find(info.destination_gxs_id.toStdString());
        allowed = game != mGameSessions.end()
                && game->second.localIdentityId == QString::fromStdString(info.source_gxs_id.toStdString());
    }
    if (!allowed) { free(data); return; }

    // Look up the GXS ID that was stored in acceptDataFromPeer() — don't pass empty one
    RsGxsId sender_gxs_id;
    {
        RsStackMutex stack(mRetroChessMtx);
        auto it = mTunnelToGxsIdMap.find(id);
        if (it != mTunnelToGxsIdMap.end())
            sender_gxs_id = it->second;
    }
    handleRawData(sender_gxs_id, id, false, (const uint8_t*)data, data_size);
    free(data); // RS tunnel service transfers ownership
}

void p3RetroChess::connectToGxsTunnelService(RsGxsTunnelService *tunnel_service)
{
    mGxsTunnels = tunnel_service;
    if (mGxsTunnels) {
        std::cout << "Chess: Registering GXS Tunnel client (Service ID: " << RETRO_CHESS_GXS_TUNNEL_SERVICE_ID << ")" << std::endl;
        if (!mGxsTunnels->registerClientService(RETRO_CHESS_GXS_TUNNEL_SERVICE_ID, this)) {
            std::cerr << "Chess: FAILED to register GXS Tunnel client!" << std::endl;
        }
    }
}

bool p3RetroChess::acceptDataFromPeer(const RsGxsId& gxs_id, const RsGxsTunnelId& tunnel_id, bool am_I_client_side)
{
    Q_UNUSED(am_I_client_side);
    RsGxsTunnelService::GxsTunnelInfo tunnelInfo;
    const bool haveTunnelInfo = mGxsTunnels && mGxsTunnels->getTunnelInfo(tunnel_id, tunnelInfo);
    if (!haveTunnelInfo || !rsIdentity || rsIdentity->isOwnId(gxs_id)) return true;
    if (!chessIdentityEnabled(tunnelInfo.source_gxs_id)) {
        RsStackMutex stack(mRetroChessMtx);
        auto game = mGameSessions.find(gxs_id.toStdString());
        if (game == mGameSessions.end() || game->second.localIdentityId != QString::fromStdString(tunnelInfo.source_gxs_id.toStdString())) return true;
    }
    {
        RsStackMutex stack(mRetroChessMtx);
        // Store the mapping so receiveData / handleRawData can identify the sender
        mTunnelToGxsIdMap[tunnel_id] = gxs_id;
        // A presence-only visitor needs no persistent peer-to-local identity
        // entry. Incoming invitations and outgoing tunnel requests record the
        // local identity themselves. Otherwise probe-only peers accumulate here
        // because they never enter mActiveTunnels' disconnect cleanup path.
    }
    return true;
}

ChessTimeControl p3RetroChess::timeControlForPeer(const RsGxsId &gxsId)
{
    RsStackMutex stack(mRetroChessMtx);
    auto it = mInviteTimeControlByPeer.find(gxsId);
    if (it != mInviteTimeControlByPeer.end()) return it->second;
    auto cIt = mChessContacts.find(gxsId);
    if (cIt != mChessContacts.end() && !cIt->second.seekTimeControl.unlimited)
        return cIt->second.seekTimeControl;
    return ChessTimeControl{};
}

void p3RetroChess::setTimeControlForPeer(const RsGxsId &gxsId, const ChessTimeControl &tc)
{
    RsStackMutex stack(mRetroChessMtx);
    mInviteTimeControlByPeer[gxsId] = tc;
}

void p3RetroChess::setLobbySeek(bool active, const ChessTimeControl &tc)
{
    std::vector<RsGxsTunnelId> tunnels;
    {
        RsStackMutex stack(mRetroChessMtx);
        mLobbySeekActive = active;
        mLobbySeek = active ? tc : ChessTimeControl{};
        for (const auto &entry : mActiveTunnels) tunnels.push_back(entry.second);
    }
    QVariantMap message;
    message["type"] = "chess_seek";
    message["seeking"] = active;
    if (active) message["tc"] = tc.toNetString();
    const QByteArray bytes = QJsonDocument::fromVariant(message).toJson(QJsonDocument::Compact);
    // Lobby advertisements are top-level packets, not game_action payloads.
    for (const auto &tunnel : tunnels)
        if (mGxsTunnels) mGxsTunnels->sendData(tunnel, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                reinterpret_cast<const uint8_t*>(bytes.constData()), bytes.size());
}

