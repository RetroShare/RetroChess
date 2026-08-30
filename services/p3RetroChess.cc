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


//#define DEBUG_RetroChess		1


/* DEFINE INTERFACE POINTER! */
RsRetroChess *rsRetroChess = NULL;



#ifdef WINDOWS_SYS
#include <time.h>
#include <sys/timeb.h>
#endif

static double getCurrentTS()
{

#ifndef WINDOWS_SYS
	struct timeval cts_tmp;
	gettimeofday(&cts_tmp, NULL);
	double cts =  (cts_tmp.tv_sec) + ((double) cts_tmp.tv_usec) / 1000000.0;
#else
	struct _timeb timebuf;
	_ftime( &timebuf);
	double cts =  (timebuf.time) + ((double) timebuf.millitm) / 1000.0;
#endif
	return cts;
}

static uint64_t convertTsTo64bits(double ts)
{
	uint32_t secs = (uint32_t) ts;
	uint32_t usecs = (uint32_t) ((ts - (double) secs) * 1000000);
	uint64_t bits = (((uint64_t) secs) << 32) + usecs;
	return bits;
}

static double convert64bitsToTs(uint64_t bits)
{
	uint32_t usecs = (uint32_t) (bits & 0xffffffff);
	uint32_t secs = (uint32_t) ((bits >> 32) & 0xffffffff);
	double ts =  (secs) + ((double) usecs) / 1000000.0;

	return ts;
}

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
	return 0;
}

int	p3RetroChess::status()
{
	return 1;
}
#include<qjsondocument.h>
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
	return invitesFrom.find(peerID)!=invitesFrom.end();
}
bool p3RetroChess::hasInviteTo(RsPeerId peerID)
{
	return invitesTo.find(peerID)!=invitesTo.end();
}

void p3RetroChess::acceptedInvite(RsPeerId peerID)
{
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
	raw_msg_peer(peerID, "{\"type\":\"chess_accept\"}");
}

void p3RetroChess::clearInvite(RsPeerId peerID)
{
	invitesTo.erase(peerID);
	invitesFrom.erase(peerID);
}

void p3RetroChess::gotInvite(RsPeerId peerID)
{

	std::set<RsPeerId>::iterator it =invitesFrom.find(peerID);
	if (it == invitesFrom.end())
	{
		invitesFrom.insert(peerID);
	}
}
void p3RetroChess::sendInvite(RsPeerId peerID)
{

	std::set<RsPeerId>::iterator it =invitesTo.find(peerID);
	if (it == invitesTo.end())
	{
		invitesTo.insert(peerID);
	}
	raw_msg_peer(peerID, "{\"type\":\"chess_invite\"}");
}

/*void p3RetroChess::set_peer(RsPeerId peer)
{
	mPeerID = peer;
}*/
void p3RetroChess::raw_msg_peer(RsPeerId peerID, std::string msg)
{
	std::cout << "MSging: " << peerID.toStdString() << "\n";
	std::cout << "MSging: " << msg << "\n";
	/* create the packet */
	RsRetroChessDataItem *pingPkt = new RsRetroChessDataItem();
	pingPkt->PeerId(peerID);
	pingPkt->m_msg = msg;
	pingPkt->data_size = msg.size();
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
	//TODO ping all!
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
	std::cout << "recvItem type: " << item->PacketSubType() << "\n";
	/* pass to specific handler */
	bool keep = false ;

	switch(item->PacketSubType())
	{
	case RS_PKT_SUBTYPE_RetroChess_DATA:
		handleData(dynamic_cast<RsRetroChessDataItem*>(item));
		keep = true ;
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

	/*vitem->tlvkvs.pairs.push_back(push_int_value("P3RetroChess_CONFIG_ATRANSMIT",_atransmit)) ;
	vitem->tlvkvs.pairs.push_back(push_int_value("P3RetroChess_CONFIG_VOICEHOLD",_voice_hold)) ;
	vitem->tlvkvs.pairs.push_back(push_int_value("P3RetroChess_CONFIG_VADMIN"   ,_vadmin)) ;
	vitem->tlvkvs.pairs.push_back(push_int_value("P3RetroChess_CONFIG_VADMAX"   ,_vadmax)) ;
	vitem->tlvkvs.pairs.push_back(push_int_value("P3RetroChess_CONFIG_NOISE_SUP",_noise_suppress)) ;
	vitem->tlvkvs.pairs.push_back(push_int_value("P3RetroChess_CONFIG_MIN_LOUDN",_min_loudness)) ;
	vitem->tlvkvs.pairs.push_back(push_int_value("P3RetroChess_CONFIG_ECHO_CNCL",_echo_cancel)) ;*/

	lst.push_back(vitem) ;

	return true ;
}
bool p3RetroChess::loadList(std::list<RsItem*>& load)
{
	for(std::list<RsItem*>::const_iterator it(load.begin()); it!=load.end(); ++it)
	{
#ifdef P3TURTLE_DEBUG
		assert(item!=NULL) ;
#endif
		RsConfigKeyValueSet *vitem = dynamic_cast<RsConfigKeyValueSet*>(*it) ;
		/*
		if(vitem != NULL)
			for(std::list<RsTlvKeyValue>::const_iterator kit = vitem->tlvkvs.pairs.begin(); kit != vitem->tlvkvs.pairs.end(); ++kit)
				if(kit->key == "P3RetroChess_CONFIG_ATRANSMIT")
					_atransmit = pop_int_value(kit->value) ;
				else if(kit->key == "P3RetroChess_CONFIG_VOICEHOLD")
					_voice_hold = pop_int_value(kit->value) ;
				else if(kit->key == "P3RetroChess_CONFIG_VADMIN")
					_vadmin = pop_int_value(kit->value) ;
				else if(kit->key == "P3RetroChess_CONFIG_VADMAX")
					_vadmax = pop_int_value(kit->value) ;
				else if(kit->key == "P3RetroChess_CONFIG_NOISE_SUP")
					_noise_suppress = pop_int_value(kit->value) ;
				else if(kit->key == "P3RetroChess_CONFIG_MIN_LOUDN")
					_min_loudness = pop_int_value(kit->value) ;
				else if(kit->key == "P3RetroChess_CONFIG_ECHO_CNCL")
					_echo_cancel = pop_int_value(kit->value) ;

		delete vitem ;
		*/
	}

	return true ;
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
    if (mActiveTunnels.find(gxs_id) == mActiveTunnels.end()) {
        // Tunnel not ready, try to re-open
        sendGxsInvite(gxs_id);
        return;
    }

    RsGxsTunnelId tunnel_id = mActiveTunnels[gxs_id];

    // Create a data item for the move
    RsRetroChessDataItem *item = new RsRetroChessDataItem();
    item->m_msg = QString("%1,%2,%3").arg(col).arg(row).arg(count).toStdString();

    // Send raw data through the secured tunnel
    mGxsTunnels->sendData(tunnel_id, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID, (const uint8_t*)item->m_msg.c_str(), item->m_msg.size());
}

void p3RetroChess::requestGxsTunnel(const RsGxsId &gxsId)
{
    // Check if we already have a tunnel
    if (mActiveTunnels.count(gxsId)) {
        mNotify->notifyGxsTunnelReady(gxsId);
        return;
    }
    // Otherwise, start the async tunnel request
    this->sendGxsInvite(gxsId); 
}

void p3RetroChess::sendGxsInvite(const RsGxsId &to_gxs_id)
{
    RsGxsId from_gxs_id;
    std::list<RsGxsId> ownIds;
    rsIdentity->getOwnIds(ownIds);
    if (ownIds.empty()) return;
    from_gxs_id = ownIds.front();

    RsGxsTunnelId tunnel_id;
    uint32_t error_code;

    // Open a tunnel using mGxsTunnel (Async Request)
    if (mGxsTunnels->requestSecuredTunnel(
            to_gxs_id, from_gxs_id, tunnel_id, 
            RETRO_CHESS_GXS_TUNNEL_SERVICE_ID, error_code)) 
    {
        mPendingTunnels[to_gxs_id] = tunnel_id;
        std::cout << "Chess Tunnel requested. Pending ID: " << tunnel_id << std::endl;
    }
}

void p3RetroChess::acceptedInviteGxs(const RsGxsId &gxsId)
{
    std::cout << "Chess: acceptedInviteGxs from " << gxsId << std::endl;

    RsStackMutex stack(mRetroChessMtx);
    mInvitesFromGxs.erase(gxsId);
    auto it = mActiveTunnels.find(gxsId);
    if (it != mActiveTunnels.end()) {
        // Tunnel already active (server side — invite arrived over it).
        // Send chess_accept immediately.
        std::string accept = "{\"type\":\"chess_accept\"}";
        std::cout << "Chess: Sending chess_accept over tunnel " << it->second << std::endl;
        mGxsTunnels->sendData(it->second, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                              (const uint8_t*)accept.c_str(), accept.size());
    } else {
        // Client side: we initiated the invite but tunnel isn't in mActiveTunnels yet.
        // Queue accept for when tunnel becomes CAN_TALK.
        mPendingGxsInvites[gxsId] = "{\"type\":\"chess_accept\"}";
        requestGxsTunnel(gxsId);
    }
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

    const std::string message = QString("{\"type\":\"rematch\",\"color\":%1}")
            .arg(localColor).toStdString();
    return mGxsTunnels->sendData(
            tunnelId, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
            reinterpret_cast<const uint8_t*>(message.data()), message.size());
}

bool p3RetroChess::sendGameActionGxs(const RsGxsId &gxsId, const std::string &action)
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
    map.insert("type", "game_action");
    map.insert("action", QString::fromStdString(action));
    const QByteArray message = QJsonDocument::fromVariant(map).toJson(QJsonDocument::Compact);
    return mGxsTunnels->sendData(
            tunnelId, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
            reinterpret_cast<const uint8_t*>(message.constData()), message.size());
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
        // Only add once; don't reset timestamp on duplicate clicks
        if (mPendingDistantChatInvites.find(chatId.toDistantChatId()) == mPendingDistantChatInvites.end()) {
            mPendingDistantChatInvites[chatId.toDistantChatId()] = time(NULL);
        }
        return true;
    }

    return doSendInviteOverGxs(info.to_id, info.own_id);
}

bool p3RetroChess::doSendInviteOverGxs(const RsGxsId &toId, const RsGxsId &ownId)
{
    std::cout << "Chess: doSendInviteOverGxs: to=" << toId << " from=" << ownId << std::endl;

    if (!mGxsTunnels) {
        std::cerr << "Chess: doSendInviteOverGxs: mGxsTunnels is NULL!" << std::endl;
        return false;
    }

    {
        RsStackMutex stack(mRetroChessMtx);
        mOwnGxsIdByPeer[toId] = ownId;
        auto activeIt = mActiveTunnels.find(toId);
        if (activeIt != mActiveTunnels.end()) {
            const std::string invite = "{\"type\":\"chess_invite\"}";
            return mGxsTunnels->sendData(activeIt->second, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                                         (const uint8_t*)invite.c_str(), invite.size());
        }
    }

    {
        RsStackMutex stack(mRetroChessMtx);
        mPendingTunnels.erase(toId); // clear any stale pending entry
    }

    RsGxsTunnelId tunnelId;
    uint32_t error_code = 0;
    if (mGxsTunnels->requestSecuredTunnel(toId, ownId, tunnelId,
                                          RETRO_CHESS_GXS_TUNNEL_SERVICE_ID, error_code))
    {
        RsStackMutex stack(mRetroChessMtx);
        mPendingTunnels[toId] = tunnelId;
        mPendingGxsInvites[toId] = "{\"type\":\"chess_invite\"}";
        std::cout << "Chess: Tunnel requested (id=" << tunnelId << "), invite queued for " << toId << std::endl;
        return true;
    } else {
        std::cerr << "Chess: doSendInviteOverGxs: requestSecuredTunnel failed, error=" << error_code << std::endl;
        return false;
    }
}

void p3RetroChess::retryPendingDistantChatInvites()
{
    std::map<DistantChatPeerId, time_t> pending;
    {
        RsStackMutex stack(mRetroChessMtx);
        pending = mPendingDistantChatInvites;
    }

    if (pending.empty()) return;

    time_t now = time(NULL);
    for (auto it = pending.begin(); it != pending.end(); ++it) {
        // Throttle: only retry every 2 seconds
        if (now - it->second < 2) continue;

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
            mPendingDistantChatInvites[it->first] = now;
        }
    }
}


void p3RetroChess::handleGxsTick()
{
    auto it = mPendingTunnels.begin();
    while (it != mPendingTunnels.end()) {
        RsGxsTunnelService::GxsTunnelInfo tinfo;
        if (mGxsTunnels->getTunnelInfo(it->second, tinfo)) {
            // Check if the tunnel is "Connected" (CAN_TALK)
            if (tinfo.tunnel_status == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_CAN_TALK) {
                RsGxsId gxsId = it->first;
                RsGxsTunnelId tunnelId = it->second;
                mActiveTunnels[gxsId] = tunnelId;
                it = mPendingTunnels.erase(it);

                // Flush any queued invite for this peer
                auto inviteIt = mPendingGxsInvites.find(gxsId);
                if (inviteIt != mPendingGxsInvites.end()) {
                    std::string invite = inviteIt->second;
                    mPendingGxsInvites.erase(inviteIt);
                    std::cout << "Chess: Tunnel ready, flushing queued invite to " << gxsId << std::endl;
                    mGxsTunnels->sendData(tunnelId, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                                         (const uint8_t*)invite.c_str(), invite.size());
                }

                mNotify->notifyGxsTunnelReady(gxsId);
                continue;
            }
            // Check for "Closed/Failed" status
            else if (tinfo.tunnel_status == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_REMOTELY_CLOSED ||
                     tinfo.tunnel_status == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_TUNNEL_DN) {
                mPendingGxsInvites.erase(it->first); // discard queued invite
                it = mPendingTunnels.erase(it);
                continue;
            }
        }
        ++it;
    }
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
    std::string msg((const char*)data, data_size);
    std::cout << "Chess::handleRawData: received from " << sender_id << ": " << msg << std::endl;

    QJsonDocument jsondoc = QJsonDocument::fromJson(QByteArray::fromStdString(msg));
    QVariantMap map = jsondoc.toVariant().toMap();
    QString type = map.value("type").toString();

    if (type == "chess_invite") {
        std::cout << "Chess: Received invite from GXS " << sender_id << std::endl;
        {
            RsStackMutex stack(mRetroChessMtx);
            // Remember this tunnel is active for the sender (server-side)
            mActiveTunnels[sender_id] = tunnel_id;
            mInvitesFromGxs.insert(sender_id);
        }
        mNotify->notifyChessInviteGxs(sender_id);

    } else if (type == "chess_accept") {
        std::cout << "Chess: Received accept from GXS " << sender_id << std::endl;
        mNotify->notifyChessAcceptedGxs(sender_id);

    } else if (type == "player_leave") {
        std::cout << "Chess: Remote GXS player left " << sender_id << std::endl;
        mNotify->notifyChessPlayerLeftGxs(sender_id);

    } else if (type == "rematch") {
        const int remoteColor = map.value("color").toInt();
        std::cout << "Chess: Remote GXS player requested a rematch " << sender_id << std::endl;
        mNotify->notifyChessRematchGxs(sender_id, remoteColor);

    } else if (type == "game_action") {
        mNotify->notifyChessGameActionGxs(sender_id, map.value("action").toString());

    } else {
        // Chess move: format "col,row,count"
        QStringList parts = QString::fromStdString(msg).split(",");
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
    RsStackMutex stack(mRetroChessMtx);
    auto it = mActiveTunnels.find(gxs_id);
    if (it == mActiveTunnels.end() || !mGxsTunnels) return;

    const std::string leave = "{\"type\":\"player_leave\"}";
    if (mGxsTunnels->sendData(it->second, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID,
                              (const uint8_t*)leave.c_str(), leave.size()))
        mPendingGxsCloses[gxs_id] = time(NULL) + 2;
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
        }
        if (!gxs_id.isNull()) {
            std::cout << "Chess: Tunnel closed for GXS " << gxs_id << std::endl;
            mNotify->notifyGxsTunnelClosed(gxs_id);
        }
    }
}

void p3RetroChess::receiveData(const RsGxsTunnelId& id, unsigned char *data, uint32_t data_size)
{
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
    std::cout << "Chess: acceptDataFromPeer: gxs=" << gxs_id
              << " tunnel=" << tunnel_id
              << " side=" << (am_I_client_side ? "client" : "server") << std::endl;
    RsGxsTunnelService::GxsTunnelInfo tunnelInfo;
    const bool haveTunnelInfo = mGxsTunnels && mGxsTunnels->getTunnelInfo(tunnel_id, tunnelInfo);
    {
        RsStackMutex stack(mRetroChessMtx);
        // Store the mapping so receiveData / handleRawData can identify the sender
        mTunnelToGxsIdMap[tunnel_id] = gxs_id;
        if (haveTunnelInfo && !tunnelInfo.source_gxs_id.isNull())
            mOwnGxsIdByPeer[gxs_id] = tunnelInfo.source_gxs_id;
    }
    return true;
}
