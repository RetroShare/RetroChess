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
#include <retroshare/rsinit.h>

#include "services/RetroChessTunnelDebug.h"
#include "gui/RetroChessFlair.h"

using RetroChessTunnelDebug::statusName;
using RetroChessTunnelDebug::payloadType;

// Presence retry schedule after consecutive failed probes of a chess contact.
// Stay responsive for the first retries, then back off so that offline
// contacts do not cause a new GXS tunnel discovery + DH handshake every few
// minutes. After kPresenceMaxFailures failures the contact is considered
// dormant and only re-checked hourly; an incoming probe from that contact
// wakes it up immediately (see handleChessPresence).
static const unsigned int kPresenceMaxFailures = 7;
static const time_t kPresenceDormantRetrySec = 3600;
// Tie-break: when both sides have each other as contact, only the side with
// the lower GXS id dials. The other one waits this long for the incoming
// tunnel before dialing itself (for contacts that do not list us).
static const time_t kPresenceYieldSec = 75;
// Orphan sweep and debug state dump interval.
static const time_t kTunnelMaintenanceIntervalSec = 60;

static time_t chessPresenceRetryDelay(unsigned int failures)
{
    static const time_t kDelays[6] = { 15, 30, 60, 120, 240, 300 };
    if (failures == 0) return kDelays[0];
    if (failures > 6) return kPresenceDormantRetrySec;
    return kDelays[failures - 1];
}


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
	RetroChessTunnelDebug::setLogDirectory(RsAccounts::AccountDirectory());
	CHESS_TLOG("service created (tunnel debug enabled from RETROCHESS_DEBUG)");


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
	closeQueuedGxsTunnels();
	retryPendingDistantChatInvites();
	reconnectInterruptedSessions();
	tickChessPresence();
	sweepOrphanGxsTunnels();
	dumpTunnelState();
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
			// Keep the presence backoff across restarts, so contacts that
			// were offline do not get the whole retry burst again after
			// every restart. Separate key: older versions ignore it.
			if (entry.second.failures > 0) {
				RsTlvKeyValue backoff;
				backoff.key = "CHESS_CONTACT_BACKOFF:" + entry.first.toStdString();
				backoff.value = std::to_string(entry.second.failures) + ","
				        + std::to_string(entry.second.nextProbe);
				vitem->tlvkvs.pairs.push_back(backoff);
			}
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
		// Flair: ours per identity, and the last one each saved contact sent
		// (so the player list shows it before the next presence reply).
		for (const auto &entry : mOwnFlair) {
			RsTlvKeyValue value;
			value.key = "CHESS_FLAIR:" + entry.first.toStdString();
			value.value = entry.second.toStdString();
			vitem->tlvkvs.pairs.push_back(value);
		}
		for (const auto &entry : mPeerFlair) {
			if (!mChessContacts.count(entry.first)) continue;
			RsTlvKeyValue value;
			value.key = "CHESS_PEER_FLAIR:" + entry.first.toStdString();
			value.value = entry.second.toStdString();
			vitem->tlvkvs.pairs.push_back(value);
		}
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
	CHESS_TLOG("CONFIG saved (" << vitem->tlvkvs.pairs.size() << " entries)");

	return true ;
}
bool p3RetroChess::loadList(std::list<RsItem*>& load)
{
	for (RsItem *item : load) {
		if (RsConfigKeyValueSet *values = dynamic_cast<RsConfigKeyValueSet *>(item)) {
			for (const RsTlvKeyValue &value : values->tlvkvs.pairs) {
				if (value.key.compare(0, 22, "CHESS_CONTACT_BACKOFF:") == 0) {
					const RsGxsId id(value.key.substr(22));
					const QStringList parts = QString::fromStdString(value.value).split(',');
					if (!id.isNull() && parts.size() == 2) {
						RsStackMutex stack(mRetroChessMtx);
						ChessContact &contact = mChessContacts[id];
						contact.failures = std::min<unsigned int>(parts[0].toUInt(), kPresenceMaxFailures);
						// A past time simply means one probe right after start;
						// if it fails the backoff continues from the saved count.
						contact.nextProbe = static_cast<time_t>(parts[1].toLongLong());
						CHESS_TLOG("CONFIG backoff loaded peer=" << id << " failures=" << contact.failures);
					}
					continue;
				}
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
				if (value.key.compare(0, 12, "CHESS_FLAIR:") == 0
				        || value.key.compare(0, 17, "CHESS_PEER_FLAIR:") == 0) {
					const bool own = value.key.compare(0, 12, "CHESS_FLAIR:") == 0;
					const RsGxsId id(value.key.substr(own ? 12 : 17));
					const QString flair = RetroChessFlair::normalize(QString::fromStdString(value.value));
					if (!id.isNull() && !flair.isEmpty()) {
						RsStackMutex stack(mRetroChessMtx);
						(own ? mOwnFlair : mPeerFlair)[id] = flair;
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

	// Fallback when no saved backoff exists (older config, or the client was
	// closed before the config was written): contacts not seen for a day, or
	// never seen, get one probe at start and go dormant if it fails, instead
	// of the whole fast retry burst. They can still reach us at any time.
	{
		const time_t now = time(nullptr);
		unsigned int restored = 0, assumedOffline = 0;
		RsStackMutex stack(mRetroChessMtx);
		for (auto &entry : mChessContacts) {
			ChessContact &contact = entry.second;
			if (contact.failures > 0) { ++restored; continue; }
			if (contact.lastSeen == 0 || now - contact.lastSeen > 24 * 3600) {
				contact.failures = kPresenceMaxFailures - 1;
				++assumedOffline;
			}
		}
		CHESS_TLOG("CONFIG loaded contacts=" << mChessContacts.size() << " backoff restored=" << restored
		           << " assumed offline (not seen for 24h)=" << assumedOffline);
	}
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
			sendGxsData(tId, reinterpret_cast<const uint8_t*>(replyBytes.constData()), replyBytes.size());
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
			sendGxsData(tId, reinterpret_cast<const uint8_t*>(bytes.constData()), bytes.size());
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
    if (mGxsTunnels && !close.isNull()) closeGxsTunnel(close, "contact removed");
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
            entry.second.failures = 0;
            entry.second.yieldUntil = 0;
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
    for (const auto &tunnel : close) if (mGxsTunnels) closeGxsTunnel(tunnel, "chess identities changed");
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

QString p3RetroChess::ownFlair(const RsGxsId &ownId)
{
    RsStackMutex stack(mRetroChessMtx);
    auto it = mOwnFlair.find(ownId);
    return it == mOwnFlair.end() ? QString() : it->second;
}

void p3RetroChess::setOwnFlair(const RsGxsId &ownId, const QString &flair)
{
    if (ownId.isNull() || !rsIdentity || !rsIdentity->isOwnId(ownId)) return;
    const QString valid = RetroChessFlair::normalize(flair);
    std::vector<RsGxsTunnelId> tunnels;
    {
        RsStackMutex stack(mRetroChessMtx);
        auto it = mOwnFlair.find(ownId);
        const QString previous = it == mOwnFlair.end() ? QString() : it->second;
        if (previous == valid) return;
        if (valid.isEmpty()) mOwnFlair.erase(ownId);
        else mOwnFlair[ownId] = valid;
        // Tell peers we are already connected to with this identity right away;
        // everyone else gets it with the next presence reply, invite or accept.
        for (const auto &entry : mActiveTunnels) {
            auto own = mOwnGxsIdByPeer.find(entry.first);
            if (own != mOwnGxsIdByPeer.end() && own->second == ownId
                    && chessPeerConfirmedLocked(entry.first))
                tunnels.push_back(entry.second);
        }
    }
    CHESS_TLOG("FLAIR own identity=" << ownId << " set to '" << valid.toStdString()
               << "', updating " << tunnels.size() << " connected peer(s)");
    // An explicit empty "flair" clears it on the other side.
    QJsonObject update{{"type", "chess_flair"}, {"flair", valid}};
    const QByteArray bytes = QJsonDocument(update).toJson(QJsonDocument::Compact);
    for (const auto &tunnel : tunnels)
        sendGxsData(tunnel, reinterpret_cast<const uint8_t*>(bytes.constData()), bytes.size());
    IndicateConfigChanged();
    mNotify->notifyPlayerFlairChanged(ownId);
}

QString p3RetroChess::playerFlair(const RsGxsId &id)
{
    RsStackMutex stack(mRetroChessMtx);
    auto own = mOwnFlair.find(id);
    if (own != mOwnFlair.end()) return own->second;
    auto peer = mPeerFlair.find(id);
    return peer == mPeerFlair.end() ? QString() : peer->second;
}

void p3RetroChess::addOwnFlairLocked(QVariantMap &message, const RsGxsId &ownId) const
{
    auto it = mOwnFlair.find(ownId);
    message["flair"] = it == mOwnFlair.end() ? QString() : it->second;
}

void p3RetroChess::addOwnFlairLocked(QJsonObject &message, const RsGxsId &ownId) const
{
    auto it = mOwnFlair.find(ownId);
    message["flair"] = it == mOwnFlair.end() ? QString() : it->second;
}

void p3RetroChess::learnPeerFlair(const RsGxsId &sender, const QVariantMap &message)
{
    // No "flair" key: an older RetroChess, or a message that does not carry
    // it. Keep what we know. An empty or unknown value clears it.
    if (sender.isNull() || !message.contains("flair")) return;
    if (rsIdentity && rsIdentity->isOwnId(sender)) return;
    const QString flair = RetroChessFlair::normalize(message.value("flair").toString());
    bool saved = false;
    {
        RsStackMutex stack(mRetroChessMtx);
        auto it = mPeerFlair.find(sender);
        const QString previous = it == mPeerFlair.end() ? QString() : it->second;
        if (previous == flair) return;
        if (flair.isEmpty()) {
            mPeerFlair.erase(sender);
        } else {
            // Bound memory: identities that are not contacts can be anyone.
            if (it == mPeerFlair.end() && mPeerFlair.size() >= 4096
                    && !mChessContacts.count(sender)) return;
            mPeerFlair[sender] = flair;
        }
        saved = mChessContacts.count(sender) > 0;
    }
    CHESS_TLOG("FLAIR peer=" << sender << " is now '" << flair.toStdString() << "'");
    if (saved) IndicateConfigChanged();
    mNotify->notifyPlayerFlairChanged(sender);
}

void p3RetroChess::tickChessPresence()
{
    if (!mGxsTunnels || !rsIdentity) return;
    const time_t now = time(nullptr);
    const RsGxsId preferred = preferredChessIdentity();
    const bool enabled = !preferred.isNull();
    std::list<RsGxsId> ownIds;
    rsIdentity->getOwnIds(ownIds);
    std::vector<RsGxsId> open;
    std::vector<RsGxsTunnelId> close;
    std::vector<std::pair<RsGxsTunnelId, QByteArray>> sends;
    bool changed = false;
    bool backoffChanged = false;
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
                contact.failures = std::min(contact.failures + 1, kPresenceMaxFailures);
                contact.nextProbe = now + chessPresenceRetryDelay(contact.failures);
                backoffChanged = true;
                CHESS_TLOG("PRESENCE timeout peer=" << id << " failures=" << contact.failures
                           << " next probe in " << (contact.nextProbe - now) << "s"
                           << (contact.failures >= kPresenceMaxFailures ? " (dormant: hourly checks only)" : ""));
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
            const bool needsTunnel = !mActiveTunnels.count(id) && !mPendingTunnels.count(id);
            bool yielding = false;
            if (!needsTunnel) contact.yieldUntil = 0;
            if (enabled && !contact.deadline && now >= contact.nextProbe && needsTunnel) {
                // Both sides dialing each other produce the same (symmetric)
                // GXS tunnel id, and the second handshake overwrites the first
                // inside GxsTunnel, making the tunnel flap. Let the lower id dial.
                auto own = mOwnGxsIdByPeer.find(id);
                const RsGxsId ownId = own != mOwnGxsIdByPeer.end() ? own->second : preferred;
                if (id < ownId) {
                    if (!contact.yieldUntil) {
                        contact.yieldUntil = now + kPresenceYieldSec;
                        CHESS_TLOG("PRESENCE yield peer=" << id << ": waiting " << kPresenceYieldSec
                                   << "s for the peer to open the tunnel");
                    }
                    yielding = now < contact.yieldUntil;
                }
            }
            if (enabled && !yielding && !contact.deadline && now >= contact.nextProbe
                    && (mActiveTunnels.count(id) || inFlight < 4)) {
                contact.yieldUntil = 0;
                // GXS discovery and its key exchange need their own connection budget.
                contact.deadline = now + 120;
                contact.nonce.clear();
                if (!mActiveTunnels.count(id)) ++inFlight;
                if (contact.status == "unknown" || contact.status == "offline") {
                    contact.status = "checking";
                    changed = true;
                }
                if (needsTunnel) {
                    open.push_back(id);
                    CHESS_TLOG("PRESENCE open tunnel to peer=" << id << " (failures=" << contact.failures << ")");
                }
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
                auto ownForProbe = mOwnGxsIdByPeer.find(id);
                addOwnFlairLocked(message, ownForProbe != mOwnGxsIdByPeer.end() ? ownForProbe->second : preferred);
                CHESS_TLOG("PRESENCE probe peer=" << id << " tunnel=" << active->second);
                sends.push_back({active->second, QJsonDocument::fromVariant(message).toJson(QJsonDocument::Compact)});
            }
        }
    }
    for (const auto &tunnel : close) closeGxsTunnel(tunnel, "presence probe timed out");
    for (const auto &id : open) requestGxsTunnel(id);
    for (const auto &send : sends) sendGxsData(send.first, reinterpret_cast<const uint8_t*>(send.second.constData()), send.second.size());
    // Called outside mRetroChessMtx: the config thread holds its own lock
    // while calling saveList(), which takes mRetroChessMtx.
    if (backoffChanged) IndicateConfigChanged();
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
            addOwnFlairLocked(reply, info.source_gxs_id);
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
                    contact.yieldUntil = 0;
                    CHESS_TLOG("PRESENCE adopt incoming tunnel=" << tunnel << " from contact " << sender
                               << " (was " << contact.status.toStdString() << ", failures=" << contact.failures << ")");
                    contact.deadline = now + 45;
                    contact.nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
                    contact.probeTunnel = tunnel;
                    if (contact.status == "offline" || contact.status == "unknown")
                        contact.status = "checking";
                    QVariantMap probe;
                    probe["type"] = "chess_presence_request";
                    probe["version"] = 1;
                    probe["nonce"] = contact.nonce;
                    addOwnFlairLocked(probe, info.source_gxs_id);
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
        sendGxsData(tunnel, reinterpret_cast<const uint8_t*>(bytes.constData()), bytes.size());
        if (!reciprocalProbe.isEmpty()) {
            sendGxsData(tunnel, reinterpret_cast<const uint8_t*>(reciprocalProbe.constData()), reciprocalProbe.size());
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
    sendGxsData(tunnel_id, (const uint8_t*)msg.c_str(), msg.size());
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

    // Open a tunnel using mGxsTunnel (Async Request)
    if (openGxsTunnel(to_gxs_id, from_gxs_id, tunnel_id, "sendGxsInvite/requestGxsTunnel"))
    {
        {
            RsStackMutex stack(mRetroChessMtx);
            mPendingTunnels[to_gxs_id] = tunnel_id;
            mOpenedTunnels.insert(tunnel_id);
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
            CHESS_INVLOG("CANCEL invite to=" << gxsId << " (was still queued, nothing sent)");
        } else {
            auto active = mActiveTunnels.find(gxsId);
            if (active == mActiveTunnels.end() || !mGxsTunnels) return false;
            tunnelId = active->second;
        }
    }
    if (!tunnelId.isNull()) {
        const std::string message = "{\"type\":\"chess_cancel\"}";
        if (!sendGxsData(tunnelId, reinterpret_cast<const uint8_t*>(message.data()), message.size())) {
            CHESS_INVLOG("CANCEL invite to=" << gxsId << " failed to send chess_cancel");
            return false;
        }
        CHESS_INVLOG("CANCEL invite to=" << gxsId << " sent chess_cancel over tunnel=" << tunnelId);
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
            auto ownIt = mOwnGxsIdByPeer.find(gxsId);
            if (ownIt != mOwnGxsIdByPeer.end()) addOwnFlairLocked(acceptObj, ownIt->second);
            auto tcIt = mInviteTimeControlByPeer.find(gxsId);
            if (tcIt != mInviteTimeControlByPeer.end() && !tcIt->second.unlimited)
                acceptObj["tc"] = tcIt->second.toNetString();
            mPendingGxsInvites[gxsId] = QJsonDocument(acceptObj).toJson(QJsonDocument::Compact).toStdString();
        }
    }
    if (activeTunnel.isNull())
        CHESS_INVLOG("ACCEPT invite from=" << gxsId << " queued (no open tunnel, requesting one)");

    if (!activeTunnel.isNull()) {
        // Tunnel already active (server side — invite arrived over it).
        // Send chess_accept immediately, outside the mutex.
        if (!mGxsTunnels) return;
        QJsonObject acceptObj{{"type", "chess_accept"}, {"game_id", gameIdForPeer(gxsId)}};
        {
            RsStackMutex stack(mRetroChessMtx);
            auto ownIt = mOwnGxsIdByPeer.find(gxsId);
            if (ownIt != mOwnGxsIdByPeer.end()) addOwnFlairLocked(acceptObj, ownIt->second);
            auto tcIt = mInviteTimeControlByPeer.find(gxsId);
            if (tcIt != mInviteTimeControlByPeer.end() && !tcIt->second.unlimited)
                acceptObj["tc"] = tcIt->second.toNetString();
        }
        const std::string accept = QJsonDocument(acceptObj).toJson(QJsonDocument::Compact).toStdString();
        std::cout << "Chess: Sending chess_accept over tunnel " << activeTunnel << std::endl;
        const bool sent = sendGxsData(activeTunnel, (const uint8_t*)accept.c_str(), accept.size());
        CHESS_INVLOG("ACCEPT invite from=" << gxsId << " sent chess_accept over tunnel=" << activeTunnel
                     << " ok=" << sent);
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
    if (!sendGxsData(tunnelId, reinterpret_cast<const uint8_t*>(reply.data()), reply.size())) {
        CHESS_INVLOG("REJECT invite from=" << gxsId << " failed to send chess_reject");
        return false;
    }
    CHESS_INVLOG("REJECT invite from=" << gxsId << " sent chess_reject");
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
    return sendGxsData(tunnelId, reinterpret_cast<const uint8_t*>(message.data()), message.size());
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
    bool sent = sendGxsData(tunnelId, reinterpret_cast<const uint8_t*>(message.constData()), message.size());

    if (!spectatorTunnels.empty() && mGxsTunnels) {
        CHESS_SPECLOG("RELAY own action " << QString::fromStdString(action).section(':', 0, 1).toStdString()
                      << " game=" << gxsId << " to " << spectatorTunnels.size() << " spectator(s)");
        QVariantMap specMap;
        specMap.insert("type", "chess_watch_action");
        specMap.insert("version", 1);
        specMap.insert("game_id", QString::fromStdString(gxsId.toStdString()));
        specMap.insert("action", QString::fromStdString(action));
        const QByteArray specMsg = QJsonDocument::fromVariant(specMap).toJson(QJsonDocument::Compact);
        for (const auto &sTunnel : spectatorTunnels) {
            sendGxsData(sTunnel, reinterpret_cast<const uint8_t*>(specMsg.constData()), specMsg.size());
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
        CHESS_SPECLOG("SEND watch request to host=" << hostPlayerId << " game=" << gameKey.toStdString()
                      << " over open tunnel=" << tunnelId);
        QVariantMap req;
        req["type"] = "chess_watch_req";
        req["version"] = 1;
        req["game_id"] = gameKey;
        const QByteArray bytes = QJsonDocument::fromVariant(req).toJson(QJsonDocument::Compact);
        return sendGxsData(tunnelId, reinterpret_cast<const uint8_t*>(bytes.constData()), bytes.size());
    }

    std::cout << "Chess: Queueing watch request and requesting tunnel for host " << hostPlayerId
              << " for game " << gameKey.toStdString() << std::endl;
    CHESS_SPECLOG("QUEUED watch request to host=" << hostPlayerId << " game=" << gameKey.toStdString()
                  << " (no tunnel, requesting one)");
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
        sendGxsData(tunnelId, reinterpret_cast<const uint8_t*>(bytes.constData()), bytes.size());
        CHESS_SPECLOG("SEND watch leave to host=" << hostPlayerId << " game=" << gameKey.toStdString());
    }
}

std::vector<RsGxsId> p3RetroChess::gameSpectators(const RsGxsId &opponent)
{
    RsStackMutex stack(mRetroChessMtx);
    std::vector<RsGxsId> spectators;
    const std::string endpoint = opponent.toStdString();
    if (mGameSessions.find(endpoint) == mGameSessions.end()) return spectators;
    const auto found = mSpectatorsByGame.find(endpoint);
    if (found != mSpectatorsByGame.end())
        for (const auto &id : found->second)
            if (mActiveTunnels.count(id)) spectators.push_back(id);
    return spectators;
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
    return sendGxsData(tunnelId, reinterpret_cast<const uint8_t*>(data.constData()), data.size());
}

void p3RetroChess::broadcastLeaderboardDataGxs(const QByteArray &data)
{
    std::vector<RsGxsTunnelId> tunnels;
    {
        RsStackMutex stack(mRetroChessMtx);
        if (!mGxsTunnels) return;
        for (const auto &entry : mActiveTunnels)
            if (chessPeerConfirmedLocked(entry.first))
                tunnels.push_back(entry.second);
    }
    for (const auto &tunnelId : tunnels)
        sendGxsData(tunnelId, reinterpret_cast<const uint8_t*>(data.constData()), data.size());
}

std::vector<RsGxsId> p3RetroChess::activeGxsTunnels()
{
    RsStackMutex stack(mRetroChessMtx);
    std::vector<RsGxsId> peers;
    for (const auto &entry : mActiveTunnels)
        if (chessPeerConfirmedLocked(entry.first))
            peers.push_back(entry.first);
    return peers;
}

bool p3RetroChess::chessPeerConfirmedLocked(const RsGxsId &id) const
{
    // A game, invitation or watch request always keeps its tunnel usable.
    if (mGameSessions.count(id.toStdString()) || mInvitesToGxs.count(id)
            || mInvitesFromGxs.count(id) || mPendingWatchRequests.count(id))
        return true;
    auto contact = mChessContacts.find(id);
    // Not a presence contact: the tunnel exists for some other chess reason.
    if (contact == mChessContacts.end()) return true;
    const QString &status = contact->second.status;
    return status == "available" || status == "playing" || status == "busy";
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
            CHESS_INVLOG("SEND invite to=" << toId << " refused: we are busy");
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
        addOwnFlairLocked(inviteJson, ownId);
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
        if (sendGxsData(activeTunnel, reinterpret_cast<const uint8_t*>(invite.data()), invite.size()))
        {
            CHESS_INVLOG("SEND invite to=" << toId << " from=" << ownId
                         << " game=" << inviteJson.value("game_id").toString().toStdString()
                         << " tc=" << (inviteJson.contains("tc") ? inviteJson.value("tc").toString().toStdString() : "unlimited")
                         << " join_open_game=" << joinOpenGame << " over open tunnel=" << activeTunnel);
            RsStackMutex stack(mRetroChessMtx);
            mInvitesToGxs.insert(toId);
            return true;
        }
        CHESS_INVLOG("SEND invite to=" << toId << " failed on stale tunnel=" << activeTunnel
                     << ", opening a new tunnel");

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
            CHESS_INVLOG("QUEUED invite to=" << toId << " game="
                         << inviteJson.value("game_id").toString().toStdString()
                         << " (tunnel already opening, sent when it is ready)");
            return true;
        }
    }

    RsGxsTunnelId tunnelId;
    if (openGxsTunnel(toId, ownId, tunnelId, "invite"))
    {
        {
            RsStackMutex stack(mRetroChessMtx);
            mPendingTunnels[toId] = tunnelId;
            mOpenedTunnels.insert(tunnelId);
            mPendingGxsInvites[toId] = invite;
            mInvitesToGxs.insert(toId);
            std::cout << "Chess: Tunnel requested (id=" << tunnelId << "), invite queued for " << toId << std::endl;
        }
        CHESS_INVLOG("QUEUED invite to=" << toId << " game="
                     << inviteJson.value("game_id").toString().toStdString()
                     << " (no tunnel, requested tunnel=" << tunnelId << ", sent when it is ready)");
        mNotify->notifyAvailablePeersChanged();
        return true;
    } else {
        std::cerr << "Chess: doSendInviteOverGxs: requestSecuredTunnel failed" << std::endl;
        CHESS_INVLOG("SEND invite to=" << toId << " failed: tunnel request failed");
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

    // A tunnel that is neither CAN_TALK nor remotely closed within this time is
    // given up: the queued invite is dropped and the tunnel closed.
    static const time_t kPendingTunnelTimeoutSec = 120;
    const time_t nowTs = time(nullptr);

    std::map<RsGxsId, RsGxsTunnelId> pending;
    std::map<RsGxsId, time_t> startedAt;
    {
        RsStackMutex stack(mRetroChessMtx); /****** LOCKED MUTEX *******/
        pending = mPendingTunnels;
        // Drop bookkeeping of requests that are gone or were replaced by a new tunnel.
        for (auto sit = mPendingTunnelSince.begin(); sit != mPendingTunnelSince.end();) {
            auto pit = pending.find(sit->first);
            if (pit == pending.end() || pit->second != sit->second.first)
                sit = mPendingTunnelSince.erase(sit);
            else
                ++sit;
        }
        for (auto pit = pending.begin(); pit != pending.end(); ++pit) {
            auto sit = mPendingTunnelSince.find(pit->first);
            if (sit == mPendingTunnelSince.end())
                sit = mPendingTunnelSince.insert(std::make_pair(pit->first,
                        std::make_pair(pit->second, nowTs))).first;
            startedAt[pit->first] = sit->second.second;
        }
    }
    if (pending.empty()) return;
    std::list<RsGxsTunnelId> expired;

    std::list<std::pair<RsGxsTunnelId, std::string> > flushes;
    std::list<RsGxsId> ready;
    bool pendingChanged = false;

    for (auto it = pending.begin(); it != pending.end(); ++it) {
        RsGxsTunnelService::GxsTunnelInfo tinfo;
        const bool haveInfo = mGxsTunnels->getTunnelInfo(it->second, tinfo);
        if (!haveInfo
                || (tinfo.tunnel_status != RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_CAN_TALK
                    && tinfo.tunnel_status != RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_REMOTELY_CLOSED)) {
            // Still connecting (TUNNEL_DN is also the engine's initial state) or
            // unknown to the tunnel service: keep waiting, but not forever.
            if (nowTs - startedAt[it->first] > kPendingTunnelTimeoutSec) {
                RsStackMutex stack(mRetroChessMtx); /****** LOCKED MUTEX *******/
                auto pit = mPendingTunnels.find(it->first);
                if (pit != mPendingTunnels.end() && pit->second == it->second) {
                    std::cerr << "Chess: tunnel to " << it->first << " did not come up in "
                              << kPendingTunnelTimeoutSec << "s, giving up" << std::endl;
                    CHESS_TLOG("PENDING timeout peer=" << it->first << " tunnel=" << it->second
                               << " status=" << (haveInfo ? statusName(tinfo.tunnel_status) : "unknown to GxsTunnel"));
                    if (mPendingGxsInvites.count(it->first))
                        CHESS_INVLOG("DROPPED queued "
                                     << payloadType(reinterpret_cast<const uint8_t*>(mPendingGxsInvites[it->first].data()),
                                                    mPendingGxsInvites[it->first].size())
                                     << " to=" << it->first << ": tunnel did not come up in "
                                     << kPendingTunnelTimeoutSec << "s");
                    if (mPendingWatchRequests.count(it->first))
                        CHESS_SPECLOG("DROPPED queued watch request to host=" << it->first
                                      << ": tunnel did not come up in " << kPendingTunnelTimeoutSec << "s");
                    mPendingGxsInvites.erase(it->first);
                    mPendingWatchRequests.erase(it->first);
                    mInvitesToGxs.erase(it->first);
                    mPendingTunnelSince.erase(it->first);
                    mPendingTunnels.erase(pit);
                    expired.push_back(it->second);
                    pendingChanged = true;
                }
            }
            continue;
        }

        // Check if the tunnel is "Connected" (CAN_TALK)
        if (tinfo.tunnel_status == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_CAN_TALK) {
            RsStackMutex stack(mRetroChessMtx); /****** LOCKED MUTEX *******/
            auto pit = mPendingTunnels.find(it->first);
            if (pit == mPendingTunnels.end() || pit->second != it->second)
                continue; // changed concurrently, handle on next tick
            mActiveTunnels[it->first] = it->second;
            mPendingTunnels.erase(pit);
            pendingChanged = true;
            CHESS_TLOG("READY peer=" << it->first << " tunnel=" << it->second << " is CAN_TALK after "
                       << (nowTs - startedAt[it->first]) << "s");

            // Flush any queued invite for this peer
            auto inviteIt = mPendingGxsInvites.find(it->first);
            if (inviteIt != mPendingGxsInvites.end()) {
                CHESS_INVLOG("FLUSH queued "
                             << payloadType(reinterpret_cast<const uint8_t*>(inviteIt->second.data()), inviteIt->second.size())
                             << " to=" << it->first << " tunnel=" << it->second << " ready after "
                             << (nowTs - startedAt[it->first]) << "s");
                flushes.push_back(std::make_pair(it->second, inviteIt->second));
                mPendingGxsInvites.erase(inviteIt);
            }
            auto watchIt = mPendingWatchRequests.find(it->first);
            if (watchIt != mPendingWatchRequests.end()) {
                std::cout << "Chess: Tunnel ready, flushing queued watch request to " << it->first
                          << " for game " << watchIt->second.toStdString() << std::endl;
                CHESS_SPECLOG("FLUSH queued watch request to host=" << it->first << " game="
                              << watchIt->second.toStdString() << " tunnel ready after "
                              << (nowTs - startedAt[it->first]) << "s");
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
                if (mPendingGxsInvites.count(it->first))
                    CHESS_INVLOG("DROPPED queued "
                                 << payloadType(reinterpret_cast<const uint8_t*>(mPendingGxsInvites[it->first].data()),
                                                mPendingGxsInvites[it->first].size())
                                 << " to=" << it->first << ": tunnel was remotely closed before it was ready");
                mPendingGxsInvites.erase(it->first); // discard queued invite
                mInvitesToGxs.erase(it->first);
                mPendingTunnels.erase(pit);
                pendingChanged = true;
                // Release our use of it too, otherwise it lingers in GxsTunnel.
                expired.push_back(it->second);
                CHESS_TLOG("PENDING peer=" << it->first << " tunnel=" << it->second << " was remotely closed");
            }
        }
    }

    for (auto it = flushes.begin(); it != flushes.end(); ++it) {
        std::cout << "Chess: Tunnel ready, flushing queued invite" << std::endl;
        sendGxsData(it->first, (const uint8_t*)it->second.c_str(), it->second.size());
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
                sendGxsData(it->first, reinterpret_cast<const uint8_t*>(cancel.data()), cancel.size());
            }
        }
    }
    for (auto it = expired.begin(); it != expired.end(); ++it)
        closeGxsTunnel(*it, "pending tunnel expired or remotely closed");
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
    learnPeerFlair(sender_id, map);
    if (type == "chess_flair") return; // flair-only update, already stored above
    if (handleChessPresence(sender_id, tunnel_id, map)) return;

    if (type == "chess_invite") {
        RsGxsTunnelService::GxsTunnelInfo info;
        if (!mGxsTunnels || !mGxsTunnels->getTunnelInfo(tunnel_id, info)
                || !chessIdentityEnabled(info.source_gxs_id)) {
            CHESS_INVLOG("RECV invite from=" << sender_id << " ignored: tunnel unknown or our identity is not enabled for chess");
            return;
        }
        {
            RsStackMutex stack(mRetroChessMtx);
            if (mChessBusy) {
                std::cerr << "Chess: ignoring distant invitation while busy" << std::endl;
                CHESS_INVLOG("RECV invite from=" << sender_id << " answered chess_busy (we are busy)");
                QJsonObject busyJson{{"type", "chess_busy"}};
                const QByteArray busy = QJsonDocument(busyJson).toJson(QJsonDocument::Compact);
                sendGxsData(tunnel_id, reinterpret_cast<const uint8_t*>(busy.constData()), busy.size());
                return;
            }
        }
        std::cout << "Chess: Received invite from GXS " << sender_id << std::endl;
        {
            RsStackMutex stack(mRetroChessMtx);
            auto game = mGameSessions.find(sender_id.toStdString());
            if (game != mGameSessions.end()
                    && game->second.localIdentityId != QString::fromStdString(info.source_gxs_id.toStdString())) {
                CHESS_INVLOG("RECV invite from=" << sender_id << " ignored: a game with this peer runs on another identity");
                return;
            }
            // Remember the identity and tunnel chosen for this invitation.
            mOwnGxsIdByPeer[sender_id] = info.source_gxs_id;
            mActiveTunnels[sender_id] = tunnel_id;
            mGameIdByPeer[sender_id] = map.value("game_id").toString();
            mInviteTimeControlByPeer[sender_id] = ChessTimeControl::fromNetString(map.value("tc").toString());
            mJoinRequestsFromGxs.erase(sender_id);
            if (map.value("join_open_game").toBool()) mJoinRequestsFromGxs.insert(sender_id);
            mInvitesFromGxs.insert(sender_id);
        }
        CHESS_INVLOG("RECV invite from=" << sender_id << " to=" << info.source_gxs_id
                     << " game=" << map.value("game_id").toString().toStdString()
                     << " tc=" << (map.contains("tc") ? map.value("tc").toString().toStdString() : "unlimited")
                     << " join_open_game=" << map.value("join_open_game").toBool()
                     << " tunnel=" << tunnel_id << " -> notifying UI");
        // A new invite packet is also a refresh of an existing pending invite.
        // Always notify the UI so the toaster and chat action reappear.
        mNotify->notifyChessInviteGxs(sender_id);

    } else if (type == "chess_busy") {
        CHESS_INVLOG("RECV busy from=" << sender_id << " (peer is busy, invite not shown there)");
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
            CHESS_INVLOG("RECV accept from=" << sender_id << " ignored: no invitation sent to this peer");
            return;
        }
        std::cout << "Chess: Received accept from GXS " << sender_id << std::endl;
        CHESS_INVLOG("RECV accept from=" << sender_id << " game=" << map.value("game_id").toString().toStdString()
                     << " tc=" << (map.contains("tc") ? map.value("tc").toString().toStdString() : "unlimited")
                     << " -> starting game");
        mNotify->notifyChessAcceptedGxs(sender_id);

    } else if (type == "chess_cancel") {
        {
            RsStackMutex stack(mRetroChessMtx);
            if (mInvitesFromGxs.erase(sender_id) == 0) {
                CHESS_INVLOG("RECV cancel from=" << sender_id << " ignored: no invitation from this peer");
                return;
            }
        }
        CHESS_INVLOG("RECV cancel from=" << sender_id << " -> invitation removed");
        mNotify->notifyChessInviteClearedGxs(sender_id);

    } else if (type == "chess_reject") {
        {
            RsStackMutex stack(mRetroChessMtx);
            // Ignore unsolicited or duplicate replies. A rejection only
            // resolves our outgoing invite, not a crossed incoming invite.
            if (mInvitesToGxs.erase(sender_id) == 0) {
                CHESS_INVLOG("RECV reject from=" << sender_id << " ignored: no invitation sent to this peer");
                return;
            }
            auto pending = mPendingGxsInvites.find(sender_id);
            if (pending != mPendingGxsInvites.end()
                    && QJsonDocument::fromJson(QByteArray::fromStdString(pending->second)).object().value("type").toString() == "chess_invite")
                mPendingGxsInvites.erase(pending);
        }
        CHESS_INVLOG("RECV reject from=" << sender_id << " -> invitation declined");
        mNotify->notifyChessRejectedGxs(sender_id);
        mNotify->notifyAvailablePeersChanged();

    } else if (type == "player_leave") {
        std::cout << "Chess: Remote GXS player left " << sender_id << std::endl;
        CHESS_GAMELOG("RECV player_leave from=" << sender_id << " (opponent closed the game)");
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
        CHESS_GAMELOG("RECV rematch from=" << sender_id << " remote_color=" << remoteColor
                      << " game=" << map.value("game_id").toString().toStdString());
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
            CHESS_SPECLOG("RELAY opponent action " << action.section(':', 0, 1).toStdString()
                          << " game=" << sender_id << " to " << spectatorTunnels.size() << " spectator(s)");
            QVariantMap specMap;
            specMap.insert("type", "chess_watch_action");
            specMap.insert("version", 1);
            specMap.insert("game_id", QString::fromStdString(sender_id.toStdString()));
            specMap.insert("action", action);
            const QByteArray specMsg = QJsonDocument::fromVariant(specMap).toJson(QJsonDocument::Compact);
            for (const auto &sTunnel : spectatorTunnels) {
                sendGxsData(sTunnel, reinterpret_cast<const uint8_t*>(specMsg.constData()), specMsg.size());
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
        CHESS_INVLOG("RECV seek from=" << sender_id << " seeking=" << map.value("seeking", !tc.unlimited).toBool()
                     << " tc=" << (tcStr.isEmpty() ? "unlimited" : tcStr.toStdString()));
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
        CHESS_SPECLOG("RECV watch request from=" << sender_id << " game=" << reqGameId.toStdString()
                      << (foundSession ? " -> spectator added" : " -> no such game, answering watch_end"));

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
            const bool stateSent = sendGxsData(tunnel_id, reinterpret_cast<const uint8_t*>(replyBytes.constData()), replyBytes.size());
            CHESS_SPECLOG("SEND watch state to=" << sender_id << " moves=" << activeSession.moveHistory.size()
                          << " sequence=" << activeSession.moveSequence << " ok=" << stateSent);
        } else if (mGxsTunnels) {
            QVariantMap failReply;
            failReply["type"] = "chess_watch_end";
            failReply["version"] = 1;
            failReply["game_id"] = reqGameId;
            failReply["reason"] = "Game not active";
            const QByteArray failBytes = QJsonDocument::fromVariant(failReply).toJson(QJsonDocument::Compact);
            sendGxsData(tunnel_id, reinterpret_cast<const uint8_t*>(failBytes.constData()), failBytes.size());
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
        CHESS_SPECLOG("RECV watch state from host=" << sender_id << " game=" << gameId.toStdString()
                      << " moves=" << moves.size() << " sequence=" << sequence);
        mNotify->notifyChessWatchState(sender_id, gameId, whiteId, whiteName, blackId, blackName, fen, sequence, lastFrom, lastTo, moves);

    } else if (type == "chess_watch_action") {
        const QString gameId = map.value("game_id").toString();
        const QString action = map.value("action").toString();
        mNotify->notifyChessWatchAction(sender_id, gameId, action);

    } else if (type == "chess_watch_end") {
        const QString gameId = map.value("game_id").toString();
        const QString reason = map.value("reason").toString();
        CHESS_SPECLOG("RECV watch end from host=" << sender_id << " game=" << gameId.toStdString()
                      << " reason=" << reason.toStdString());
        mNotify->notifyChessWatchEnd(sender_id, gameId, reason);

    } else if (type == "chess_watch_leave") {
        const QString gameId = map.value("game_id").toString();
        CHESS_SPECLOG("RECV watch leave from=" << sender_id << " game=" << gameId.toStdString()
                      << " -> spectator removed");
        RsStackMutex stack(mRetroChessMtx);
        for (auto &entry : mSpectatorsByGame) {
            const auto session = mGameSessions.find(entry.first);
            if (gameId.isEmpty() || gameId.contains(QString::fromStdString(entry.first))
                    || (session != mGameSessions.end()
                        && !session->second.gameId.isEmpty() && gameId == session->second.gameId)) {
                entry.second.erase(sender_id);
            }
        }

    } else if (type == "leaderboard_receipt" || type == "leaderboard_sync" || type == "leaderboard_sync_req") {
        mNotify->notifyLeaderboardDataGxs(sender_id, QByteArray((const char*)data, data_size));

    } else {
        // The legacy "col,row,count" click packet is no longer accepted over
        // GXS tunnels: moves travel as authenticated, sequence-checked
        // "game_action" move packets, so an arbitrary payload must never be
        // able to drive the board.
        std::cerr << "Chess: Unknown message type '" << type.toStdString() << "' ignored" << std::endl;
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
    CHESS_GAMELOG("SEND player_leave to=" << gxs_id << " (we closed the game)");
    if (!spectatorTunnels.empty())
        CHESS_SPECLOG("SEND watch end to " << spectatorTunnels.size() << " spectator(s) of game=" << gxs_id
                      << " reason=Player left");
    if (sendGxsData(tunnelId, (const uint8_t*)leave.c_str(), leave.size())) {
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
            sendGxsData(tId, reinterpret_cast<const uint8_t*>(bytes.constData()), bytes.size());
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
        closeGxsTunnel(tunnelId, "delayed close after player_leave");
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
    CHESS_TLOG("STATUS tunnel=" << tunnel_id << " -> " << statusName(tunnel_status));

    // React to tunnel being closed or going down
    if (tunnel_status != RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_REMOTELY_CLOSED &&
        tunnel_status != RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_TUNNEL_DN)
        return;

    // Important: on the side that opened the tunnel, GxsTunnel turns a remote
    // close into TUNNEL_DN and keeps re-digging turtle tunnels for it until
    // the client service calls closeExistingTunnel(). Merely forgetting the
    // tunnel here (as this function used to do) leaves it alive forever:
    // turtle rebuilds it, it goes back to CAN_TALK and keep-alives flow.
    // So every tunnel we stop using is either closed or kept under watch.
    // The close itself happens from tick() (closeQueuedGxsTunnels), not from
    // inside this GxsTunnel callback.
    const bool remoteClose = tunnel_status == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_REMOTELY_CLOSED;
    RsGxsId gxs_id;          // peer whose active tunnel went away
    RsGxsId pendingPeer;     // peer whose pending tunnel was remotely closed
    const char *action = "nothing to do";
    bool handled = false;
    bool backoffChanged = false;
    {
        RsStackMutex stack(mRetroChessMtx);
        const time_t now = time(nullptr);
        // Search active tunnels for the closed tunnel
        for (auto it = mActiveTunnels.begin(); it != mActiveTunnels.end(); ++it) {
            if (it->second == tunnel_id) {
                gxs_id = it->first;
                mActiveTunnels.erase(it);
                break;
            }
        }
        if (!gxs_id.isNull()) {
            handled = true;
            const bool leaving = mPendingGxsCloses.count(gxs_id) != 0;
            if (remoteClose || leaving) {
                mTunnelsToClose.insert(tunnel_id);
                mTunnelToGxsIdMap.erase(tunnel_id);
                action = remoteClose ? "active tunnel closed by peer -> closing our side"
                                     : "active tunnel lost while leaving -> closing";
            } else {
                // Temporary loss: GxsTunnel/turtle is digging a new route.
                // Watch it as pending so handleGxsTick() re-adopts it once it
                // is CAN_TALK again, or closes it after its timeout, instead
                // of leaving an unmanaged tunnel behind.
                mPendingTunnels[gxs_id] = tunnel_id;
                action = "active tunnel down -> watching as pending (re-adopt or time out)";
            }
        } else {
            for (auto it = mPendingTunnels.begin(); it != mPendingTunnels.end(); ++it) {
                if (it->second != tunnel_id) continue;
                handled = true;
                if (remoteClose) {
                    pendingPeer = it->first;
                    mPendingTunnels.erase(it);
                    mPendingGxsInvites.erase(pendingPeer);
                    mInvitesToGxs.erase(pendingPeer);
                    mTunnelsToClose.insert(tunnel_id);
                    mTunnelToGxsIdMap.erase(tunnel_id);
                    action = "pending tunnel closed by peer -> closing our side";
                } else {
                    action = "pending tunnel still connecting";
                }
                break;
            }
            if (!handled) {
                // Not (or no longer) tracked by us: an orphan we opened
                // earlier, or an incoming probe tunnel. Release our service's
                // use of it; GxsTunnel keeps it if another service uses it.
                mTunnelsToClose.insert(tunnel_id);
                mTunnelToGxsIdMap.erase(tunnel_id);
                action = "untracked tunnel -> closing our side";
            }
        }
        const RsGxsId peer = !gxs_id.isNull() ? gxs_id : pendingPeer;
        if (!peer.isNull()) {
            auto contact = mChessContacts.find(peer);
            if (contact != mChessContacts.end()) {
                contact->second.status = "offline";
                contact->second.seeking = false;
                contact->second.seekTimeControl = ChessTimeControl{};
                contact->second.deadline = 0;
                contact->second.nonce.clear();
                contact->second.yieldUntil = 0;
                if (remoteClose) {
                    // The peer closed it on purpose (plugin disabled, contact
                    // removed, identity changed...). Count it as a failure so
                    // the normal backoff applies instead of redialing in 15s.
                    contact->second.failures = std::min(contact->second.failures + 1, kPresenceMaxFailures);
                    contact->second.nextProbe = now + chessPresenceRetryDelay(contact->second.failures);
                    backoffChanged = true;
                } else {
                    // Network hiccup: the tunnel is being re-dug already.
                    contact->second.nextProbe = now + chessPresenceRetryDelay(0);
                }
            }
        }
        if (!gxs_id.isNull()) {
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
    CHESS_TLOG("STATUS tunnel=" << tunnel_id << " peer="
               << (!gxs_id.isNull() ? gxs_id : pendingPeer) << ": " << action);
    if (!gxs_id.isNull()) {
        std::cout << "Chess: Tunnel closed for GXS " << gxs_id << std::endl;
        mNotify->notifyGxsTunnelClosed(gxs_id);
    }
    if (backoffChanged) IndicateConfigChanged();
    if (!gxs_id.isNull() || !pendingPeer.isNull())
        mNotify->notifyAvailablePeersChanged();
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
    CHESS_TLOG("RECV tunnel=" << id << " from=" << info.destination_gxs_id << " to=" << info.source_gxs_id
               << " type=" << payloadType(data, data_size) << " bytes=" << data_size
               << (allowed ? "" : " DROPPED (identity not enabled for chess / not our tunnel)"));
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
        for (const auto &entry : mActiveTunnels)
            if (chessPeerConfirmedLocked(entry.first)) tunnels.push_back(entry.second);
    }
    QVariantMap message;
    message["type"] = "chess_seek";
    message["seeking"] = active;
    if (active) message["tc"] = tc.toNetString();
    const QByteArray bytes = QJsonDocument::fromVariant(message).toJson(QJsonDocument::Compact);
    // Lobby advertisements are top-level packets, not game_action payloads.
    for (const auto &tunnel : tunnels)
        if (mGxsTunnels) sendGxsData(tunnel, reinterpret_cast<const uint8_t*>(bytes.constData()), bytes.size());
}


/*****************************************************************************
 * Tunnel helpers: every request / send / close goes through these so that the
 * whole tunnel lifecycle shows up in the debug log.
 *****************************************************************************/

bool p3RetroChess::openGxsTunnel(const RsGxsId &to, const RsGxsId &from, RsGxsTunnelId &tunnel, const char *reason)
{
    if (!mGxsTunnels) return false;
    uint32_t error_code = 0;
    const bool ok = mGxsTunnels->requestSecuredTunnel(to, from, tunnel,
            RETRO_CHESS_GXS_TUNNEL_SERVICE_ID, error_code);
    CHESS_TLOG("REQUEST tunnel=" << tunnel << " to=" << to << " from=" << from
               << " ok=" << ok << " error=" << error_code << " reason: " << reason);
    return ok;
}

bool p3RetroChess::sendGxsData(const RsGxsTunnelId &tunnel, const uint8_t *data, uint32_t size)
{
    if (!mGxsTunnels) return false;
    const bool ok = mGxsTunnels->sendData(tunnel, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID, data, size);
    if (RetroChessTunnelDebug::enabled()) {
        RsGxsTunnelService::GxsTunnelInfo info;
        if (mGxsTunnels->getTunnelInfo(tunnel, info)) {
            CHESS_TLOG("SEND tunnel=" << tunnel << " to=" << info.destination_gxs_id
                       << " type=" << payloadType(data, size) << " bytes=" << size << " ok=" << ok
                       << " status=" << statusName(info.tunnel_status)
                       << " unacked_packets=" << info.pending_data_packets);
        } else {
            CHESS_TLOG("SEND tunnel=" << tunnel << " type=" << payloadType(data, size) << " bytes=" << size
                       << " ok=" << ok << " (tunnel unknown to GxsTunnel)");
        }
    }
    return ok;
}

bool p3RetroChess::closeGxsTunnel(const RsGxsTunnelId &tunnel, const char *reason)
{
    if (!mGxsTunnels || tunnel.isNull()) return false;
    {
        RsStackMutex stack(mRetroChessMtx);
        mOpenedTunnels.erase(tunnel);
        mTunnelsToClose.erase(tunnel);
    }
    const bool ok = mGxsTunnels->closeExistingTunnel(tunnel, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID);
    CHESS_TLOG("CLOSE tunnel=" << tunnel << " ok=" << ok << " reason: " << reason);
    return ok;
}

void p3RetroChess::closeQueuedGxsTunnels()
{
    std::vector<RsGxsTunnelId> close;
    {
        RsStackMutex stack(mRetroChessMtx);
        if (mTunnelsToClose.empty()) return;
        // Tunnel ids are deterministic per identity pair: if a new request for
        // the same pair started meanwhile, it is in use again - keep it.
        std::set<RsGxsTunnelId> inUse;
        for (const auto &entry : mActiveTunnels) inUse.insert(entry.second);
        for (const auto &entry : mPendingTunnels) inUse.insert(entry.second);
        for (const auto &tunnel : mTunnelsToClose)
            if (!inUse.count(tunnel)) close.push_back(tunnel);
        mTunnelsToClose.clear();
    }
    for (const auto &tunnel : close) closeGxsTunnel(tunnel, "went down / remotely closed (queued)");
}

void p3RetroChess::sweepOrphanGxsTunnels()
{
    if (!mGxsTunnels) return;
    const time_t now = time(nullptr);
    std::vector<RsGxsTunnelId> orphans;
    {
        RsStackMutex stack(mRetroChessMtx);
        if (now - mLastOrphanSweep < kTunnelMaintenanceIntervalSec) return;
        mLastOrphanSweep = now;
        std::set<RsGxsTunnelId> inUse;
        for (const auto &entry : mActiveTunnels) inUse.insert(entry.second);
        for (const auto &entry : mPendingTunnels) inUse.insert(entry.second);
        for (const auto &tunnel : mOpenedTunnels)
            if (!inUse.count(tunnel)) orphans.push_back(tunnel);
    }
    for (const auto &tunnel : orphans) {
        std::cerr << "Chess: closing orphan GXS tunnel " << tunnel << std::endl;
        closeGxsTunnel(tunnel, "orphan sweep: opened by RetroChess but no longer tracked");
    }
}

void p3RetroChess::dumpTunnelState()
{
    if (!RetroChessTunnelDebug::enabled() || !mGxsTunnels) return;
    RetroChessTunnelDebug::setLogDirectoryIfUnset(RsAccounts::AccountDirectory());
    const time_t now = time(nullptr);
    std::map<RsGxsId, RsGxsTunnelId> active, pending;
    std::map<RsGxsId, ChessContact> contacts;
    std::set<RsGxsTunnelId> opened;
    size_t queuedCloses = 0;
    {
        RsStackMutex stack(mRetroChessMtx);
        if (now - mLastTunnelDump < kTunnelMaintenanceIntervalSec) return;
        mLastTunnelDump = now;
        active = mActiveTunnels;
        pending = mPendingTunnels;
        contacts = mChessContacts;
        opened = mOpenedTunnels;
        queuedCloses = mTunnelsToClose.size();
    }
    std::map<RsGxsTunnelId, std::string> role;
    for (const auto &entry : active) role[entry.second] = "active:" + entry.first.toStdString();
    for (const auto &entry : pending) role[entry.second] = "pending:" + entry.first.toStdString();

    CHESS_TLOG("STATE active=" << active.size() << " pending=" << pending.size()
               << " opened_by_us=" << opened.size() << " queued_closes=" << queuedCloses
               << " contacts=" << contacts.size());
    for (const auto &entry : contacts) {
        const ChessContact &c = entry.second;
        CHESS_TLOG("  contact " << entry.first << " status=" << c.status.toStdString()
                   << " failures=" << c.failures
                   << " last_seen=" << (c.lastSeen ? now - c.lastSeen : -1) << "s_ago"
                   << " next_probe_in=" << (c.nextProbe > now ? c.nextProbe - now : 0) << "s"
                   << " deadline_in=" << (c.deadline > now ? c.deadline - now : 0) << "s"
                   << (c.yieldUntil > now ? " yielding" : "")
                   << (c.failures >= kPresenceMaxFailures ? " DORMANT" : ""));
    }
    std::vector<RsGxsTunnelService::GxsTunnelInfo> infos;
    mGxsTunnels->getTunnelsInfo(infos);
    for (const auto &info : infos) {
        auto r = role.find(info.tunnel_id);
        std::string what;
        if (r != role.end()) what = r->second;
        else if (opened.count(info.tunnel_id)) what = "ORPHAN (opened by RetroChess, untracked)";
        else what = "not tracked by RetroChess (distant chat or incoming probe)";
        CHESS_TLOG("  gxs-tunnel " << info.tunnel_id << " " << info.source_gxs_id << " -> "
                   << info.destination_gxs_id << " status=" << statusName(info.tunnel_status)
                   << " sent=" << info.total_size_sent << " recv=" << info.total_size_received
                   << " unacked_packets=" << info.pending_data_packets << " [" << what << "]");
    }
}

void p3RetroChess::setTunnelDebugEnabled(bool enabled)
{
    RetroChessTunnelDebug::setLogDirectory(RsAccounts::AccountDirectory());
    RetroChessTunnelDebug::setEnabled(enabled);
    if (enabled) {
        RsStackMutex stack(mRetroChessMtx);
        mLastTunnelDump = 0; // dump the current state on the next tick
    }
}

bool p3RetroChess::tunnelDebugEnabled()
{
    return RetroChessTunnelDebug::enabled();
}

bool p3RetroChess::tunnelTraffic(std::vector<RsGxsTunnelService::GxsTunnelInfo> &infos)
{
    infos.clear();
    if (!mGxsTunnels) return false;
    std::set<RsGxsTunnelId> tracked;
    {
        RsStackMutex stack(mRetroChessMtx);
        tracked = mOpenedTunnels;
        for (const auto &entry : mActiveTunnels) tracked.insert(entry.second);
        for (const auto &entry : mPendingTunnels) tracked.insert(entry.second);
        for (const auto &entry : mTunnelToGxsIdMap) tracked.insert(entry.first);
        tracked.insert(mTunnelsToClose.begin(), mTunnelsToClose.end());
    }
    // Never call the tunnel service while holding the chess mutex: callbacks
    // from that service take the chess mutex in the opposite direction.
    for (const auto &id : tracked) {
        RsGxsTunnelService::GxsTunnelInfo info{};
        // The bulk query does not initialize is_client_side in this core.
        if (!mGxsTunnels->getTunnelInfo(id, info)) continue;
        info.tunnel_id = id; // The single-tunnel query does not populate this.
        infos.push_back(info);
    }
    return true;
}
