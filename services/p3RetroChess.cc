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
#include "gui/common/AvatarDefs.h"

#include <sys/time.h>

#include "gui/RetroChessNotify.h"
#include "util/rsdebug.h"


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

#if 0
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
#endif

p3RetroChess::p3RetroChess(RsPluginHandler *handler,RetroChessNotify *notifier)
	: RsPQIService(RS_SERVICE_TYPE_RetroChess_PLUGIN,0,handler), mRetroChessMtx("p3RetroChess"), mServiceControl(handler->getServiceControl()), mNotify(notifier), mGxsTunnels(NULL), mIdentity(NULL), mChats(NULL)
{
	RsDbg() << "CHESS: Initializing GXS Tunnel Client Service interface.";
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
	std::set<RsPeerId>::iterator it = invitesTo.find(peerID);
	if (it != invitesTo.end())
	{
		invitesTo.erase(it);
	}

	it = invitesFrom.find(peerID);
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
RsPeerId p3RetroChess::getOwnId(const RsPeerId& remoteId)
{
	RsStackMutex stack(mRetroChessMtx);
	if (mPseudoToRealGxsMap.count(remoteId)) {
		RsGxsId targetId = mPseudoToRealGxsMap[remoteId];
		if (mTargetToOwnGxsIdMap.count(targetId)) {
			return RsPeerId(mTargetToOwnGxsIdMap[targetId].toStdString());
		}
	}
	return rsPeers->getOwnId();
}

bool p3RetroChess::isLocalId(const RsPeerId& id)
{
	if (id == rsPeers->getOwnId()) return true;
	
	RsStackMutex stack(mRetroChessMtx);
	RsGxsId gxsId(id.toStdString());
	if (mPseudoToRealGxsMap.count(id)) {
		gxsId = mPseudoToRealGxsMap[id];
	}

	if (!gxsId.isNull() && mIdentity) {
		return mIdentity->isOwnId(gxsId);
	}
	return false;
}

void p3RetroChess::raw_msg_peer(RsPeerId peerID, std::string msg)
{
	{
		RsStackMutex stack(mRetroChessMtx);
		if (mPseudoToRealGxsMap.count(peerID)) {
			RsDbg() << "CHESS: Routing move via GXS tunnel for " << peerID;
			RsGxsId targetGxsId = mPseudoToRealGxsMap[peerID];
			if (mTargetGxsToTunnelMap.count(targetGxsId)) {
				RsGxsTunnelId tunnelId = mTargetGxsToTunnelMap[targetGxsId];
				RsGxsTunnelService::GxsTunnelInfo info;
				if (mGxsTunnels->getTunnelInfo(tunnelId, info)) {
					RsDbg() << "CHESS: Routing via tunnel " << tunnelId << " with real source " << info.source_gxs_id << " and target " << info.destination_gxs_id;
					raw_msg_gxs(info.destination_gxs_id, info.source_gxs_id, msg);
					return;
				}
			}
			RsWarn() << "CHESS: Tunnel NOT FOUND for GXS " << targetGxsId;
			RsWarn() << "CHESS: Could not find tunnel for pseudo-peer " << peerID;
			return;
		}
	}

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
	(void) ts;

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
	(void) ts;


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
		(void) vitem;
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

void p3RetroChess::raw_msg_gxs(const RsGxsId& targetId, const RsGxsId& sourceId, const std::string& msg)
{
	RsDbg() << "CHESS: Entering raw_msg_gxs (mutex should be locked). Target=" << targetId << " Source=" << sourceId;
	// Suppression du lock ici pour éviter le deadlock si appelé depuis sendInvite_chat ou msg_chat
	// RsStackMutex stack(mRetroChessMtx);

	if (!mGxsTunnels) {
		RsErr() << "CHESS: mGxsTunnels is NULL, cannot send GXS message.";
		return;
	}

	TunnelKey key = { sourceId, targetId };

	if (mGxsToTunnelMap.find(key) == mGxsToTunnelMap.end()) {
		RsGxsTunnelId tunnelId;
		uint32_t error_code = 0;
		RsDbg() << "CHESS: Requesting tunnel from " << sourceId << " to " << targetId;
		if (mGxsTunnels->requestSecuredTunnel(targetId, sourceId, tunnelId, RS_SERVICE_TYPE_RetroChess_PLUGIN, error_code)) {
			mGxsToTunnelMap[key] = tunnelId;
			mTargetGxsToTunnelMap[targetId] = tunnelId;
			mTargetToOwnGxsIdMap[targetId] = sourceId;
			mTunnelToPeerGxsIdMap[tunnelId] = targetId;

			// On mémorise aussi notre propre ID pour que getAvatar fonctionne pour nous
			mPseudoToRealGxsMap[RsPeerId(sourceId.toStdString())] = sourceId;
			mPseudoToNameMap[RsPeerId(sourceId.toStdString())] = getGxsName(sourceId);

			RsDbg() << "CHESS: Tunnel requested successfully: " << tunnelId;
		} else {
			RsErr() << "CHESS: Failed to request tunnel, error=" << error_code;
			return;
		}
	}

	RsGxsTunnelId tunnelId = mGxsToTunnelMap[key];
	RsGxsTunnelService::GxsTunnelInfo info;
	if (mGxsTunnels->getTunnelInfo(tunnelId, info) && info.tunnel_status == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_CAN_TALK) {
		RsDbg() << "CHESS: Tunnel is ready, sending data (" << msg.size() << " bytes).";
		mGxsTunnels->sendData(tunnelId, RS_SERVICE_TYPE_RetroChess_PLUGIN, (const uint8_t*)msg.c_str(), msg.size());
	} else {
		RsDbg() << "CHESS: Tunnel not ready (status=" << (mGxsTunnels->getTunnelInfo(tunnelId, info) ? info.tunnel_status : 0) << "), queuing message.";
		mPendingTunnelMessages[tunnelId].push_back(msg);
	}
}

void p3RetroChess::notifyTunnelStatus(const RsGxsTunnelId& tunnel_id, uint32_t tunnel_status)
{
	RsDbg() << "CHESS: notifyTunnelStatus: tunnel=" << tunnel_id << ", status=" << tunnel_status;

	RsStackMutex stack(mRetroChessMtx);
	if (tunnel_status == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_CAN_TALK) {
		std::list<std::string>& pending = mPendingTunnelMessages[tunnel_id];
		if (!pending.empty()) {
			RsDbg() << "CHESS: Tunnel CAN_TALK, flushing " << pending.size() << " pending messages.";
			for (const std::string& msg : pending) {
				mGxsTunnels->sendData(tunnel_id, RS_SERVICE_TYPE_RetroChess_PLUGIN, (const uint8_t*)msg.c_str(), msg.size());
			}
			pending.clear();
		}
	}
}

void p3RetroChess::receiveData(const RsGxsTunnelId& tunnel_id, unsigned char *data, uint32_t data_size)
{
	RsDbg() << "CHESS: receiveData: tunnel=" << tunnel_id << ", size=" << data_size;

	std::string msg((const char*)data, data_size);
	RsDbg() << "CHESS: Received message via tunnel: " << msg;

	RsGxsId peerGxsId;
	RsPeerId pseudoPeerId;
	bool tunnelFound = false;

	{
		RsStackMutex stack(mRetroChessMtx);
		if (mTunnelToPeerGxsIdMap.find(tunnel_id) != mTunnelToPeerGxsIdMap.end()) {
			peerGxsId = mTunnelToPeerGxsIdMap[tunnel_id];
			pseudoPeerId = RsPeerId(peerGxsId.toStdString());
			tunnelFound = true;
		}
	}

	if (tunnelFound) {
		// Décodage JSON pour déclencher les bons signaux
		QJsonDocument jsondoc = QJsonDocument::fromJson(QByteArray::fromStdString(msg));
		QVariantMap map = jsondoc.toVariant().toMap();
		QString type = map.value("type").toString();

		if (type == "chess_invite") {
			RsDbg() << "CHESS: Handling incoming invitation from GXS " << peerGxsId;
			{
				RsStackMutex stack(mRetroChessMtx);
				gxsInvitesFrom.insert(peerGxsId);
				mPseudoToNameMap[pseudoPeerId] = getGxsName(peerGxsId);
				mPseudoToRealGxsMap[pseudoPeerId] = peerGxsId;
			}
			mNotify->notifyChessInvite(pseudoPeerId);
		} else if (type == "chess_hello") {
			RsDbg() << "CHESS: Received HELLO from GXS " << peerGxsId;
			{
				RsStackMutex stack(mRetroChessMtx);
				gxsPeersReady.insert(peerGxsId);
			}
			// On répond par un ACK
			RsGxsTunnelService::GxsTunnelInfo info;
			if (mGxsTunnels->getTunnelInfo(tunnel_id, info)) {
				std::string ack = "{\"type\":\"chess_hello_ack\"}";
				raw_msg_gxs(peerGxsId, info.source_gxs_id, ack);
			}
			mNotify->notifyChessInvite(pseudoPeerId); // Refresh UI
		} else if (type == "chess_hello_ack") {
			RsDbg() << "CHESS: Received HELLO_ACK from GXS " << peerGxsId;
			{
				RsStackMutex stack(mRetroChessMtx);
				gxsPeersReady.insert(peerGxsId);
			}
			mNotify->notifyChessInvite(pseudoPeerId); // Refresh UI
		} else if (type == "chess_accept") {
			RsDbg() << "CHESS: Handling incoming acceptance from GXS " << peerGxsId;
			{
				RsStackMutex stack(mRetroChessMtx);
				gxsInvitesTo.erase(peerGxsId);
				mPseudoToNameMap[pseudoPeerId] = getGxsName(peerGxsId);
				mPseudoToRealGxsMap[pseudoPeerId] = peerGxsId;
				RsDbg() << "CHESS: Mapped pseudo " << pseudoPeerId << " to name " << mPseudoToNameMap[pseudoPeerId];
			}
			mNotify->notifyChessStart(pseudoPeerId, 0); // Inviteur = Blanc (0)
		} else {
			// Pour les autres messages (coups, etc.), on utilise la méthode générique
			mNotify->notifyReceivedMsg(pseudoPeerId, QString::fromStdString(msg));
		}
	} else {
		RsWarn() << "CHESS: Received data for unknown tunnel " << tunnel_id;
	}

	// Important: RetroShare tunnel service transfers ownership of 'data', we must free it.
	free(data);
}

void p3RetroChess::connectToGxsTunnelService(RsGxsTunnelService *tunnel_service)
{
	RsDbg() << "CHESS: connectToGxsTunnelService: " << (tunnel_service ? "valid pointer" : "NULL");
	mGxsTunnels = tunnel_service;

	if(mGxsTunnels)
	{
		RsDbg() << "CHESS: Registering RetroChess as GXS Tunnel client (Service ID: " << RS_SERVICE_TYPE_RetroChess_PLUGIN << ")";
		if(!mGxsTunnels->registerClientService(RS_SERVICE_TYPE_RetroChess_PLUGIN, this))
		{
			RsErr() << "CHESS: Failed to register RetroChess GXS Tunnel client!";
		}
	}
}

bool p3RetroChess::acceptDataFromPeer(const RsGxsId& gxs_id, const RsGxsTunnelId& tunnel_id, bool am_I_client_side)
{
	RsDbg() << "CHESS: acceptDataFromPeer: id=" << gxs_id << ", tunnel=" << tunnel_id << ", side=" << (am_I_client_side ? "client" : "server");
	
	RsStackMutex stack(mRetroChessMtx);
	mTunnelToPeerGxsIdMap[tunnel_id] = gxs_id;
	
	return true;
}

void p3RetroChess::connectToIdentityService(RsIdentity *identity_service)
{
	RsDbg() << "CHESS: connectToIdentityService: " << (identity_service ? "valid pointer" : "NULL");
	mIdentity = identity_service;
}

void p3RetroChess::connectToChatService(RsChats *chat_service)
{
	RsDbg() << "CHESS: connectToChatService: " << (chat_service ? "valid pointer" : "NULL");
	mChats = chat_service;
}

void p3RetroChess::msg_chat(const ChatId& chatId, const std::string& msg)
{
	RsDbg() << "CHESS: msg_chat for chatId: " << chatId.toStdString();
	RsChats *chats = mChats ? mChats : rsChats;

	RsStackMutex stack(mRetroChessMtx); // On verrouille ici pour toute l'opération
	if (chatId.isPeerId()) {
		RsDbg() << "CHESS: msg_chat identified as PeerId";
		raw_msg_peer(chatId.toPeerId(), msg);
	} else if (chatId.isDistantChatId()) {
		RsDbg() << "CHESS: msg_chat identified as DistantChatId (using " << (mChats?"mChats":"rsChats") << ")";
		if (!chats) {
			RsErr() << "CHESS: No chat service available, cannot send GXS message.";
			return;
		}
		DistantChatPeerInfo info;
		if (chats->getDistantChatStatus(chatId.toDistantChatId(), info)) {
			RsDbg() << "CHESS: Found distant chat status: from=" << info.own_id << " to=" << info.to_id;
			raw_msg_gxs(info.to_id, info.own_id, msg);
		} else {
			RsErr() << "CHESS: Could not get distant chat status for " << chatId.toStdString();
		}
	} else {
		RsWarn() << "CHESS: msg_chat: unknown ChatId type for " << chatId.toStdString();
	}
}

void p3RetroChess::chess_click_chat(const ChatId& chatId, int col, int row, int count)
{
	QVariantMap map;
	map.insert("type", "chessclick");
	map.insert("col", col);
	map.insert("row", row);
	map.insert("count", count);

	QJsonDocument jsondoc = QJsonDocument::fromVariant(map);
	std::string msg = jsondoc.toJson().toStdString();
	msg_chat(chatId, msg);
}

void p3RetroChess::player_leave_chat(const ChatId& chatId)
{
	std::string msg = "{\"type\":\"player_status_message\",\"player_status\":\"leave\"}";
	msg_chat(chatId, msg);
}

void p3RetroChess::sendInvite_chat(const ChatId& chatId)
{
	RsDbg() << "CHESS: sendInvite_chat for: " << chatId.toStdString();
	RsChats *chats = mChats ? mChats : rsChats;

	if (chatId.isPeerId()) {
		RsDbg() << "CHESS: Identified as PeerId";
		sendInvite(chatId.toPeerId());
	} else if (chatId.isDistantChatId()) {
		RsDbg() << "CHESS: Identified as DistantChatId (using " << (mChats?"mChats":"rsChats") << ")";
		DistantChatPeerInfo info;
		if (chats && chats->getDistantChatStatus(chatId.toDistantChatId(), info)) {
			RsPeerId pseudoPeerId(info.to_id.toStdString());

			{
				RsStackMutex stack(mRetroChessMtx);
				gxsInvitesTo.insert(info.to_id);
				mPseudoToRealGxsMap[pseudoPeerId] = info.to_id;
				mPseudoToNameMap[pseudoPeerId] = getGxsName(info.to_id);
				RsDbg() << "CHESS: Sending invite to GXS " << info.to_id << " (pseudo=" << pseudoPeerId << ")";
			}
			raw_msg_gxs(info.to_id, info.own_id, "{\"type\":\"chess_invite\"}");
		} else {
			RsWarn() << "CHESS: Failed to get distant chat status for " << chatId.toStdString() << " (chats=" << (chats?"OK":"NULL") << ")";
		}
	} else {
		RsWarn() << "CHESS: sendInvite_chat: unknown ID type for " << chatId.toStdString();
	}
}

void p3RetroChess::pokeTunnel_chat(const ChatId& chatId)
{
	RsDbg() << "CHESS: pokeTunnel_chat for: " << chatId.toStdString();
	if (chatId.isDistantChatId()) {
		RsChats *chats = mChats ? mChats : rsChats;
		DistantChatPeerInfo info;
		if (chats && chats->getDistantChatStatus(chatId.toDistantChatId(), info)) {
			RsDbg() << "CHESS: Poking GXS tunnel with HELLO for " << info.to_id;
			std::string hello = "{\"type\":\"chess_hello\"}";
			raw_msg_gxs(info.to_id, info.own_id, hello);
		}
	}
}

bool p3RetroChess::isPeerReady_chat(const ChatId& chatId)
{
	RsChats *chats = mChats ? mChats : rsChats;
	if (chatId.isDistantChatId()) {
		DistantChatPeerInfo info;
		if (chats && chats->getDistantChatStatus(chatId.toDistantChatId(), info)) {
			RsStackMutex stack(mRetroChessMtx);
			return gxsPeersReady.find(info.to_id) != gxsPeersReady.end();
		}
	}
	return false;
}

void p3RetroChess::acceptedInvite_chat(const ChatId& chatId)
{
	RsDbg() << "CHESS: acceptedInvite_chat for: " << chatId.toStdString();
	RsChats *chats = mChats ? mChats : rsChats;

	if (chatId.isPeerId()) {
		acceptedInvite(chatId.toPeerId());
	} else if (chatId.isDistantChatId()) {
		DistantChatPeerInfo info;
		if (chats && chats->getDistantChatStatus(chatId.toDistantChatId(), info)) {
			RsPeerId pseudoPeerId(info.to_id.toStdString());

			{
				RsStackMutex stack(mRetroChessMtx);
				gxsInvitesTo.erase(info.to_id);
				gxsInvitesFrom.erase(info.to_id);
				
				// On mémorise le nom et l'ID réel avant d'ouvrir la fenêtre
				mPseudoToNameMap[pseudoPeerId] = getGxsName(info.to_id);
				mPseudoToRealGxsMap[pseudoPeerId] = info.to_id;
				RsDbg() << "CHESS: Mapped pseudo " << pseudoPeerId << " to name " << mPseudoToNameMap[pseudoPeerId];
			}

			raw_msg_gxs(info.to_id, info.own_id, "{\"type\":\"chess_accept\"}");

			// On ne notifie plus ici, c'est l'UI qui le fait pour éviter le double échiquier
			// mNotify->notifyChessStart(pseudoPeerId, 1);
		}
	}
}

bool p3RetroChess::getAvatar(const RsPeerId& id, QPixmap& avatar)
{
	RsStackMutex stack(mRetroChessMtx);
	RsGxsId gxsId;

	if (mPseudoToRealGxsMap.count(id)) {
		gxsId = mPseudoToRealGxsMap[id];
		RsDbg() << "CHESS: getAvatar - Found GXS ID for pseudo " << id << " -> " << gxsId;
	} else {
		// Tentative de détection directe si l'ID est binaire mais compatible
		gxsId = RsGxsId(id.toStdString());
	}

	if (!gxsId.isNull()) {
		if (AvatarDefs::getAvatarFromGxsId(gxsId, avatar)) {
			RsDbg() << "CHESS: getAvatar - Successfully loaded GXS avatar for " << gxsId;
			return true;
		}
	}
	
	RsDbg() << "CHESS: getAvatar - Falling back to SSL avatar for " << id;
	return AvatarDefs::getAvatarFromSslId(id, avatar);
}

std::string p3RetroChess::getGxsName(const RsGxsId& gxs_id)
{
	if (mIdentity) {
		RsIdentityDetails details;
		if (mIdentity->getIdDetails(gxs_id, details)) {
			return details.mNickname;
		}
	}
	return "GXS Friend";
}

std::string p3RetroChess::getPeerName(const RsPeerId& id)
{
	RsStackMutex stack(mRetroChessMtx);
	RsDbg() << "CHESS: getPeerName lookup for: " << id;
	if (mPseudoToNameMap.count(id)) {
		RsDbg() << "CHESS: Found name in pseudo map: " << mPseudoToNameMap[id];
		return mPseudoToNameMap[id];
	}
	
	// Fallback : si c'est un pseudo-ID GXS, on tente une résolution directe
	RsGxsId gxsId(id.toStdString());
	if (mPseudoToRealGxsMap.count(id)) {
		gxsId = mPseudoToRealGxsMap[id];
	}
	
	if (!gxsId.isNull()) {
		std::string name = getGxsName(gxsId);
		if (!name.empty() && name != "GXS Friend") return name;
	}
	
	return rsPeers->getPeerName(id);
}

bool p3RetroChess::hasInviteFrom_chat(const ChatId& chatId)
{
	RsChats *chats = mChats ? mChats : rsChats;
	if (chatId.isPeerId()) {
		return hasInviteFrom(chatId.toPeerId());
	} else if (chatId.isDistantChatId()) {
		DistantChatPeerInfo info;
		if (chats && chats->getDistantChatStatus(chatId.toDistantChatId(), info)) {
			RsStackMutex stack(mRetroChessMtx);
			return gxsInvitesFrom.find(info.to_id) != gxsInvitesFrom.end();
		}
	}
	return false;
}

bool p3RetroChess::hasInviteTo_chat(const ChatId& chatId)
{
	RsChats *chats = mChats ? mChats : rsChats;
	if (chatId.isPeerId()) {
		return hasInviteTo(chatId.toPeerId());
	} else if (chatId.isDistantChatId()) {
		DistantChatPeerInfo info;
		if (chats && chats->getDistantChatStatus(chatId.toDistantChatId(), info)) {
			RsStackMutex stack(mRetroChessMtx);
			return gxsInvitesTo.find(info.to_id) != gxsInvitesTo.end();
		}
	}
	return false;
}

