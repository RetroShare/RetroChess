// The runner inserts the production presence methods into this deterministic
// transport fixture. No RetroShare profile or network connection is used.
#include <QJsonDocument>
#include <QVariantMap>
#include <QUuid>
#include <QString>
#include <algorithm>
#include <cassert>
#include <ctime>
#include <cstdlib>
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
static time_t clockNow = 1000;
static time_t testTime(time_t *) { return clockNow; }
static unsigned int freedPackets = 0;
static void testFree(void *data) { ++freedPackets; std::free(data); }

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
struct RetroChessFlair { static QString normalize(const QString &flair) { return flair; } };
struct RsIdentityDetails { std::string mNickname; };
struct Identity {
    void getOwnIds(std::list<Id> &ids) { ids = {1}; }
    bool isOwnId(Id id) { return id == Id(1); }
    bool getIdDetails(Id, RsIdentityDetails &) { return false; }
};
static Identity identity;
static Identity *rsIdentity = &identity;
struct Notify {
    unsigned int changes = 0;
    void notifyAvailablePeersChanged() { ++changes; }
};
struct RsGxsTunnelService {
    struct GxsTunnelInfo { Id source_gxs_id = 1, destination_gxs_id = 2; };
    std::vector<std::pair<Id, QVariantMap>> sends;
    std::vector<Id> closes;
    bool getTunnelInfo(Id, GxsTunnelInfo &info) { info.source_gxs_id = 1; return true; }
    bool sendData(Id tunnel, int service, const uint8_t *bytes, unsigned int size) {
        assert(service == RETRO_CHESS_GXS_TUNNEL_SERVICE_ID);
        sends.push_back({tunnel, QJsonDocument::fromJson(QByteArray(reinterpret_cast<const char *>(bytes), size)).toVariant().toMap()});
        return true;
    }
    void closeExistingTunnel(Id id, int) { closes.push_back(id); }
};
struct RsItem { virtual ~RsItem() = default; };
struct RsTlvKeyValue { std::string key, value; };
struct RsConfigKeyValueSet : RsItem {
    struct { std::list<RsTlvKeyValue> pairs; } tlvkvs;
};
struct RsRetroChessGameSession {
    bool gxs = true;
    QString endpointId, localIdentityId = "1", fen, gameId;
    int localColor = 0;
    unsigned int moveSequence = 0;
    bool interrupted = false;
};
struct p3RetroChess {
    // CONTACT_STRUCT
    std::map<Id, ChessContact> mChessContacts;
    std::map<Id, Id> mPendingTunnels, mActiveTunnels, mTunnelToGxsIdMap, mOwnGxsIdByPeer;
    std::map<std::string, RsRetroChessGameSession> mGameSessions;
    std::set<Id> mChessIdentities;
    Id mPreferredChessIdentity;
    bool mChessIdentitiesConfigured = false;
    std::set<Id> mInvitesToGxs, mInvitesFromGxs;
    std::map<Id, QString> mGameIdByPeer, mPendingWatchRequests, mOwnFlair, mPeerFlair;
    int mRetroChessMtx = 0;
    bool mChessBusy = false, mLobbySeekActive = false;
    ChessTimeControl mLobbySeek;
    bool enabled = true;
    unsigned int saves = 0;
    unsigned int delivered = 0;
    RsGxsTunnelService transport;
    Notify notify;
    RsGxsTunnelService *mGxsTunnels = &transport;
    Notify *mNotify = &notify;
    void IndicateConfigChanged() { ++saves; }
    void addOwnFlairLocked(QVariantMap &, const Id &) const {}
    bool sendGxsData(const Id &tunnel, const uint8_t *data, uint32_t size) {
        return transport.sendData(tunnel, RETRO_CHESS_GXS_TUNNEL_SERVICE_ID, data, size);
    }
    bool closeGxsTunnel(const Id &tunnel, const char *) { transport.closeExistingTunnel(tunnel, 0); return true; }
    Id preferredChessIdentity() { return enabled ? Id(1) : Id(); }
    std::list<Id> chessIdentitiesFrom(std::list<Id> own) { return enabled ? own : std::list<Id>(); }
    Id preferredChessIdentityFrom(const std::list<Id> &ids) { return ids.empty() ? Id() : ids.front(); }
    bool chessIdentityEnabled(Id id) { return enabled && id == Id(1); }
    void requestGxsTunnel(Id id) { mPendingTunnels[id] = Id(id.value + 100); }
    void tickChessPresence();
    bool handleChessPresence(const Id &, const Id &, const RsGxsTunnelService::GxsTunnelInfo &, const QVariantMap &);
    // Test helper: the production receiveData() passes the tunnel info along.
    bool handleChessPresence(const Id &sender, const Id &tunnel, const QVariantMap &message) {
        RsGxsTunnelService::GxsTunnelInfo info;
        transport.getTunnelInfo(tunnel, info);
        return handleChessPresence(sender, tunnel, info, message);
    }
    bool saveList(bool &, std::list<RsItem *> &);
    bool loadList(std::list<RsItem *> &);
    bool addChessContact(const Id &);
    void removeChessContact(const Id &);
    bool acceptDataFromPeer(const Id &, const Id &, bool);
    void receiveData(const Id &, unsigned char *, uint32_t);
    void handleRawData(const Id &, const Id &, const RsGxsTunnelService::GxsTunnelInfo &, const uint8_t *, uint32_t) { ++delivered; }
};

#define time testTime
#define free testFree
// PRODUCTION_METHODS
#undef free
#undef time

static QVariantMap replyFor(const p3RetroChess &chess, Id id, const QString &status = "available")
{
    QVariantMap reply;
    reply["type"] = "chess_presence_reply";
    reply["version"] = 1;
    reply["nonce"] = chess.mChessContacts.at(id).nonce;
    reply["status"] = status;
    return reply;
}

int main()
{
    p3RetroChess chess;
    chess.mChessContacts[2];
    chess.tickChessPresence();
    assert(chess.mPendingTunnels.count(2));
    assert(chess.transport.sends.empty()); // Connecting is not presence.
    assert(chess.mChessContacts.at(2).status == "checking");
    chess.mPendingTunnels.erase(2);
    chess.mActiveTunnels[2] = 102;
    chess.tickChessPresence();
    assert(chess.transport.sends.size() == 1);
    assert(chess.transport.sends.back().second["type"] == "chess_presence_request");
    auto reply = replyFor(chess, 2);
    auto wrong = reply;
    wrong["nonce"] = "wrong";
    chess.handleChessPresence(2, 102, wrong);
    chess.handleChessPresence(2, 999, reply);
    wrong = reply;
    wrong["version"] = 2;
    chess.handleChessPresence(2, 102, wrong);
    assert(chess.mChessContacts.at(2).lastSeen == 0);
    chess.handleChessPresence(2, 102, reply);
    assert(chess.mChessContacts.at(2).status == "available");
    assert(chess.mChessContacts.at(2).lastSeen == clockNow && chess.saves == 1);
    chess.handleChessPresence(2, 102, reply); // Duplicate cannot extend presence.
    assert(chess.saves == 1);
    clockNow += 61;
    chess.tickChessPresence();
    assert(chess.transport.sends.size() == 2);
    const auto expiredReply = replyFor(chess, 2);
    clockNow += 46;
    chess.handleChessPresence(2, 102, expiredReply);
    chess.tickChessPresence();
    assert(chess.mChessContacts.count(2)); // Offline contact is retained.
    assert(chess.mChessContacts.at(2).status == "offline");
    assert(chess.transport.closes.size() == 1);
    const time_t next = chess.mChessContacts.at(2).nextProbe;
    chess.tickChessPresence();
    assert(chess.mPendingTunnels.empty()); // Backoff prevents immediate retry.
    clockNow = next;
    chess.tickChessPresence();
    assert(chess.mPendingTunnels.count(2));

    p3RetroChess bounded;
    for (int id = 2; id < 12; ++id) bounded.mChessContacts[id];
    bounded.mChessContacts[1]; // Never probe ourselves, including loaded IDs.
    bounded.tickChessPresence();
    assert(bounded.mPendingTunnels.size() == 4);
    assert(!bounded.mPendingTunnels.count(1));

    p3RetroChess playing;
    playing.mChessContacts[2];
    playing.mActiveTunnels[2] = 102;
    playing.mGameSessions["2"] = {};
    playing.tickChessPresence();
    clockNow += 46;
    playing.tickChessPresence();
    assert(playing.transport.closes.empty() && playing.mActiveTunnels.count(2));
    p3RetroChess invited;
    invited.mChessContacts[2];
    invited.mPendingTunnels[2] = 102;
    invited.mInvitesToGxs.insert(2);
    invited.tickChessPresence();
    clockNow += 46;
    invited.tickChessPresence();
    assert(invited.transport.closes.empty() && invited.mPendingTunnels.count(2));

    p3RetroChess responder;
    QVariantMap request;
    request["type"] = "chess_presence_request";
    request["version"] = 1;
    request["nonce"] = "request-token";
    responder.handleChessPresence(2, 102, request);
    assert(responder.transport.sends.back().second["status"] == "available");
    assert(responder.mChessContacts.empty()); // Probes do not add strangers.
    responder.mChessBusy = true;
    responder.handleChessPresence(2, 102, request);
    assert(responder.transport.sends.back().second["status"] == "busy");
    responder.mGameSessions["2"] = {};
    responder.handleChessPresence(2, 102, request);
    assert(responder.transport.sends.back().second["status"] == "playing");
    responder.enabled = false;
    responder.handleChessPresence(2, 102, request);
    responder.mChessContacts[3];
    responder.tickChessPresence();
    assert(responder.transport.sends.size() == 3 && responder.mPendingTunnels.empty());

    p3RetroChess contacts;
    assert(!contacts.addChessContact(1) && !contacts.addChessContact(0));
    assert(contacts.addChessContact(2) && contacts.addChessContact(2));
    assert(contacts.mChessContacts.size() == 1 && contacts.saves == 1);
    contacts.mChessContacts[2].lastSeen = clockNow;
    contacts.mChessContacts[2].status = "available";
    contacts.mChessIdentitiesConfigured = true;
    contacts.mChessIdentities.insert(1);
    contacts.mChessIdentities.insert(9);
    contacts.mPreferredChessIdentity = 9; // Preferred identity need not sort first.
    contacts.mChessBusy = true;
    std::list<RsItem *> saved;
    bool cleanup = false;
    assert(contacts.saveList(cleanup, saved) && cleanup);
    p3RetroChess restored;
    assert(restored.loadList(saved) && saved.empty());
    assert(restored.mChessContacts.size() == 1);
    assert(restored.mChessContacts.at(2).lastSeen == clockNow);
    assert(restored.mChessContacts.at(2).status == "unknown");
    assert(restored.mChessIdentitiesConfigured && restored.mChessIdentities.count(1));
    assert(restored.mChessIdentities.size() == 2 && restored.mChessIdentities.count(9));
    assert(restored.mPreferredChessIdentity == Id(9) && restored.mChessBusy);
    restored.removeChessContact(2);
    restored.mChessIdentities.clear();
    restored.saveList(cleanup, saved);
    p3RetroChess empty;
    empty.loadList(saved);
    assert(empty.mChessContacts.empty());
    assert(empty.mChessIdentitiesConfigured && empty.mChessIdentities.empty());

    p3RetroChess visitor;
    assert(visitor.acceptDataFromPeer(2, 102, false));
    visitor.receiveData(102, static_cast<unsigned char *>(std::malloc(1)), 1);
    assert(visitor.delivered == 1 && freedPackets == 1);
    assert(visitor.mTunnelToGxsIdMap.count(102));
    assert(visitor.mOwnGxsIdByPeer.empty()); // Presence-only visitors do not accumulate identity entries.

    p3RetroChess disabled;
    disabled.enabled = false;
    assert(disabled.acceptDataFromPeer(2, 102, false)); // always true: GxsTunnel would leak the buffer
    disabled.receiveData(102, static_cast<unsigned char *>(std::malloc(1)), 1);
    assert(disabled.delivered == 0 && freedPackets == 2);
    assert(!disabled.mTunnelToGxsIdMap.count(102));
    disabled.mGameSessions["2"] = {};
    assert(disabled.acceptDataFromPeer(2, 102, false));
    disabled.receiveData(102, static_cast<unsigned char *>(std::malloc(1)), 1);
    assert(disabled.delivered == 1 && freedPackets == 3); // Existing game may finish.
    std::cout << "Chess presence regression checks passed\n";
}
