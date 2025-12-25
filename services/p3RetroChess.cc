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

#include <sstream> // for std::istringstream

#include "services/p3RetroChess.h"
#include "services/rsRetroChessItems.h"

#include <sys/time.h>

#include "gui/RetroChessNotify.h"


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
	: RsPQIService(RS_SERVICE_TYPE_RetroChess_PLUGIN,0,handler), mRetroChessMtx("p3RetroChess"), mServiceControl(handler->getServiceControl()), mNotify(notifier)
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

	//processIncoming();
	//sendPackets();

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

	double ts = getCurrentTS();

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

	double ts = getCurrentTS();


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
    if (gxs_id.isNull()) {
        std::cerr << "p3RetroChess::chess_click_gxs Error: Null GXS ID" << std::endl;
        return;
    }

    // 1. Find or Initiate a connection to get a DistantChatPeerId (the tunnel)
    // We reuse our own identity logic similar to IdDialog
    RsGxsId fromGxsId;
    std::list<RsGxsId> ownIdentities;
    rsIdentity->getOwnIds(ownIdentities);
    
    if (ownIdentities.empty()) {
        std::cerr << "p3RetroChess Error: No local GXS identity found to send move." << std::endl;
        return;
    }
    fromGxsId = ownIdentities.front();

    uint32_t error_code;
    DistantChatPeerId tunnelId;

    // Use initiateDistantChatConnexion to ensure the tunnel exists
    if(!rsMsgs->initiateDistantChatConnexion(gxs_id, fromGxsId, tunnelId, error_code)) {
        std::cerr << "p3RetroChess Error: Could not open tunnel for move. Code: " << error_code << std::endl;
        return;
    }

    // 2. Serialize the move data
    // We use a specific prefix so the receiving plugin knows this is a game move, not a text chat.
    QString moveData = QString("ACTION_RETROCHESS_MOVE:%1,%2,%3")
                        .arg(col)
                        .arg(row)
                        .arg(count);

    // 3. Send the move through the tunnel
    // rsMsgs->sendDistantChatMessage is the GXS equivalent of p2p raw_msg_peer
    if (!rsMsgs->sendDistantChatMessage(gxs_id, moveData.toStdString())) {
        std::cerr << "p3RetroChess Error: Failed to send move through GXS tunnel." << std::endl;
    } else {
        std::cout << "p3RetroChess: Move sent to " << gxs_id << " via tunnel " << tunnelId << std::endl;
    }
}

void p3RetroChess::requestGxsTunnel(const RsGxsId &toGxsId)
{
    if (toGxsId.isNull()) return;

    // 1. Determine our own identity (the sender)
    RsGxsId fromGxsId;
    std::list<RsGxsId> ownIdentities;
    rsIdentity->getOwnIds(ownIdentities);
    
    if (ownIdentities.empty()) {
        std::cerr << "RetroChess: No own identities found to open tunnel." << std::endl;
        return;
    }
    fromGxsId = ownIdentities.front();

    // 2. Initiate the connection (Tunnel)
    uint32_t error_code;
    DistantChatPeerId did; // This is the unique tunnel ID

    if(rsMsgs->initiateDistantChatConnexion(toGxsId, fromGxsId, did, error_code)) {
        std::cout << "RetroChess: Tunnel initiated successfully. ID: " << did << std::endl;
        // Store 'did' to use for sending moves later via this tunnel
    } else {
        std::cerr << "RetroChess: Failed to initiate tunnel. Error: " << error_code << std::endl;
    }
}

void p3RetroChess::sendGxsInvite(const RsGxsId &toGxsId)
{
    // For GXS invites, we initiate the tunnel first. 
    // Once the tunnel is requested, we send a specific Chess packet through it.
    requestGxsTunnel(toGxsId);
    
    // After requesting the tunnel, we send a Distant Message to notify the peer.
    // The distant message serves as the "Invite" visible in their chat window.
    std::string invite_str = "ACTION_RETROCHESS_INVITE";
    rsMsgs->sendDistantChatMessage(toGxsId, invite_str);
}