// Leaderboard sync regression test: two peers with a shared game history
// sync over hours. Checks that the history is sent once and afterwards only
// new receipts travel, and that a restart falls back to one full sync.
// The runner (leaderboard-sync.py) inserts the production methods.
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QString>
#include <QUuid>
#include <algorithm>
#include <cassert>
#include <deque>
#include <iostream>
#include <map>
#include <vector>

struct RsGxsId {
    std::string v;
    RsGxsId() {}
    explicit RsGxsId(const std::string &s) : v(s) {}
    bool isNull() const { return v.empty(); }
    std::string toStdString() const { return v; }
    bool operator<(const RsGxsId &o) const { return v < o.v; }
    bool operator==(const RsGxsId &o) const { return v == o.v; }
    bool operator!=(const RsGxsId &o) const { return v != o.v; }
};
static std::ostream &operator<<(std::ostream &stream, const RsGxsId &id) { return stream << id.v; }
#include "services/RetroChessTunnelDebug.h"
#define LB_RECEIPT(r) "game=" << (r).gameId.toStdString() << " result=" << (r).result.toStdString()
static qint64 fakeMs = 0;
struct QElapsedTimer { qint64 elapsed() const { return fakeMs; } };
#define emit

// CONSTANTS

struct RetroChessLeaderboard;
struct AvailablePeer { bool gxs = true; QString endpointId, status = "available"; };
struct Network {
    struct Packet { RsGxsId from, to; QByteArray data; };
    std::map<RsGxsId, RetroChessLeaderboard *> nodes;
    std::deque<Packet> queue;
    RsGxsId current;
    unsigned int receiptsSent = 0, packetsSent = 0;
    bool sendLeaderboardDataGxs(const RsGxsId &to, const QByteArray &data);
    void broadcastLeaderboardDataGxs(const QByteArray &data) {
        for (auto &n : nodes) if (n.first != current) sendLeaderboardDataGxs(n.first, data);
    }
    std::vector<RsGxsId> activeGxsTunnels() {
        std::vector<RsGxsId> r;
        for (auto &n : nodes) if (n.first != current) r.push_back(n.first);
        return r;
    }
    std::vector<AvailablePeer> availableChessPeers() {
        std::vector<AvailablePeer> r;
        for (auto &n : nodes) if (n.first != current) { AvailablePeer p; p.endpointId = QString::fromStdString(n.first.v); r.push_back(p); }
        return r;
    }
    void pump();
};
static Network net;
static Network *rsRetroChess = &net;

struct RetroChessLeaderboard {
    struct Receipt {
        QString gameId, white, black, result, signer;
        qint64 finishedAt = 0;
        quint64 seq = 0;
        QString learnedFrom;
        Receipt() = default;
        Receipt(const QString &g, const QString &w, const QString &b,
                const QString &r, const QString &s, qint64 f)
            : gameId(g), white(w), black(b), result(r), signer(s), finishedAt(f) {}
    };
    RsGxsId self;
    QMap<QString, Receipt> mReceipts, mPending;
    QMap<QString, QSet<QString>> mWitnesses;
    QSet<QString> mGossipedReceipts;
    QElapsedTimer mSyncClock;
    QMap<RsGxsId, qint64> mLastSyncRequest, mLastSyncResponse, mLastFullSyncResponse;
    QString mSyncEpoch;
    quint64 mNextSeq = 0;
    struct SyncCursor { QString epoch; quint64 seq = 0; };
    QMap<RsGxsId, SyncCursor> mSyncCursors;
    explicit RetroChessLeaderboard(const std::string &id) : self(id) {
        static int n = 0;
        mSyncEpoch = QString::fromStdString("epoch-" + std::to_string(++n));
    }
    void save() const {}
    void recompute() {}
    void changed() {}
    void scheduleCommit(bool = true) {}
    bool consumeReceipt(const Receipt &r, const RsGxsId &sender);
    void storeReceipt(const QString &key, Receipt r, const RsGxsId &from = RsGxsId());
    void broadcastReceipt(const Receipt &r, const RsGxsId &excludePeer = RsGxsId());
    void sendSyncToPeer(const RsGxsId &peerId, const QJsonObject &request);
    void sendSyncRequest(const RsGxsId &peerId);
    void handleTunnelData(const RsGxsId &sender, const QByteArray &data);
    static QString canonicalKey(const Receipt &r);
    static bool validResult(const QString &r);
    void synchronizeTunnels();
};

bool Network::sendLeaderboardDataGxs(const RsGxsId &to, const QByteArray &data) {
    ++packetsSent;
    const QJsonObject o = QJsonDocument::fromJson(data).object();
    if (o.value("type").toString() == "leaderboard_sync") receiptsSent += o.value("receipts").toArray().size();
    if (o.value("type").toString() == "leaderboard_receipt") ++receiptsSent;
    queue.push_back({current, to, data});
    return true;
}
void Network::pump() {
    while (!queue.empty()) {
        Packet p = queue.front(); queue.pop_front();
        auto it = nodes.find(p.to);
        if (it == nodes.end()) continue;
        const RsGxsId saved = current; current = p.to;
        it->second->handleTunnelData(p.from, p.data);
        current = saved;
    }
}

// PRODUCTION_METHODS

static void tickAll()
{
    for (auto &n : net.nodes) { net.current = n.first; n.second->synchronizeTunnels(); }
    net.pump();
}

int main()
{
    RetroChessLeaderboard a("A"), b("B");
    // 200 games between A and B, both receipts of each game known to both
    // sides (a long shared history), plus 30 games only A knows about.
    for (int g = 0; g < 200; ++g) {
        for (RetroChessLeaderboard *n : {&a, &b}) {
            RetroChessLeaderboard::Receipt w(QString::fromStdString("g" + std::to_string(g)), "A", "B", "1-0", "A", 1000 + g);
            RetroChessLeaderboard::Receipt k(QString::fromStdString("g" + std::to_string(g)), "A", "B", "1-0", "B", 1000 + g);
            n->consumeReceipt(w, RsGxsId("A"));
            n->consumeReceipt(k, RsGxsId("B"));
        }
    }
    for (int g = 200; g < 230; ++g)
        a.consumeReceipt(RetroChessLeaderboard::Receipt(QString::fromStdString("g" + std::to_string(g)), "A", "B", "0-1", "A", 1000 + g), RsGxsId("A"));
    net.nodes[RsGxsId("A")] = &a;
    net.nodes[RsGxsId("B")] = &b;

    // First contact: full history both ways.
    tickAll();
    const unsigned int firstSync = net.receiptsSent;
    std::cerr << "first sync: " << firstSync << " receipts\n";
    // A sends its 230 own receipts, B its 200; nobody echoes what it got from the other.
    assert(firstSync == 230 + 200);
    assert(b.mReceipts.size() == 430);                 // B learned A's 30 extra games
    assert(b.mSyncCursors.value(RsGxsId("A")).seq == a.mNextSeq);

    // 4 hours of 5-minute syncs with nothing new: no receipt is resent.
    for (int i = 0; i < 48; ++i) { fakeMs += 5 * 60 * 1000; tickAll(); }
    std::cerr << "4h idle: " << net.receiptsSent - firstSync << " receipts, "
              << net.packetsSent << " packets total\n";
    assert(net.receiptsSent == firstSync);

    // A new game: exactly the new receipt travels in the next sync.
    a.consumeReceipt(RetroChessLeaderboard::Receipt("g999", "A", "B", "1-0", "A", 9999), RsGxsId("A"));
    fakeMs += 5 * 60 * 1000; tickAll();
    assert(net.receiptsSent == firstSync + 1);
    assert(b.mReceipts.contains("g999|A|B|1-0|A"));

    // A restarts (new epoch, new numbering): B gets the full history once more.
    RetroChessLeaderboard a2("A");
    for (const auto &r : a.mReceipts) a2.consumeReceipt(r, RsGxsId(r.signer.toStdString()));
    net.nodes[RsGxsId("A")] = &a2;
    const unsigned int beforeRestart = net.receiptsSent;
    fakeMs += 5 * 60 * 1000; tickAll();
    const unsigned int restartSync = net.receiptsSent - beforeRestart;
    std::cerr << "after A restart: " << restartSync << " receipts\n";
    assert(restartSync >= 231);                        // A's receipts once more (+ B's, fresh epoch)
    const unsigned int afterRestart = net.receiptsSent;
    for (int i = 0; i < 12; ++i) { fakeMs += 5 * 60 * 1000; tickAll(); }
    assert(net.receiptsSent == afterRestart);

    // An old version (no "since"): full history, but at most every 30 minutes.
    QJsonObject legacy{{"type", "leaderboard_sync_req"}, {"version", 1}};
    unsigned int legacyReceipts = 0;
    for (int i = 0; i < 12; ++i) {                     // 1 hour of requests every 5 min
        fakeMs += 5 * 60 * 1000;
        const unsigned int before = net.receiptsSent;
        net.current = RsGxsId("A");
        a2.handleTunnelData(RsGxsId("B"), QJsonDocument(legacy).toJson(QJsonDocument::Compact));
        net.queue.clear();
        legacyReceipts += net.receiptsSent - before;
    }
    std::cerr << "legacy peer, 1h: " << legacyReceipts << " receipts\n";
    int fromA = 0;
    for (const auto &r : a2.mReceipts) if (r.learnedFrom != QString("B")) ++fromA;
    assert(legacyReceipts == 2u * fromA);              // full history twice in 1h, was 12 times

    std::cerr << "Leaderboard sync regression checks passed\n";
}
