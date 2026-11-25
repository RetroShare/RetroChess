/*******************************************************************************
 * services/BackgammonService.h                                                *
 *                                                                             *
 * Copyright (C) 2026 RetroShare Team <retroshare.project@gmail.com>           *
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

#pragma once

#include "core/BgRules.h"
#include "gui/BackgammonConstants.h"

#include <retroshare/rsgxstunnel.h>

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>

#include <atomic>
#include <map>
#include <vector>

#include "gui/BackgammonGameWindow.h"

// Talks to opponents over secured GXS tunnels (no friendship needed, only
// the opponent's identity) and drives one BackgammonGameWindow per match.
//
// Packet: "RBG2" | match token (16 bytes) | op (1 byte) | JSON object
//
//   I  invite   {"v":2,"variant":"classic"|"nardy","length":n,"commit":hex}
//   O  offered  {} the invitation arrived and is shown (inviter knows the
//               opponent is there and has the plugin)
//   A  accept   {"nonce":hex}
//   S  start    {"nonce":hex}
//   D  dice     {"g":game,"p":ply}
//   T  turn     {"g":game,"p":ply,"moves":"24/18 13/11"}
//   P  state    {"g":game,"p":ply} after a reconnection, answered by Q
//   Q  state    {"g":game,"p":ply}
//   C  double   {"g":game,"p":ply} offer the cube before rolling
//   K  take     {"g":game,"p":ply}
//   N  drop     {"g":game,"p":ply}
//   R  resign   {}
//   X  leave    {} (decline, cancel, window closed)
//
// Fair dice: the inviter commits to a random nonce A (commit = SHA-256(A))
// in the invitation, the opponent answers with its own nonce B, then the
// inviter reveals A. Both peers seed their dice from A and B, so neither
// side can choose the rolls and no roll ever travels over the network.
class BackgammonService : public QObject, public RsGxsTunnelService::RsGxsTunnelClientService
{
    Q_OBJECT

public:
    static constexpr uint32_t ServiceId = 0x42474d4e; // "BGMN"

    static BackgammonService &instance();

    // RsGxsTunnelClientService (called from RetroShare threads)
    void connectToGxsTunnelService(RsGxsTunnelService *service) override;
    bool acceptDataFromPeer(const RsGxsId &, const RsGxsTunnelId &, bool) override { return true; }
    void notifyTunnelStatus(const RsGxsTunnelId &id, uint32_t status) override;
    void receiveData(const RsGxsTunnelId &id, unsigned char *data, uint32_t size) override;

    bool isConnected() const { return m_net != nullptr; }
    // Stop handling tunnel callbacks: the plugin is being unloaded.
    void shutdown();

    bool invite(const RsGxsId &peer, const RsGxsId &local, bg::Variant type, int length,
                QString *error = nullptr);
    QList<BackgammonGameWindow *> games() const;
    // Invitations received and not answered yet.
    int pendingInvitations() const;

    // For the tunnel traffic viewer: the tunnels of this plugin's matches,
    // with RetroShare's statistics and this plugin's own packet counts.
    struct TunnelTraffic {
        RsGxsTunnelService::GxsTunnelInfo info {};
        QString match;               // what the tunnel is used for now
        quint32 packetsSent = 0;     // Backgammon packets only
        quint32 packetsReceived = 0;
    };
    bool tunnelTraffic(std::vector<TunnelTraffic> &tunnels) const;

    bool contactsOnly() const { return m_contactsOnly; }
    void setContactsOnly(bool contactsOnly) { m_contactsOnly = contactsOnly; }

signals:
    // Delivered from the event loop, at most once per round of changes.
    void gamesChanged();
    void invitationReceived(BackgammonGameWindow *window);
    // A match started against this identity (for "My players").
    void playerMet(const RsGxsId &peer);

private:
    struct Session {
        QPointer<BackgammonGameWindow> window;
        QByteArray token;
        bool inviter = false;
        QByteArray localNonce;
        QByteArray peerNonce;
        QByteArray commit; // opponent's commitment, when we were invited
        bool delivered = false; // the opponent confirmed the invitation ('O')
        QJsonObject lastTurn; // last turn sent, resent if the opponent missed it
        QJsonObject offer;    // last double we offered
        char answerOp = 0;    // our answer to the opponent's last double
        QJsonObject answer;
    };

    BackgammonService();

    void handleStatus(const RsGxsTunnelId &id, uint32_t status);
    void handlePacket(const RsGxsTunnelId &id, const QByteArray &packet);
    void notifyGamesChanged();
    void reconnect(const RsGxsTunnelId &id);
    void sendState(const RsGxsTunnelId &id, char op);
    void resendIfBehind(Session &session, const RsGxsTunnelId &id, const QJsonObject &state);
    void handleInvite(const RsGxsTunnelId &id, const QByteArray &token, const QJsonObject &body);

    bool send(const RsGxsTunnelId &id, const QByteArray &token, char op,
              const QJsonObject &body = QJsonObject());
    BackgammonGameWindow *makeWindow(const RsGxsTunnelId &id, const RsGxsId &peer,
                                     const RsGxsId &local, bg::Variant type, int length);
    void sendInvite(const RsGxsTunnelId &id);
    void watchInvitation(const RsGxsTunnelId &id);
    void accept(const RsGxsTunnelId &id);
    void start(const RsGxsTunnelId &id);
    void retire(const RsGxsTunnelId &id);
    void releaseWhenFlushed(const RsGxsTunnelId &id, const QByteArray &token, int retries);
    bool allowedPeer(const RsGxsTunnelService::GxsTunnelInfo &info) const;

    static QByteArray randomNonce();
    static QByteArray diceSeed(const QByteArray &inviterNonce, const QByteArray &inviteeNonce);
    static bool inviterIsWhite(const QByteArray &seed);

    RsGxsTunnelService *m_net = nullptr;
    std::atomic<bool> m_stopped { false };
    bool m_contactsOnly = false;
    bool m_gamesChangedPending = false;
    std::map<RsGxsTunnelId, Session> m_sessions;
    std::map<RsGxsTunnelId, QByteArray> m_idle; // finished matches, tunnel kept for a rematch
    struct PacketCount { quint32 sent = 0; quint32 received = 0; };
    std::map<RsGxsTunnelId, PacketCount> m_packets; // per tunnel, until it is closed
};
