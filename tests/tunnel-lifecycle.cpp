// Regression tests for the GXS tunnel lifecycle (tunnels must be closed when
// they go down or are closed remotely, presence retries must back off and go
// dormant, both sides must not dial each other at once).
// The runner (tunnel-lifecycle.py) inserts the production methods into this
// deterministic transport fixture. No RetroShare profile or network is used.
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>
#include <QUuid>
#include <QString>
#include <algorithm>
#include <cassert>
#include <ctime>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <vector>

struct Id {
    int value = 0;
    Id(int v = 0) : value(v) {}
    explicit Id(const std::string &text) : value(text.empty() ? 0 : std::stoi(text)) {}
    bool isNull() const { return value == 0; }
    std::string toStdString() const { return std::to_string(value); }
    bool operator<(const Id &other) const { return value < other.value; }
    bool operator==(const Id &other) const { return value == other.value; }
    bool operator!=(const Id &other) const { return !(*this == other); }
};
static std::ostream &operator<<(std::ostream &stream, Id id) { return stream << id.value; }
using RsGxsId = Id;
using RsGxsTunnelId = Id;
struct RsStackMutex { explicit RsStackMutex(int &) {} };
constexpr int RETRO_CHESS_GXS_TUNNEL_SERVICE_ID = 0xC4E5;
static time_t clockNow = 100000;
static time_t testTime(time_t *) { return clockNow; }

#define time testTime
#include "services/RetroChessTunnelDebug.h"
#undef time
using RetroChessTunnelDebug::statusName;
using RetroChessTunnelDebug::payloadType;

struct ChessTimeControl {
    bool unlimited = true;
    QString toNetString() const { return QString(); }
    static ChessTimeControl fromNetString(const QString &) { return ChessTimeControl(); }
};
struct RsIdentityDetails { std::string mNickname; };
struct Identity {
    Id own = 5;
    void getOwnIds(std::list<Id> &ids) { ids = {own}; }
    bool isOwnId(Id id) { return id == own; }
    bool getIdDetails(Id, RsIdentityDetails &) { return false; }
};
static Identity identity;
static Identity *rsIdentity = &identity;
struct RsAccounts { static std::string AccountDirectory() { return std::string(); } };
struct Notify {
    unsigned int changes = 0, closed = 0, ready = 0;
    void notifyAvailablePeersChanged() { ++changes; }
    void notifyGxsTunnelClosed(const Id &) { ++closed; }
    void notifyGxsTunnelReady(const Id &) { ++ready; }
};
// Fake GxsTunnel: tunnel id = 1000 + peer, like the real deterministic ids.
struct RsGxsTunnelService {
    enum {
        RS_GXS_TUNNEL_STATUS_UNKNOWN = 0, RS_GXS_TUNNEL_STATUS_TUNNEL_DN = 1,
        RS_GXS_TUNNEL_STATUS_CAN_TALK = 2, RS_GXS_TUNNEL_STATUS_REMOTELY_CLOSED = 3
    };
    struct GxsTunnelInfo {
        Id tunnel_id, destination_gxs_id, source_gxs_id = 5;
        uint32_t tunnel_status = 0, total_size_sent = 0, total_size_received = 0, pending_data_packets = 0;
    };
    std::map<Id, uint32_t> tunnels;   // tunnel -> status
    std::map<Id, uint32_t> unacked;   // tunnel -> packets not acknowledged yet
    uint32_t newTunnelStatus = RS_GXS_TUNNEL_STATUS_CAN_TALK;
    unsigned int requests = 0;
    std::vector<std::pair<Id, QVariantMap>> sends;
    std::vector<Id> closes;
    bool getTunnelInfo(Id tunnel, GxsTunnelInfo &info) {
        auto it = tunnels.find(tunnel);
        if (it == tunnels.end()) return false;
        info.tunnel_id = tunnel;
        info.destination_gxs_id = Id(tunnel.value - 1000);
        info.tunnel_status = it->second;
        info.pending_data_packets = unacked.count(tunnel) ? unacked[tunnel] : 0;
        return true;
    }
    bool requestSecuredTunnel(Id to, Id, Id &tunnel, int, uint32_t &) {
        ++requests;
        tunnel = Id(1000 + to.value);
        if (!tunnels.count(tunnel)) tunnels[tunnel] = newTunnelStatus;
        return true;
    }
    bool sendData(Id tunnel, int service, const uint8_t *bytes, unsigned int size) {
        assert(service == RETRO_CHESS_GXS_TUNNEL_SERVICE_ID);
        sends.push_back({tunnel, QJsonDocument::fromJson(QByteArray(reinterpret_cast<const char *>(bytes), size)).toVariant().toMap()});
        return tunnels.count(tunnel) && tunnels[tunnel] == RS_GXS_TUNNEL_STATUS_CAN_TALK;
    }
    bool getTunnelsInfo(std::vector<GxsTunnelInfo> &infos) {
        for (const auto &t : tunnels) { GxsTunnelInfo info; getTunnelInfo(t.first, info); infos.push_back(info); }
        return true;
    }
    bool closeExistingTunnel(Id tunnel, int) {
        closes.push_back(tunnel);
        return tunnels.erase(tunnel) != 0;
    }
};
struct RsRetroChessGameSession {
    bool gxs = true;
    QString endpointId, localIdentityId = "5", gameId;
};

struct p3RetroChess {
    // CONTACT_STRUCT
    std::map<Id, ChessContact> mChessContacts;
    std::map<Id, Id> mPendingTunnels, mActiveTunnels, mTunnelToGxsIdMap, mOwnGxsIdByPeer;
    std::map<Id, std::pair<Id, time_t>> mPendingTunnelSince;
    std::map<Id, std::string> mPendingGxsInvites;
    std::map<Id, time_t> mPendingGxsCloses;
    std::map<Id, QString> mPendingWatchRequests, mGameIdByPeer;
    std::map<std::string, RsRetroChessGameSession> mGameSessions;
    std::set<Id> mInvitesToGxs, mInvitesFromGxs, mJoinRequestsFromGxs;
    std::set<Id> mTunnelsToClose, mOpenedTunnels;
    struct DeferredClose { time_t deadline; std::string reason; };
    std::map<Id, DeferredClose> mDeferredCloses;
    time_t mLastOrphanSweep = 0, mLastTunnelDump = 0;
    int mRetroChessMtx = 0;
    bool mChessBusy = false, mLobbySeekActive = false;
    ChessTimeControl mLobbySeek;
    RsGxsTunnelService transport;
    Notify notify;
    RsGxsTunnelService *mGxsTunnels = &transport;
    Notify *mNotify = &notify;
    void IndicateConfigChanged() {}
    void addOwnFlairLocked(QVariantMap &, const Id &) const {}
    Id preferredChessIdentity() { return Id(5); }
    std::list<Id> chessIdentitiesFrom(std::list<Id> own) { return own; }
    Id preferredChessIdentityFrom(const std::list<Id> &ids) { return ids.empty() ? Id() : ids.front(); }
    bool chessIdentityEnabled(Id id) { return id == Id(5); }
    bool openGxsTunnel(const Id &to, const Id &from, Id &tunnel, const char *reason);
    // Same bookkeeping as the production sendGxsInvite().
    void requestGxsTunnel(const Id &id) {
        Id tunnel;
        if (mPendingTunnels.count(id) || mActiveTunnels.count(id)) return;
        if (openGxsTunnel(id, Id(5), tunnel, "test")) {
            mPendingTunnels[id] = tunnel;
            mOpenedTunnels.insert(tunnel);
        }
    }
    bool sendGxsData(const Id &tunnel, const uint8_t *data, uint32_t size);
    void dumpTunnelState();
    bool closeGxsTunnel(const Id &tunnel, const char *reason);
    bool closeGxsTunnelNow(const Id &tunnel, const char *reason);
    void processDeferredCloses();
    void closeQueuedGxsTunnels();
    void sweepOrphanGxsTunnels();
    void tickChessPresence();
    bool handleChessPresence(const Id &, const Id &, const RsGxsTunnelService::GxsTunnelInfo &, const QVariantMap &);
    void notifyTunnelStatus(const Id &, uint32_t);
    void handleGxsTick();
    // Same order as the production tick().
    void tick() {
        handleGxsTick();
        closeQueuedGxsTunnels();
        processDeferredCloses();
        tickChessPresence();
        sweepOrphanGxsTunnels();
        dumpTunnelState();
    }
};

#define time testTime
// PRODUCTION_METHODS
#undef time

using S = RsGxsTunnelService;

int main()
{
    // 1. A remote close must release the tunnel (not just forget it), and back off.
    {
        p3RetroChess chess;
        chess.mChessContacts[7];
        chess.tick();                                   // opens tunnel 1007
        assert(chess.mPendingTunnels.count(7) && chess.transport.requests == 1);
        chess.tick();                                   // CAN_TALK -> active, probe sent
        assert(chess.mActiveTunnels.count(7));
        chess.notifyTunnelStatus(1007, S::RS_GXS_TUNNEL_STATUS_REMOTELY_CLOSED);
        assert(!chess.mActiveTunnels.count(7) && !chess.mPendingTunnels.count(7));
        assert(chess.transport.closes.empty());         // not from inside the callback
        chess.closeQueuedGxsTunnels();
        assert(chess.transport.closes.size() == 1 && chess.transport.closes[0] == Id(1007));
        assert(!chess.mOpenedTunnels.count(1007));
        assert(chess.mChessContacts.at(7).failures == 1);
        assert(chess.mChessContacts.at(7).nextProbe == clockNow + 15);
    }

    // 2. A temporary TUNNEL_DN keeps the tunnel under watch and re-adopts it.
    {
        p3RetroChess chess;
        chess.mActiveTunnels[7] = 1007;
        chess.transport.tunnels[1007] = S::RS_GXS_TUNNEL_STATUS_TUNNEL_DN;
        chess.mGameSessions["7"] = {};
        chess.notifyTunnelStatus(1007, S::RS_GXS_TUNNEL_STATUS_TUNNEL_DN);
        assert(chess.mPendingTunnels.count(7) && chess.mTunnelsToClose.empty());
        chess.tick();
        assert(chess.transport.closes.empty() && chess.mPendingTunnels.count(7));
        chess.transport.tunnels[1007] = S::RS_GXS_TUNNEL_STATUS_CAN_TALK;   // turtle found a new route
        chess.tick();
        assert(chess.mActiveTunnels.count(7) && chess.notify.ready == 1);
        assert(chess.transport.closes.empty());
    }

    // 3. ...and closes it when it never comes back (no endless re-digging).
    {
        p3RetroChess chess;
        chess.mActiveTunnels[7] = 1007;
        chess.transport.tunnels[1007] = S::RS_GXS_TUNNEL_STATUS_TUNNEL_DN;
        chess.notifyTunnelStatus(1007, S::RS_GXS_TUNNEL_STATUS_TUNNEL_DN);
        chess.tick();
        clockNow += 121;
        chess.tick();
        assert(!chess.mPendingTunnels.count(7));
        assert(chess.transport.closes.size() == 1 && !chess.transport.tunnels.count(1007));
    }

    // 4. Down/closed tunnels we do not track any more are released too.
    {
        p3RetroChess chess;
        chess.transport.tunnels[1009] = S::RS_GXS_TUNNEL_STATUS_TUNNEL_DN;
        chess.notifyTunnelStatus(1009, S::RS_GXS_TUNNEL_STATUS_TUNNEL_DN);
        chess.closeQueuedGxsTunnels();
        assert(chess.transport.closes.size() == 1 && chess.transport.closes[0] == Id(1009));
    }

    // 5. A queued close is skipped when the same (deterministic) tunnel id is in use again.
    {
        p3RetroChess chess;
        chess.mActiveTunnels[7] = 1007;
        chess.notifyTunnelStatus(1007, S::RS_GXS_TUNNEL_STATUS_REMOTELY_CLOSED);
        chess.mPendingTunnels[7] = 1007;                // user re-invited before tick()
        chess.closeQueuedGxsTunnels();
        assert(chess.transport.closes.empty() && chess.mTunnelsToClose.empty());
    }

    // 6. Orphan sweep: a tunnel we opened but no longer track is closed.
    {
        p3RetroChess chess;
        chess.transport.tunnels[1008] = S::RS_GXS_TUNNEL_STATUS_TUNNEL_DN;
        chess.mOpenedTunnels.insert(1008);
        chess.mOpenedTunnels.insert(1007);
        chess.mActiveTunnels[7] = 1007;
        chess.transport.tunnels[1007] = S::RS_GXS_TUNNEL_STATUS_CAN_TALK;
        chess.sweepOrphanGxsTunnels();
        assert(chess.transport.closes.size() == 1 && chess.transport.closes[0] == Id(1008));
        assert(chess.mOpenedTunnels.count(1007));
    }

    // 7. Tie-break: the higher id waits for the lower id to dial.
    {
        p3RetroChess chess;                 // own id is 5
        chess.mChessContacts[3];            // lower than us: they dial first
        chess.mChessContacts[7];            // higher than us: we dial
        chess.tick();
        assert(chess.mPendingTunnels.count(7) && !chess.mPendingTunnels.count(3));
        assert(chess.mChessContacts.at(3).yieldUntil == clockNow + kPresenceYieldSec);
        // Peer 3 dials us: its probe is answered and its tunnel adopted.
        chess.transport.tunnels[1003] = S::RS_GXS_TUNNEL_STATUS_CAN_TALK;
        QVariantMap request;
        request["type"] = "chess_presence_request";
        request["version"] = 1;
        request["nonce"] = "n";
        S::GxsTunnelInfo info;
        assert(chess.transport.getTunnelInfo(1003, info));
        chess.handleChessPresence(3, 1003, info, request);
        assert(chess.mActiveTunnels.count(3) && chess.mActiveTunnels.at(3) == Id(1003));
        assert(chess.mChessContacts.at(3).yieldUntil == 0);
        clockNow += kPresenceYieldSec + 1;
        chess.tick();
        assert(chess.transport.requests == 1);          // never dialed peer 3 ourselves

        // A lower contact that does not list us is still dialed after the wait.
        p3RetroChess lonely;
        lonely.mChessContacts[3];
        lonely.tick();
        assert(lonely.transport.requests == 0);
        clockNow += kPresenceYieldSec;
        lonely.tick();
        assert(lonely.transport.requests == 1 && lonely.mPendingTunnels.count(3));
    }

    // 8. Friend with RetroChess disabled: the tunnel connects (GxsTunnel is in
    //    the RetroShare core) but probes are never answered. Count redials in 24h.
    {
        p3RetroChess chess;
        chess.mChessContacts[7];
        const time_t start = clockNow;
        for (; clockNow < start + 24 * 3600; ++clockNow) chess.tick();
        const auto &c = chess.mChessContacts.at(7);
        std::cout << "disabled-plugin friend: " << chess.transport.requests
                  << " tunnel requests in 24h (failures=" << c.failures << ")\n";
        assert(c.failures == kPresenceMaxFailures);
        assert(chess.transport.requests <= 32);         // old code: ~290 per day
        assert(chess.mActiveTunnels.empty() || chess.mChessContacts.at(7).deadline);
    }

    // 9. A close waits until GxsTunnel has the last packets acknowledged
    //    (GxsTunnel never frees unacknowledged packets of a closed tunnel).
    {
        p3RetroChess chess;
        chess.transport.tunnels[1007] = S::RS_GXS_TUNNEL_STATUS_CAN_TALK;
        chess.transport.unacked[1007] = 1;             // e.g. player_leave in flight
        chess.mOpenedTunnels.insert(1007);
        chess.closeGxsTunnel(1007, "test");
        assert(chess.transport.closes.empty() && chess.mDeferredCloses.count(1007));
        chess.processDeferredCloses();
        assert(chess.transport.closes.empty());
        chess.transport.unacked[1007] = 0;             // acknowledged
        chess.processDeferredCloses();
        assert(chess.transport.closes.size() == 1 && chess.mDeferredCloses.empty());
        assert(!chess.mOpenedTunnels.count(1007));

        // Never acknowledged: closed anyway after the grace period.
        p3RetroChess stuck;
        stuck.transport.tunnels[1008] = S::RS_GXS_TUNNEL_STATUS_TUNNEL_DN;
        stuck.transport.unacked[1008] = 2;
        stuck.closeGxsTunnel(1008, "test");
        stuck.processDeferredCloses();
        assert(stuck.transport.closes.empty());
        clockNow += 61;
        stuck.processDeferredCloses();
        assert(stuck.transport.closes.size() == 1);

        // Remotely closed: nothing will be acknowledged, close right away.
        p3RetroChess remote;
        remote.transport.tunnels[1009] = S::RS_GXS_TUNNEL_STATUS_REMOTELY_CLOSED;
        remote.transport.unacked[1009] = 1;
        remote.closeGxsTunnel(1009, "test");
        assert(remote.transport.closes.size() == 1 && remote.mDeferredCloses.empty());

        // Picked up again (same deterministic id) before the close happened.
        p3RetroChess reused;
        reused.transport.tunnels[1007] = S::RS_GXS_TUNNEL_STATUS_CAN_TALK;
        reused.transport.unacked[1007] = 1;
        reused.closeGxsTunnel(1007, "test");
        reused.mActiveTunnels[7] = 1007;
        reused.processDeferredCloses();
        assert(reused.transport.closes.empty() && reused.mDeferredCloses.empty());
    }

    std::cout << "Tunnel lifecycle regression checks passed\n";
}
