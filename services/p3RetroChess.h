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
#include <set>
#include <string>
#include <QVariantMap>

#include "services/rsRetroChessItems.h"
#include "services/p3service.h"
#include "serialiser/rstlvbase.h"
#include "rsitems/rsconfigitems.h"
#include "plugins/rspqiservice.h"
#include "retroshare/rsidentity.h"
#include <retroshare/rsgxstunnel.h>
#include <retroshare/rschats.h>

#include <interface/rsRetroChess.h>

class p3LinkMgr;
class RetroChessNotify ;

// Use a valid 16-bit Service ID (below 0xFFFF)
#define RETRO_CHESS_GXS_TUNNEL_SERVICE_ID 0xC4E5

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
	virtual int   tick() override;
	virtual int   status();
	virtual bool  recvItem(RsItem *item);

	/*************** pqiMonitor callback ***********************/



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

	void 	ping_all();

	void broadcast_paint(int x, int y);
	void 	msg_all(std::string msg);
	void str_msg_peer(RsPeerId peerID, QString strdata);
	void raw_msg_peer(RsPeerId peerID, std::string msg);
	void 	qvm_msg_peer(RsPeerId peerID, QVariantMap data);

    void chess_click(std::string peer_id, int col, int row, int count);

    void player_leave(std::string peer_id);

	bool hasInviteFrom(RsPeerId peerID);
	bool hasInviteTo(RsPeerId peerID);
	void gotInvite(RsPeerId peerID);
	void acceptedInvite(RsPeerId peerID);
	void clearInvite(RsPeerId peerID) override;
	void sendInvite(RsPeerId peerID);

	void player_leave_gxs(const RsGxsId &gxs_id);

	void sendGxsInvite(const RsGxsId &toGxsId);
	bool sendInviteToGxs(const RsGxsId &gxsId, bool joinOpenGame = false) override;
	bool isJoinRequestFromGxs(const RsGxsId &gxsId) override;
	bool hasInviteToGxs(const RsGxsId &gxsId) override;
	bool cancelInviteToGxs(const RsGxsId &gxsId) override;
	void acceptedInviteGxs(const RsGxsId &gxsId);
	bool rejectedInviteGxs(const RsGxsId &gxsId) override;
	void clearInviteGxs(const RsGxsId &gxsId) override;
	bool hasInviteFromGxs(const RsGxsId &gxsId) override;
	QString gameIdForPeer(const RsGxsId &gxsId) override;
	void startNewGameIdForPeer(const RsGxsId &gxsId) override;
	RsGxsId ownGxsIdForPeer(const RsGxsId &gxsId) override;
	bool sendRematchGxs(const RsGxsId &gxsId, int localColor) override;
	bool sendGameActionGxs(const RsGxsId &gxsId, const std::string &action) override;
	void registerGameSession(const RsRetroChessGameSession &session) override;
	void updateGameSession(const QString &endpointId, const QString &fen,
	                       uint32_t moveSequence, int lastFromTile = -1, int lastToTile = -1,
	                       const QStringList &moveHistory = QStringList()) override;
	void unregisterGameSession(const QString &endpointId) override;
	std::vector<RsRetroChessGameSession> gameSessions() override;
	std::vector<RsRetroChessAvailablePeer> availableChessPeers() override;
	bool addChessContact(const RsGxsId &id) override;
	void removeChessContact(const RsGxsId &id) override;
	std::list<RsGxsId> chessIdentities() override;
	RsGxsId preferredChessIdentity() override;
	void setChessIdentities(const std::list<RsGxsId> &ids, const RsGxsId &preferred) override;
	bool chessBusy() override;
	void setChessBusy(bool busy) override;
	void chess_click_gxs(const RsGxsId &gxs_id, int col, int row, int count);
	virtual void requestGxsTunnel(const RsGxsId &gxsId) override;

	// Send invite via existing distant chat tunnel (correct approach)
	virtual bool sendInvite_chat(const ChatId &chatId) override;

	// Leaderboard data exchange over GXS tunnels
	bool sendLeaderboardDataGxs(const RsGxsId &gxsId, const QByteArray &data) override;
	void broadcastLeaderboardDataGxs(const QByteArray &data) override;
	std::vector<RsGxsId> activeGxsTunnels() override;

	// Spectator / Live Watching
	bool sendWatchRequestGxs(const RsGxsId &hostPlayerId, const QString &gameKey) override;
	void sendWatchLeaveGxs(const RsGxsId &hostPlayerId, const QString &gameKey) override;

	ChessTimeControl timeControlForPeer(const RsGxsId &gxsId) override;
	void setLobbySeek(bool active, const ChessTimeControl &tc) override;
	void setTimeControlForPeer(const RsGxsId &gxsId, const ChessTimeControl &tc) override;

	// Async tunnel management
	void handleGxsTick(); // Called periodically by the core
	void closePendingGxsTunnels();
	void closeQueuedGxsTunnels();   // closes tunnels queued by notifyTunnelStatus()
	void sweepOrphanGxsTunnels();   // closes tunnels we opened but no longer track
	void dumpTunnelState();         // periodic state dump when tunnel debug is on
	void retryPendingDistantChatInvites(); // Retry invites queued before the tunnel was ready
	void reconnectInterruptedSessions();
	bool doSendInviteOverGxs(const RsGxsId &toId, const RsGxsId &ownId, bool joinOpenGame = false); // Actually request tunnel + queue invite

	virtual uint32_t getGxsTunnelServiceId() const { 
			return RETRO_CHESS_GXS_TUNNEL_SERVICE_ID; 
	}

	// Fix handleRawData signature
	void handleRawData(const RsGxsId& gxs_id, const RsGxsTunnelId& tunnel_id, bool am_I_client_side, const uint8_t *data, uint32_t data_size);

	virtual void notifyTunnelStatus(const RsGxsTunnelId& tunnel_id, uint32_t tunnel_status) override;
	virtual void receiveData(const RsGxsTunnelId& id, unsigned char *data, uint32_t data_size) override;
	virtual void connectToGxsTunnelService(RsGxsTunnelService *tunnel_service) override;
	virtual bool acceptDataFromPeer(const RsGxsId& gxs_id, const RsGxsTunnelId& tunnel_id, bool am_I_client_side) override;

	// Tunnel debug logging (see services/RetroChessTunnelDebug.h)
	void setTunnelDebugEnabled(bool enabled) override;
	bool tunnelDebugEnabled() override;
	bool tunnelTraffic(std::vector<RsGxsTunnelService::GxsTunnelInfo> &infos) override;

private:
	// All tunnel traffic goes through these two helpers so that it can be traced.
	// Neither takes mRetroChessMtx for sending; closeGxsTunnel() does, so it
	// must be called with the mutex released.
	bool sendGxsData(const RsGxsTunnelId &tunnel, const uint8_t *data, uint32_t size);
	bool closeGxsTunnel(const RsGxsTunnelId &tunnel, const char *reason);
	bool openGxsTunnel(const RsGxsId &to, const RsGxsId &from, RsGxsTunnelId &tunnel, const char *reason);

	void tickChessPresence();
	bool handleChessPresence(const RsGxsId &sender, const RsGxsTunnelId &tunnel, const QVariantMap &message);
	bool chessIdentityEnabled(const RsGxsId &id);
	RsGxsId selectChessIdentity(const RsGxsId &peer);
	struct ChessContact {
		time_t lastSeen = 0;
		time_t nextProbe = 0;
		time_t deadline = 0;
		unsigned int failures = 0;
		// Tie-break (see tickChessPresence): the side with the higher identity
		// waits until this time for the other side to open the tunnel first.
		time_t yieldUntil = 0;
		QString status = "unknown";
		QString nonce;
		RsGxsTunnelId probeTunnel;
		QString opponentId;
		QString opponentName;
		QString gameId;
		/// Time control advertised by a "chess_seek" action from this peer.
		ChessTimeControl seekTimeControl;
		bool seeking = false;
	};
	std::map<RsGxsId, ChessContact> mChessContacts;
	std::set<RsGxsId> mChessIdentities;
	RsGxsId mPreferredChessIdentity;
	bool mChessIdentitiesConfigured = false;
	bool mChessBusy = false;
	bool mLobbySeekActive = false;
	ChessTimeControl mLobbySeek;
	// Helper to find which friend sent the data based on the tunnel ID
	RsGxsId findGxsIdByTunnel(const RsGxsTunnelId& tunnel_id);
	// Lobby seeks and leaderboard data only go to tunnels whose peer has
	// answered a chess presence probe (or that carry a game, invite or watch
	// request). Probe tunnels to contacts that never answer get nothing.
	// Caller must hold mRetroChessMtx.
	bool chessPeerConfirmedLocked(const RsGxsId &id) const;

	std::set<RsPeerId> invitesTo;
	std::set<RsPeerId> invitesFrom;
	std::set<RsGxsId> mInvitesFromGxs;
	// GXS identities we sent (or queued) a chess_invite to. A chess_accept
	// from anyone else must be ignored, like hasInviteTo() on the peer path.
	std::set<RsGxsId> mInvitesToGxs;

	void handleData(RsRetroChessDataItem*) ;

	// Tracks GXS IDs that we are currently trying to connect to
	std::map<RsGxsId, RsGxsTunnelId> mPendingTunnels;
	// When each pending tunnel request was first seen by handleGxsTick(), so a
	// peer that never answers does not stay pending (and re-polled) forever.
	std::map<RsGxsId, std::pair<RsGxsTunnelId, time_t> > mPendingTunnelSince;
	// Tracks established tunnels ready for data
	std::map<RsGxsId, RsGxsTunnelId> mActiveTunnels;
	// Pending invite messages to send once a tunnel becomes CAN_TALK
	std::map<RsGxsId, std::string> mPendingGxsInvites;
	// Maps tunnel ID → remote GXS ID (populated by acceptDataFromPeer, server-side)
	std::map<RsGxsTunnelId, RsGxsId> mTunnelToGxsIdMap;
	// Exact local identity used to communicate with each remote GXS identity.
	std::map<RsGxsId, RsGxsId> mOwnGxsIdByPeer;
	std::map<RsGxsId, QString> mGameIdByPeer;
	std::map<RsGxsId, QString> mRematchIdByPeer;
	// Leave messages get a short delivery window before their tunnel is closed.
	std::map<RsGxsId, time_t> mPendingGxsCloses;
	// Tunnels that went down or were closed remotely. Closed from tick(), not
	// from inside the GxsTunnel callback that reported them.
	std::set<RsGxsTunnelId> mTunnelsToClose;
	// Tunnels this plugin requested with requestSecuredTunnel() and has not
	// closed yet. Anything here that is neither active nor pending is an
	// orphan that GxsTunnel/turtle would otherwise keep alive forever.
	std::set<RsGxsTunnelId> mOpenedTunnels;
	time_t mLastOrphanSweep = 0;
	time_t mLastTunnelDump = 0;
	// DistantChatIds for which sendInvite_chat() was called but getDistantChatStatus()
	// failed (tunnel not established yet). Retried every tick() until it succeeds
	// or the give-up deadline passes.
	struct PendingDistantInvite { time_t queuedTS; time_t lastTryTS; };
	std::map<DistantChatPeerId, PendingDistantInvite> mPendingDistantChatInvites;
	std::map<std::string, RsRetroChessGameSession> mGameSessions;
	std::map<std::string, time_t> mLastSessionReconnect;
	std::map<std::string, std::set<RsGxsId>> mSpectatorsByGame;
	std::map<RsGxsId, QString> mPendingWatchRequests;
	std::map<RsGxsId, ChessTimeControl> mInviteTimeControlByPeer;
	std::set<RsGxsId> mJoinRequestsFromGxs;

	RsMutex mRetroChessMtx;
	RsServiceControl *mServiceControl;
	RetroChessNotify *mNotify ;
	RsGxsTunnelService *mGxsTunnels;

	//RsPeerId mPeerID;


	static RsTlvKeyValue push_int_value(const std::string& key,int value) ;
	static int pop_int_value(const std::string& s) ;


};
