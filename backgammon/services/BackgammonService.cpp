/*******************************************************************************
 * services/BackgammonService.cpp                                              *
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

#include "BackgammonService.h"
#include "BackgammonDebug.h"

#include "gui/BackgammonGameWindow.h"
#include "gui/BackgammonSettings.h"
#include "gui/BackgammonSounds.h"

#include <retroshare/rsidentity.h>
#include <retroshare/rsreputations.h>

#include <QApplication>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QTimer>
#include <QUuid>
#include <QElapsedTimer>

#include <memory>

#include <cstdlib>
#include <cstring>

namespace {
const QByteArray Magic("RBG2");
// Version 2: own rules engine, turns sent as move text ("24/18 13/11").
constexpr int ProtocolVersion = 2;
constexpr int TokenSize = 16;
constexpr int HeaderSize = 4 + TokenSize + 1;
constexpr uint32_t MaxPacketSize = 64 * 1024;
constexpr int NonceSize = 32;
constexpr int InviteTimeoutMs = 120000;
constexpr int AcceptRetryMs = 3000;
// Invitation progress (see watchInvitation()).
constexpr int InviteCheckMs = 2000;
constexpr int ReachTimeoutMs = 90000;      // no tunnel: offline or unreachable (turtle tunnels can take a while)
constexpr int DeliveryTimeoutMs = 30000;   // tunnel up but no 'O': plugin missing
constexpr int MaxPendingInvitations = 10;
// How long a match survives a dropped tunnel before it is abandoned.
constexpr int LinkGraceMs = 180000;
constexpr int ReconnectRetryMs = 10000;
// How long a finished match keeps its tunnel open for a rematch.
constexpr int IdleTunnelMs = 30000;

// First bytes of a match token, to tell matches apart in the logs.
std::string shortToken(const QByteArray &token)
{
    return token.left(4).toHex().toStdString();
}

std::string jsonText(const QJsonObject &body)
{
    return QJsonDocument(body).toJson(QJsonDocument::Compact).toStdString();
}

QString variantName(bg::Variant variant)
{
    return variant == bg::Variant::LongNardy ? QStringLiteral("nardy") : QStringLiteral("classic");
}
}

BackgammonService &BackgammonService::instance()
{
    // Never deleted: GxsTunnel keeps a pointer to it after the plugin stops.
    static BackgammonService *service = new BackgammonService;
    return *service;
}

BackgammonService::BackgammonService()
{
    // Tunnel callbacks arrive on RetroShare threads; handle them in the GUI thread.
    if (qApp)
        moveToThread(qApp->thread());
}

void BackgammonService::connectToGxsTunnelService(RsGxsTunnelService *service)
{
    if (!service || service == m_net)
        return;
    const bool registered = service->registerClientService(ServiceId, this);
    BG_TLOG("register client service 0x" << std::hex << ServiceId << std::dec
            << (registered ? " ok" : " FAILED"));
    if (registered)
        m_net = service;
}

void BackgammonService::shutdown()
{
    m_stopped = true;
}

void BackgammonService::notifyGamesChanged()
{
    // Coalesced and delivered later: listeners rebuild widgets (the games
    // list), which must not happen while one of those widgets is still
    // handling the click that caused the change.
    if (m_gamesChangedPending)
        return;
    m_gamesChangedPending = true;
    QTimer::singleShot(0, this, [this] {
        m_gamesChangedPending = false;
        emit gamesChanged();
    });
}

/* ------------------------------------------------------------------------ */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------ */

QByteArray BackgammonService::randomNonce()
{
    QByteArray nonce(NonceSize, Qt::Uninitialized);
    quint32 words[NonceSize / 4];
    QRandomGenerator::system()->fillRange(words);
    memcpy(nonce.data(), words, NonceSize);
    return nonce;
}

QByteArray BackgammonService::diceSeed(const QByteArray &inviterNonce, const QByteArray &inviteeNonce)
{
    return QCryptographicHash::hash("RetroBackgammon/seed/" + inviterNonce + inviteeNonce,
                                    QCryptographicHash::Sha256);
}

bool BackgammonService::inviterIsWhite(const QByteArray &seed)
{
    const QByteArray digest = QCryptographicHash::hash(seed + "/colour", QCryptographicHash::Sha256);
    return (static_cast<unsigned char>(digest.at(0)) & 1) == 0;
}

bool BackgammonService::allowedPeer(const RsGxsTunnelService::GxsTunnelInfo &info) const
{
    const RsGxsId &peer = info.destination_gxs_id;
    const RsGxsId &local = info.source_gxs_id;
    if (peer.isNull() || local.isNull() || peer == local)
        return false;
    if (!rsIdentity || !rsIdentity->isOwnId(local) || rsIdentity->isOwnId(peer))
        return false;
    if (rsReputations && rsReputations->isIdentityBanned(peer))
        return false;
    if (m_contactsOnly && !rsIdentity->isARegularContact(peer))
        return false;
    return true;
}

bool BackgammonService::send(const RsGxsTunnelId &id, const QByteArray &token, char op,
                             const QJsonObject &body)
{
    if (!m_net || m_stopped)
        return false;
    QByteArray packet = Magic + token;
    packet.append(op);
    packet.append(QJsonDocument(body).toJson(QJsonDocument::Compact));
    const bool ok = m_net->sendData(id, ServiceId, reinterpret_cast<const uint8_t *>(packet.constData()),
                                    static_cast<uint32_t>(packet.size()));
    if (ok)
        ++m_packets[id].sent;
    BG_TLOG("send tunnel=" << id << " op=" << op << "(" << BackgammonDebug::opName(op) << ") "
            << packet.size() << " bytes match=" << shortToken(token) << (ok ? "" : " FAILED"));
    BG_GAMELOG("sent " << BackgammonDebug::opName(op) << " match=" << shortToken(token) << " "
               << jsonText(body) << (ok ? "" : " (could not be sent)"));
    return ok;
}

int BackgammonService::pendingInvitations() const
{
    int count = 0;
    for (const auto &entry : m_sessions)
        if (entry.second.window && entry.second.window->phase() == BackgammonGameWindow::Offered)
            ++count;
    return count;
}

QList<BackgammonGameWindow *> BackgammonService::games() const
{
    QList<BackgammonGameWindow *> list;
    for (const auto &entry : m_sessions)
        if (entry.second.window)
            list.append(entry.second.window);
    return list;
}

/* ------------------------------------------------------------------------ */
/* Tunnel callbacks                                                          */
/* ------------------------------------------------------------------------ */

void BackgammonService::notifyTunnelStatus(const RsGxsTunnelId &id, uint32_t status)
{
    if (m_stopped)
        return;
    BG_TLOG("status tunnel=" << id << " " << BackgammonDebug::statusName(status));
    QMetaObject::invokeMethod(this, [this, id, status] { handleStatus(id, status); },
                              Qt::QueuedConnection);
}

void BackgammonService::receiveData(const RsGxsTunnelId &id, unsigned char *data, uint32_t size)
{
    // The core hands the buffer over to us: always free it.
    QByteArray packet;
    if (data && size >= HeaderSize && size <= MaxPacketSize)
        packet = QByteArray(reinterpret_cast<const char *>(data), static_cast<int>(size));
    std::free(data);

    if (packet.isEmpty()) {
        BG_TLOG("receive tunnel=" << id << " " << size << " bytes: ignored (size out of range)");
        return;
    }
    if (m_stopped)
        return;
    QMetaObject::invokeMethod(this, [this, id, packet] { handlePacket(id, packet); },
                              Qt::QueuedConnection);
}

void BackgammonService::handleStatus(const RsGxsTunnelId &id, uint32_t status)
{
    if (m_stopped)
        return;
    auto it = m_sessions.find(id);
    if (it == m_sessions.end() || !it->second.window)
        return;
    BackgammonGameWindow *window = it->second.window;

    switch (status) {
    case RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_CAN_TALK:
        if (it->second.inviter && window->phase() == BackgammonGameWindow::Connecting)
            sendInvite(id);
        if (window->linkDown()) {
            window->setLinkDown(false);
            sendState(id, 'P'); // catch up on anything lost meanwhile
        }
        break;

    case RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_TUNNEL_DN:
    case RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_REMOTELY_CLOSED:
        // The tunnel is shared with every other service using GXS tunnels
        // and one of them may have closed it. That says nothing about the
        // opponent having left: an 'R' or 'X' packet does. Dig it again.
        reconnect(id);
        break;
    }
}

void BackgammonService::reconnect(const RsGxsTunnelId &id)
{
    auto it = m_sessions.find(id);
    if (it == m_sessions.end() || !it->second.window)
        return;
    BackgammonGameWindow *window = it->second.window;
    if (window->phase() == BackgammonGameWindow::Ended || window->linkDown())
        return;
    window->setLinkDown(true);
    BG_TLOG("tunnel=" << id << " lost during match " << shortToken(it->second.token)
            << ": asking for it every " << ReconnectRetryMs / 1000 << " s");

    const QByteArray token = it->second.token;
    const RsGxsId peer = window->peerId(), local = window->localId();
    QPointer<BackgammonGameWindow> guard(window);
    auto *timer = new QTimer(window);
    auto attempt = [this, id, token, peer, local, guard, timer] {
        auto session = m_sessions.find(id);
        if (!guard || !guard->linkDown() || guard->phase() == BackgammonGameWindow::Ended
                || session == m_sessions.end() || session->second.token != token) {
            timer->deleteLater();
            return;
        }
        // Asking again also registers this service on the tunnel when
        // another service (re)created it.
        RsGxsTunnelId again;
        uint32_t code = 0;
        const bool requested = m_net && m_net->requestSecuredTunnel(peer, local, again, ServiceId, code);
        BG_TLOG("reconnect tunnel=" << id << (requested ? ": requested" : ": request FAILED, error ") 
                << (requested ? std::string() : std::to_string(code)));
        if (requested)
            sendState(id, 'P');
    };
    connect(timer, &QTimer::timeout, window, attempt);
    timer->start(ReconnectRetryMs);
    QTimer::singleShot(0, window, attempt);

    QTimer::singleShot(LinkGraceMs, window, [guard, id] {
        if (guard && guard->linkDown()) {
            BG_TLOG("tunnel=" << id << " did not come back within " << LinkGraceMs / 1000 << " s: match ended");
            guard->finish(BackgammonGameWindow::tr("Connection to your opponent was lost"));
        }
    });
}

void BackgammonService::sendState(const RsGxsTunnelId &id, char op)
{
    auto it = m_sessions.find(id);
    if (it == m_sessions.end() || !it->second.window)
        return;
    BackgammonGameWindow *window = it->second.window;
    if (window->phase() != BackgammonGameWindow::Playing || window->gameIndex() < 0)
        return;
    QJsonObject body;
    body.insert("g", window->gameIndex());
    body.insert("p", window->ply());
    send(id, it->second.token, op, body);
}

void BackgammonService::resendIfBehind(Session &session, const RsGxsTunnelId &id, const QJsonObject &state)
{
    // The opponent is still waiting for the turn we sent last: it was lost
    // while the tunnel was down.
    if (session.lastTurn.isEmpty())
        return;
    if (state.value("g").toInt(-1) == session.lastTurn.value("g").toInt(-2)
            && state.value("p").toInt(-1) == session.lastTurn.value("p").toInt(-2))
        send(id, session.token, 'T', session.lastTurn);
}

void BackgammonService::handlePacket(const RsGxsTunnelId &id, const QByteArray &packet)
{
    if (m_stopped)
        return;
    if (!packet.startsWith(Magic)) {
        BG_TLOG("receive tunnel=" << id << " " << packet.size() << " bytes: not a Backgammon packet, ignored");
        return;
    }

    const QByteArray token = packet.mid(4, TokenSize);
    const char op = packet.at(4 + TokenSize);
    const QJsonDocument document = QJsonDocument::fromJson(packet.mid(HeaderSize));
    const QJsonObject body = document.isObject() ? document.object() : QJsonObject();
    ++m_packets[id].received;
    BG_TLOG("receive tunnel=" << id << " op=" << op << "(" << BackgammonDebug::opName(op) << ") "
            << packet.size() << " bytes match=" << shortToken(token));

    auto it = m_sessions.find(id);
    if (it == m_sessions.end() || it->second.token != token) {
        // Only an invitation can open a new match.
        if (op == 'I')
            handleInvite(id, token, body);
        else
            BG_GAMELOG("received " << BackgammonDebug::opName(op) << " for unknown match "
                       << shortToken(token) << ": ignored");
        return;
    }
    BG_GAMELOG("received " << BackgammonDebug::opName(op) << " match=" << shortToken(token) << " "
               << jsonText(body));

    Session &session = it->second;
    BackgammonGameWindow *window = session.window;
    if (!window)
        return;

    // Any packet of this match proves the tunnel works again.
    window->setLinkDown(false);

    switch (op) {
    case 'R':
    case 'X':
        if (window->phase() != BackgammonGameWindow::Ended)
            BackgammonSounds::play(op == 'R' ? BackgammonSounds::Resign : BackgammonSounds::Declined);
        if (window->phase() == BackgammonGameWindow::Playing)
            window->finish(op == 'R' ? BackgammonGameWindow::tr("Your opponent resigned — you won the match!")
                                     : BackgammonGameWindow::tr("Your opponent left — you won the match!"),
                           BackgammonResultDialog::Won);
        else if (window->phase() == BackgammonGameWindow::Waiting)
            window->finish(BackgammonGameWindow::tr("Invitation declined"));
        else if (window->phase() != BackgammonGameWindow::Ended) {
            const bool hiddenInvitation = window->phase() == BackgammonGameWindow::Offered && !window->isVisible();
            window->finish(BackgammonGameWindow::tr("Your opponent left"));
            // An invitation withdrawn before it was ever opened: nothing to show.
            if (hiddenInvitation)
                window->close();
        }
        break;

    case 'O':
        if (session.inviter && window->phase() == BackgammonGameWindow::Waiting && !session.delivered) {
            session.delivered = true;
            window->setWaitingDetail(BackgammonGameWindow::tr("%1 has your invitation. Waiting for an answer…")
                                         .arg(window->opponentName()));
        }
        break;

    case 'A': {
        if (!session.inviter)
            break;
        const QByteArray nonce = QByteArray::fromHex(body.value("nonce").toString().toLatin1());
        if (nonce.size() != NonceSize)
            break;
        if (window->phase() == BackgammonGameWindow::Waiting) {
            session.peerNonce = nonce;
            start(id);
        } else if (window->phase() == BackgammonGameWindow::Playing && nonce == session.peerNonce) {
            // Our start packet was lost: the opponent is still asking.
            QJsonObject reveal;
            reveal.insert("nonce", QString::fromLatin1(session.localNonce.toHex()));
            send(id, session.token, 'S', reveal);
        }
        break;
    }

    case 'S': {
        if (session.inviter || window->phase() != BackgammonGameWindow::Confirming)
            break;
        const QByteArray nonce = QByteArray::fromHex(body.value("nonce").toString().toLatin1());
        if (QCryptographicHash::hash(nonce, QCryptographicHash::Sha256) != session.commit) {
            send(id, session.token, 'X');
            window->finish(BackgammonGameWindow::tr("Could not verify your opponent's dice seed. The game was not started."));
            break;
        }
        session.peerNonce = nonce;
        const QByteArray seed = diceSeed(session.peerNonce, session.localNonce);
        window->startMatch(seed, !inviterIsWhite(seed));
        emit playerMet(window->peerId());
        notifyGamesChanged();
        break;
    }

    case 'P': // the opponent is back: tell it where we are, resend what it missed
        if (window->phase() == BackgammonGameWindow::Playing) {
            resendIfBehind(session, id, body);
            if (window->waitingForDoubleAnswer())
                send(id, session.token, 'C', session.offer);
            sendState(id, 'Q');
        }
        break;

    case 'Q':
        if (window->phase() == BackgammonGameWindow::Playing) {
            resendIfBehind(session, id, body);
            if (window->waitingForDoubleAnswer())
                send(id, session.token, 'C', session.offer);
        }
        break;

    case 'C':
        // A double we already answered: our answer was lost, send it again.
        if (session.answerOp && body.value("g").toInt(-1) == session.answer.value("g").toInt(-2)
                && body.value("p").toInt(-1) == session.answer.value("p").toInt(-2)) {
            send(id, session.token, session.answerOp, session.answer);
            break;
        }
        window->remoteDouble(body.value("g").toInt(-1), body.value("p").toInt(-1));
        break;

    case 'K':
    case 'N':
        window->remoteDoubleAnswer(op == 'K', body.value("g").toInt(-1), body.value("p").toInt(-1));
        break;

    case 'D':
        window->remoteDice(body.value("g").toInt(-1), body.value("p").toInt(-1));
        break;

    case 'T': {
        if (!body.value("moves").isString())
            break;
        window->remoteTurn(body.value("g").toInt(-1), body.value("p").toInt(-1),
                           body.value("moves").toString());
        break;
    }

    default:
        break;
    }
}

/* ------------------------------------------------------------------------ */
/* Match set-up                                                              */
/* ------------------------------------------------------------------------ */

bool BackgammonService::invite(const RsGxsId &peer, const RsGxsId &local, bg::Variant type,
                               int length, QString *error)
{
    if (!m_net) {
        if (error)
            *error = tr("The tunnel service is not available.");
        return false;
    }

    RsGxsTunnelId id;
    uint32_t code = 0;
    const bool requested = m_net->requestSecuredTunnel(peer, local, id, ServiceId, code);
    BG_TLOG("request tunnel to " << peer << " from " << local << ": "
            << (requested ? "ok tunnel=" : "FAILED error=") << (requested ? id.toStdString() : std::to_string(code)));
    if (!requested) {
        if (error)
            *error = tr("Could not open a tunnel to this identity (error %1).").arg(code);
        return false;
    }

    auto existing = m_sessions.find(id);
    if (existing != m_sessions.end() && existing->second.window) {
        BackgammonGameWindow *old = existing->second.window;
        if (old->phase() != BackgammonGameWindow::Ended) {
            old->raise();
            old->activateWindow();
            return true;
        }
    }
    retire(id); // a finished board makes room for the rematch

    Session &session = m_sessions[id];
    session.token = QUuid::createUuid().toRfc4122();
    session.inviter = true;
    session.localNonce = randomNonce();
    BackgammonGameWindow *window = makeWindow(id, peer, local, type, length);
    session.window = window;
    BG_GAMELOG("invite " << peer << " to " << variantName(type).toStdString() << " to " << length
               << " point(s), match=" << shortToken(session.token));

    // The core queues packets until the tunnel is ready; if it refuses,
    // the invitation goes out when the tunnel reports it can talk.
    sendInvite(id);

    watchInvitation(id);
    window->show();
    notifyGamesChanged();
    return true;
}

void BackgammonService::watchInvitation(const RsGxsTunnelId &id)
{
    // Tells the inviter what is going on instead of a silent wait:
    //   1. the tunnel to the identity comes up (it is online and reachable),
    //   2. its plugin confirms the invitation arrived ('O'),
    //   3. the player answers.
    // Each step has its own time limit and its own message.
    auto it = m_sessions.find(id);
    if (it == m_sessions.end() || !it->second.window)
        return;
    BackgammonGameWindow *window = it->second.window;
    const QByteArray token = it->second.token;
    const QString name = window->opponentName();
    window->setWaitingDetail(BackgammonGameWindow::tr("Looking for %1 on the network…").arg(name));

    QPointer<BackgammonGameWindow> guard(window);
    auto *timer = new QTimer(window);
    auto started = std::make_shared<QElapsedTimer>();
    auto reachedAt = std::make_shared<qint64>(-1);
    started->start();

    connect(timer, &QTimer::timeout, window, [this, id, token, guard, timer, started, reachedAt] {
        auto session = m_sessions.find(id);
        if (!guard || session == m_sessions.end() || session->second.token != token
                || (guard->phase() != BackgammonGameWindow::Connecting
                    && guard->phase() != BackgammonGameWindow::Waiting)) {
            timer->stop();
            timer->deleteLater();
            return;
        }
        const QString name = guard->opponentName();
        const qint64 elapsed = started->elapsed();

        // The tunnel may be shared and already up: ask its state.
        RsGxsTunnelService::GxsTunnelInfo info;
        const bool canTalk = m_net && m_net->getTunnelInfo(id, info)
                             && info.tunnel_status == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_CAN_TALK;
        if (canTalk && *reachedAt < 0) {
            *reachedAt = elapsed;
            if (!session->second.delivered)
                guard->setWaitingDetail(BackgammonGameWindow::tr("%1 is online, sending the invitation…").arg(name));
        }

        QString failure;
        if (*reachedAt < 0 && elapsed > ReachTimeoutMs)
            failure = BackgammonGameWindow::tr("%1 could not be reached. They may be offline, or their RetroShare "
                                               "is not connected to the network yet.").arg(name);
        else if (*reachedAt >= 0 && !session->second.delivered && elapsed - *reachedAt > DeliveryTimeoutMs)
            failure = BackgammonGameWindow::tr("%1 is online but did not receive the invitation. They may not have "
                                               "the Backgammon plugin installed or enabled.").arg(name);
        else if (elapsed > InviteTimeoutMs)
            failure = session->second.delivered
                          ? BackgammonGameWindow::tr("%1 did not answer your invitation.").arg(name)
                          : BackgammonGameWindow::tr("Invitation expired");
        if (!failure.isEmpty()) {
            BG_GAMELOG("invitation " << shortToken(token) << " given up after " << elapsed / 1000 << " s: "
                       << failure.toStdString());
            send(id, token, 'X');
            guard->finish(failure);
            timer->stop();
            timer->deleteLater();
        }
    });
    timer->start(InviteCheckMs);
}

void BackgammonService::sendInvite(const RsGxsTunnelId &id)
{
    auto it = m_sessions.find(id);
    if (it == m_sessions.end() || !it->second.window)
        return;
    Session &session = it->second;
    BackgammonGameWindow *window = session.window;
    if (window->phase() != BackgammonGameWindow::Connecting)
        return;

    QJsonObject body;
    body.insert("v", ProtocolVersion);
    body.insert("variant", variantName(window->variant()));
    body.insert("length", window->matchLength());
    body.insert("commit", QString::fromLatin1(
        QCryptographicHash::hash(session.localNonce, QCryptographicHash::Sha256).toHex()));
    if (send(id, session.token, 'I', body))
        window->setPhase(BackgammonGameWindow::Waiting);
}

void BackgammonService::handleInvite(const RsGxsTunnelId &id, const QByteArray &token,
                                     const QJsonObject &body)
{
    auto last = m_idle.find(id);
    if (last != m_idle.end() && last->second == token) {
        BG_GAMELOG("invitation " << shortToken(token) << " ignored: late copy of a closed match");
        return;
    }

    auto it = m_sessions.find(id);
    BackgammonGameWindow *current = it == m_sessions.end() ? nullptr : it->second.window.data();

    // Crossed invitations: the larger token wins and the other side
    // accepts it, so exactly one match starts.
    const bool crossed = current && it->second.inviter
                         && current->phase() == BackgammonGameWindow::Waiting;
    if (crossed && token < it->second.token) {
        BG_GAMELOG("crossed invitations: ours wins, " << shortToken(token) << " ignored");
        return;
    }

    RsGxsTunnelService::GxsTunnelInfo info;
    if (!m_net || !m_net->getTunnelInfo(id, info)) {
        BG_GAMELOG("invitation " << shortToken(token) << " ignored: no tunnel information");
        return;
    }
    if (!allowedPeer(info)) {
        BG_GAMELOG("invitation from " << info.destination_gxs_id << " refused: own, banned or unknown identity"
                   << (m_contactsOnly ? " (only contacts allowed)" : ""));
        return;
    }

    const bg::Variant type = body.value("variant").toString() == QLatin1String("nardy")
                                 ? bg::Variant::LongNardy : bg::Variant::Backgammon;
    const int length = body.value("length").toInt(1);
    const QByteArray commit = QByteArray::fromHex(body.value("commit").toString().toLatin1());
    if (body.value("v").toInt() != ProtocolVersion || length < MIN_NUM_GAMES || length > MAX_NUM_GAMES
            || commit.size() != 32) {
        BG_GAMELOG("invitation from " << info.destination_gxs_id << " refused: protocol version "
                   << body.value("v").toInt() << " (want " << ProtocolVersion << "), length " << length
                   << ", commitment " << commit.size() << " bytes");
        return;
    }

    // Too many invitations waiting already: ignore new ones (flooding).
    if (!crossed && pendingInvitations() >= MaxPendingInvitations) {
        BG_GAMELOG("invitation from " << info.destination_gxs_id << " refused: "
                   << MaxPendingInvitations << " invitations already waiting");
        return;
    }

    // A new invitation replaces an older match on this tunnel: a finished
    // one (rematch) or one the opponent has abandoned.
    retire(id);

    Session &session = m_sessions[id];
    session.token = token;
    session.inviter = false;
    session.commit = commit;
    BackgammonGameWindow *window = makeWindow(id, info.destination_gxs_id, info.source_gxs_id,
                                              type, length);
    session.window = window;
    window->setPhase(BackgammonGameWindow::Offered);
    BG_GAMELOG("invitation from " << info.destination_gxs_id << " to " << variantName(type).toStdString()
               << " to " << length << " point(s), match=" << shortToken(token) << (crossed ? " (crossed)" : ""));
    // Tell the inviter the invitation arrived (and that this plugin exists).
    send(id, token, 'O');
    // The window opens now, or later from "Your games" or the toaster.
    if (crossed || BackgammonSettings::openInvitationWindow())
        window->show();
    notifyGamesChanged();

    // The inviter gives up after InviteTimeoutMs; drop an invitation that was
    // never answered a little later, so it does not wait forever.
    QPointer<BackgammonGameWindow> guard(window);
    QTimer::singleShot(InviteTimeoutMs + 30000, window, [guard] {
        if (guard && guard->phase() == BackgammonGameWindow::Offered)
            guard->close();   // sends the decline
    });

    if (crossed)
        accept(id); // both players invited each other: start right away
    else {
        BackgammonSounds::play(BackgammonSounds::Invitation);
        emit invitationReceived(window);
    }
}

void BackgammonService::accept(const RsGxsTunnelId &id)
{
    auto it = m_sessions.find(id);
    if (it == m_sessions.end() || !it->second.window)
        return;
    Session &session = it->second;
    BackgammonGameWindow *window = session.window;
    if (window->phase() != BackgammonGameWindow::Offered)
        return;

    session.localNonce = randomNonce();
    QJsonObject body;
    body.insert("nonce", QString::fromLatin1(session.localNonce.toHex()));
    if (!send(id, session.token, 'A', body)) {
        window->finish(BackgammonGameWindow::tr("Could not send your answer"));
        return;
    }
    window->setPhase(BackgammonGameWindow::Confirming);

    // Repeat the answer until the inviter starts the game.
    QPointer<BackgammonGameWindow> guard(window);
    const QByteArray token = session.token;
    auto *retry = new QTimer(window);
    connect(retry, &QTimer::timeout, window, [this, id, token, body, guard, retry] {
        if (!guard || guard->phase() != BackgammonGameWindow::Confirming) {
            retry->stop();
            return;
        }
        send(id, token, 'A', body);
    });
    retry->start(AcceptRetryMs);

    QTimer::singleShot(InviteTimeoutMs, window, [this, id, token, guard] {
        if (guard && guard->phase() == BackgammonGameWindow::Confirming) {
            send(id, token, 'X');
            guard->finish(BackgammonGameWindow::tr("The game did not start in time"));
        }
    });
}

void BackgammonService::start(const RsGxsTunnelId &id)
{
    Session &session = m_sessions[id];
    QJsonObject reveal;
    reveal.insert("nonce", QString::fromLatin1(session.localNonce.toHex()));
    if (!send(id, session.token, 'S', reveal)) {
        session.window->finish(BackgammonGameWindow::tr("Could not start the game"));
        return;
    }
    const QByteArray seed = diceSeed(session.localNonce, session.peerNonce);
    session.window->startMatch(seed, inviterIsWhite(seed));
    emit playerMet(session.window->peerId());
    notifyGamesChanged();
}

BackgammonGameWindow *BackgammonService::makeWindow(const RsGxsTunnelId &id, const RsGxsId &peer,
                                                    const RsGxsId &local, bg::Variant type, int length)
{
    auto *window = new BackgammonGameWindow(peer, local, type, length);
    const QByteArray token = m_sessions[id].token;

    auto stillCurrent = [this, id, window] {
        auto it = m_sessions.find(id);
        return it != m_sessions.end() && it->second.window == window;
    };

    connect(window, &BackgammonGameWindow::acceptRequested, this, [this, id, stillCurrent] {
        if (stillCurrent())
            accept(id);
    });
    connect(window, &BackgammonGameWindow::resignRequested, this, [this, id, token, stillCurrent] {
        if (stillCurrent())
            send(id, token, 'R');
    });
    connect(window, &BackgammonGameWindow::diceToSend, this,
            [this, id, token, stillCurrent](int game, int ply) {
        if (!stillCurrent())
            return;
        QJsonObject body;
        body.insert("g", game);
        body.insert("p", ply);
        send(id, token, 'D', body);
    });
    connect(window, &BackgammonGameWindow::turnToSend, this,
            [this, id, token, stillCurrent](int game, int ply, const QString &moves) {
        if (!stillCurrent())
            return;
        QJsonObject body;
        body.insert("g", game);
        body.insert("p", ply);
        body.insert("moves", moves);
        m_sessions[id].lastTurn = body;
        // A turn that cannot be sent now is sent again once the tunnel is back.
        if (!send(id, token, 'T', body))
            reconnect(id);
    });
    connect(window, &BackgammonGameWindow::doubleOffered, this,
            [this, id, token, stillCurrent](int game, int ply) {
        if (!stillCurrent())
            return;
        QJsonObject body;
        body.insert("g", game);
        body.insert("p", ply);
        m_sessions[id].offer = body;
        if (!send(id, token, 'C', body))
            reconnect(id);
    });
    connect(window, &BackgammonGameWindow::doubleAnswered, this,
            [this, id, token, stillCurrent](bool take, int game, int ply) {
        if (!stillCurrent())
            return;
        QJsonObject body;
        body.insert("g", game);
        body.insert("p", ply);
        Session &session = m_sessions[id];
        session.answerOp = take ? 'K' : 'N';
        session.answer = body;
        if (!send(id, token, session.answerOp, body))
            reconnect(id);
    });
    connect(window, &BackgammonGameWindow::summaryChanged, this, &BackgammonService::notifyGamesChanged);
    connect(window, &BackgammonGameWindow::closed, this, [this, id, token, window, stillCurrent] {
        if (!stillCurrent())
            return;
        const auto phase = window->phase();
        if (phase == BackgammonGameWindow::Waiting || phase == BackgammonGameWindow::Offered
                || phase == BackgammonGameWindow::Confirming)
            send(id, token, 'X');
        m_sessions.erase(id);
        BG_GAMELOG("match " << shortToken(token) << " window closed, tunnel kept "
                   << IdleTunnelMs / 1000 << " s for a rematch");
        // Keep the tunnel for a quick rematch, then let it go.
        m_idle[id] = token;
        QTimer::singleShot(IdleTunnelMs, this, [this, id, token] { releaseWhenFlushed(id, token, 60); });
        notifyGamesChanged();
    });
    return window;
}

void BackgammonService::retire(const RsGxsTunnelId &id)
{
    auto it = m_sessions.find(id);
    if (it == m_sessions.end())
        return;
    QPointer<BackgammonGameWindow> old = it->second.window;
    m_sessions.erase(it);
    m_idle.erase(id);
    if (old) {
        old->setPhase(BackgammonGameWindow::Ended); // no resign question, nothing sent
        old->close();
    }
}

void BackgammonService::releaseWhenFlushed(const RsGxsTunnelId &id, const QByteArray &token, int retries)
{
    auto it = m_idle.find(id);
    if (it == m_idle.end() || it->second != token || !m_net || m_stopped)
        return;
    RsGxsTunnelService::GxsTunnelInfo info;
    if (!m_net->getTunnelInfo(id, info)) {
        m_idle.erase(it);
        m_packets.erase(id);
        BG_TLOG("tunnel=" << id << " already gone, nothing to close");
        return;
    }
    if (info.pending_data_packets && retries > 0) {
        QTimer::singleShot(1000, this, [this, id, token, retries] { releaseWhenFlushed(id, token, retries - 1); });
        return;
    }
    m_idle.erase(it);
    m_packets.erase(id);
    const bool closed = m_net->closeExistingTunnel(id, ServiceId);
    BG_TLOG("close tunnel=" << id << (closed ? " ok" : " (not ours or already closed)")
            << (info.pending_data_packets ? ", packets still unacknowledged" : ""));
}

/* ------------------------------------------------------------------------ */
/* Tunnel traffic viewer                                                     */
/* ------------------------------------------------------------------------ */

bool BackgammonService::tunnelTraffic(std::vector<TunnelTraffic> &tunnels) const
{
    tunnels.clear();
    if (!m_net)
        return false;
    std::map<RsGxsTunnelId, QString> used;
    for (const auto &entry : m_sessions) {
        const BackgammonGameWindow *window = entry.second.window;
        if (!window)
            continue;
        QString what;
        switch (window->phase()) {
        case BackgammonGameWindow::Connecting: what = tr("Invitation: connecting"); break;
        case BackgammonGameWindow::Waiting:    what = tr("Invitation sent"); break;
        case BackgammonGameWindow::Offered:    what = tr("Invitation received"); break;
        case BackgammonGameWindow::Confirming: what = tr("Starting"); break;
        case BackgammonGameWindow::Playing:
            what = window->linkDown() ? tr("Playing, reconnecting") : tr("Playing, game %1").arg(window->gameIndex() + 1);
            break;
        case BackgammonGameWindow::Ended:      what = tr("Match over"); break;
        }
        used[entry.first] = what;
    }
    for (const auto &entry : m_idle)
        used.emplace(entry.first, tr("Idle (kept for a rematch)"));

    for (const auto &entry : used) {
        TunnelTraffic tunnel;
        if (!m_net->getTunnelInfo(entry.first, tunnel.info))
            continue;
        tunnel.info.tunnel_id = entry.first;   // not filled in by getTunnelInfo()
        tunnel.match = entry.second;
        const auto count = m_packets.find(entry.first);
        if (count != m_packets.end()) {
            tunnel.packetsSent = count->second.sent;
            tunnel.packetsReceived = count->second.received;
        }
        tunnels.push_back(tunnel);
    }
    return true;
}
