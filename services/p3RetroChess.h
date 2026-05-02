/*******************************************************************************
 * services/p3RetroChess.h                                                     *
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
/* this handles the networking service of this plugin */

#pragma once

#include <list>
#include <string>
#include <QVariantMap>

#include "services/rsRetroChessItems.h"
#include "services/p3service.h"
#include "serialiser/rstlvbase.h"
#include "rsitems/rsconfigitems.h"
#include "plugins/rspqiservice.h"
#include <interface/rsRetroChess.h>
#include "retroshare/rsgxstunnel.h"
#include "retroshare/rsidentity.h"

class p3LinkMgr;
class RetroChessNotify ;



//!The RS VoIP Test service.
/**
 *
 * This is only used to test Latency for the moment.
 */

class p3RetroChess: public RsPQIService, public RsRetroChess, public RsGxsTunnelService::RsGxsTunnelClientService
// Maybe we inherit from these later - but not needed for now.
//, public p3Config, public pqiMonitor
{
public:
	p3RetroChess(RsPluginHandler *cm,RetroChessNotify *);

	/***** overloaded from rsRetroChess *****/


	/***** overloaded from p3Service *****/
	/*!
	 * This retrieves all chat msg items and also (important!)
	 * processes chat-status items that are in service item queue. chat msg item requests are also processed and not returned
	 * (important! also) notifications sent to notify base  on receipt avatar, immediate status and custom status
	 * : notifyCustomState, notifyChatStatus, notifyPeerHasNewAvatar
	 * @see NotifyBase
	 */
	virtual int   tick();
	virtual int   status();
	virtual bool  recvItem(RsItem *item);

	/*************** pqiMonitor callback ***********************/
	//virtual void statusChange(const std::list<pqipeer> &plist);


	/************* from p3Config *******************/
	virtual RsSerialiser *setupSerialiser() ;

	/*!
	 * chat msg items and custom status are saved
	 */
	virtual bool saveList(bool& cleanup, std::list<RsItem*>&) ;
	virtual bool loadList(std::list<RsItem*>& load) ;
	virtual std::string configurationFileName() const
	{
		return "RetroChess.cfg" ;
	}

	virtual RsServiceInfo getServiceInfo() ;

	/***** overloaded from RsGxsTunnelClientService *****/
	virtual void notifyTunnelStatus(const RsGxsTunnelId& tunnel_id, uint32_t tunnel_status) override;
	virtual void receiveData(const RsGxsTunnelId& id, unsigned char *data, uint32_t data_size) override;
	virtual void connectToGxsTunnelService(RsGxsTunnelService *tunnel_service) override;
	virtual bool acceptDataFromPeer(const RsGxsId& gxs_id, const RsGxsTunnelId& tunnel_id, bool am_I_client_side) override;

	void connectToIdentityService(RsIdentity *identity_service);
	void connectToChatService(RsChats *chat_service);
	virtual std::string getPeerName(const RsPeerId& id) override;
	virtual std::string getGxsName(const RsGxsId& gxs_id) override;

	void 	ping_all();

	void broadcast_paint(int x, int y);
	void 	msg_all(std::string msg);
	void str_msg_peer(RsPeerId peerID, QString strdata);
	void raw_msg_peer(RsPeerId peerID, std::string msg);
	void raw_msg_gxs(const RsGxsId& targetId, const RsGxsId& sourceId, const std::string& msg);
	void 	qvm_msg_peer(RsPeerId peerID, QVariantMap data);

    void chess_click(std::string peer_id, int col, int row, int count);
	//void set_peer(RsPeerId peer);
    void player_leave(std::string peer_id);

	bool hasInviteFrom(RsPeerId peerID);
	bool hasInviteTo(RsPeerId peerID);
	void gotInvite(RsPeerId peerID);
	void acceptedInvite(RsPeerId peerID);
	virtual void sendInvite(RsPeerId peerID) override;

	virtual void chess_click_chat(const ChatId& chatId, int col, int row, int count) override;
	virtual void player_leave_chat(const ChatId& chatId) override;
	virtual void sendInvite_chat(const ChatId& chatId) override;
	virtual void acceptedInvite_chat(const ChatId& chatId) override;
	virtual bool hasInviteFrom_chat(const ChatId& chatId) override;
	virtual bool hasInviteTo_chat(const ChatId& chatId) override;
private:


	std::set<RsPeerId> invitesTo;
	std::set<RsPeerId> invitesFrom;
	std::set<RsGxsId> gxsInvitesTo;
	std::set<RsGxsId> gxsInvitesFrom;
	void handleData(RsRetroChessDataItem*) ;
	void msg_chat(const ChatId& chatId, const std::string& msg);

	RsMutex mRetroChessMtx;

	//RsPeerId mPeerID;


	static RsTlvKeyValue push_int_value(const std::string& key,int value) ;

	struct TunnelKey {
		RsGxsId toId;
		RsGxsId ownId;
		bool operator<(const TunnelKey& other) const {
			if (toId != other.toId) return toId < other.toId;
			return ownId < other.ownId;
		}
	};

	std::map<TunnelKey, RsGxsTunnelId> mGxsToTunnelMap;
	std::map<RsGxsId, RsGxsTunnelId> mTargetGxsToTunnelMap;
	std::map<RsGxsTunnelId, RsGxsId> mTunnelToPeerGxsIdMap;
	std::map<RsGxsTunnelId, std::list<std::string>> mPendingTunnelMessages;
	std::map<RsPeerId, std::string> mPseudoToNameMap;
	std::map<RsPeerId, RsGxsId> mPseudoToRealGxsMap;
	static int pop_int_value(const std::string& s) ;


	RsServiceControl *mServiceControl;
	RetroChessNotify *mNotify ;

	RsGxsTunnelService *mGxsTunnels;
	RsIdentity *mIdentity;
	RsChats *mChats;

};
