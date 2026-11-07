/*******************************************************************************
 * 4connect/FourConnect.cpp                                                    *
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

#include "FourConnect.h"
#include "FourBoard.h"
#include "../gui/RetroChessSettings.h"
#include "../interface/rsRetroChess.h"
#include "gui/SoundManager.h"

#include "gui/gxs/GxsIdDetails.h"
#include <retroshare/rsidentity.h>
#include <retroshare/rsreputations.h>
#include <QPainter>
#include <QApplication>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMediaPlayer>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioOutput>
#endif
#include <QPushButton>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <map>

namespace {
constexpr uint32_t serviceId = 0x34434e54; // "4CNT"
// How long a match survives a dropped tunnel before it is abandoned.
constexpr int linkGraceMs = 90000;
enum Phase { Connecting, Waiting, Offered, Confirming, Playing, Ended };

class PlayerStrip : public QWidget
{
public:
    QString nickname() const { return name.text(); }
    PlayerStrip(const RsGxsId &id, const QString &role)
    {
        auto *row = new QHBoxLayout(this);
        row->setContentsMargins(0,0,0,0);
        row->addWidget(&avatar);
        row->addWidget(&name,1);
        row->addWidget(new QLabel(role));
        name.setTextFormat(Qt::PlainText);
        name.setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
        name.setText(QString::fromStdString(id.toStdString()));
        avatar.setFixedSize(32,32);
        avatar.setScaledContents(true);
        avatar.setPixmap(GxsIdDetails::makeDefaultIcon(id));
        GxsIdDetails::process(id, [](GxsIdDetailsType type, const RsIdentityDetails &details,
                                     QObject *target, const QVariant &) {
            if (type != GXS_ID_DETAILS_TYPE_DONE) return;
            auto *strip = static_cast<PlayerStrip*>(target);
            if (!details.mNickname.empty())
                strip->name.setText(QString::fromUtf8(details.mNickname.c_str()));
            QPixmap picture;
            if (details.mAvatar.mSize && GxsIdDetails::loadPixmapFromData(
                    details.mAvatar.mData, details.mAvatar.mSize, picture))
                strip->avatar.setPixmap(picture);
        }, this);
    }
private:
    QLabel avatar, name;
};

class Disc : public QPushButton
{
public:
    using QPushButton::QPushButton;
    int player = 0;
    int preview = 0;
    bool columnHovered = false;
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QColor colors[] = {QColor("#302e30"), QColor("#ed5043"), QColor("#f2c519")};
        const bool showPreview = isEnabled() && columnHovered && preview;
        painter.setBrush(showPreview ? colors[preview].darker(165) : colors[player]);
        painter.setPen(QPen(showPreview ? colors[preview] : QColor("#514e50"),
                            showPreview ? 3 : 2));
        painter.drawEllipse(rect().adjusted(2,2,-2,-2));
        if (isEnabled() && hasFocus() && !columnHovered) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(palette().color(QPalette::Highlight),1,Qt::DashLine));
            painter.drawEllipse(rect().adjusted(5,5,-5,-5));
        }
    }
};

class BoardArea : public QWidget
{
public:
    Disc *cells[6][7];
    BoardArea()
    {
        setMinimumSize(7*24,6*24);
        for (auto &row : cells)
            for (auto &cell : row) {
                cell = new Disc(this);
                cell->installEventFilter(this);
            }
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override
    {
        if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
            for (auto &row : cells)
                for (int c = 0; c < 7; ++c)
                    if (row[c] == object) {
                        for (auto &line : cells) {
                            line[c]->columnHovered = event->type() == QEvent::Enter;
                            line[c]->update();
                        }
                        return false;
                    }
        }
        return QWidget::eventFilter(object,event);
    }
    void resizeEvent(QResizeEvent *) override
    {
        const int side = qMin(width()/7,height()/6);
        const int left = (width()-7*side)/2, top = (height()-6*side)/2;
        for (int r = 0; r < 6; ++r)
            for (int c = 0; c < 7; ++c)
                cells[r][c]->setGeometry(left+c*side,top+r*side,side,side);
    }
};

struct Match : QDialog
{
    Match(const RsGxsId &peer, const RsGxsId &local, int color) : me(color)
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(tr("Connect Four"));
        auto *layout = new QGridLayout(this);
        resize(760,590);
        setSizeGripEnabled(true);
        opponent = new PlayerStrip(peer,me == 1 ? tr("Opponent · Yellow") : tr("Opponent · Red"));
        layout->addWidget(opponent,0,0);
        auto *boardColumn = new QVBoxLayout;
        result.setAlignment(Qt::AlignCenter);
        result.setWordWrap(true);
        result.setTextFormat(Qt::PlainText);
        result.hide();
        boardColumn->addWidget(&result);
        boardColumn->addWidget(&area,1);
        layout->addLayout(boardColumn,1,0);
        self = new PlayerStrip(local,me == 1 ? tr("You · Red") : tr("You · Yellow"));
        layout->addWidget(self,2,0);
        layout->addWidget(&status,0,1);
        layout->addWidget(&history,1,1);
        history.setMinimumWidth(130);
        layout->setColumnStretch(0,3);
        layout->setColumnStretch(1,1);
        layout->setRowStretch(1,1);
        auto *actions = new QHBoxLayout;
        actions->addWidget(&accept);
        actions->addWidget(&leave);
        layout->addLayout(actions,2,1);
        connect(&leave,&QPushButton::clicked,this,[this] {
            if (phase != Playing) { close(); return; }
            // Resigning keeps the window open so the result stays visible.
            if (confirmResign(tr("Resign this game?"))) resign(true);
        });
    }
    // Esc, the title bar close button and close() all end up here.
    void reject() override
    {
        if (phase == Playing) {
            if (!confirmResign(tr("Leaving now resigns the game. Leave anyway?"))) return;
            resign(false);
        }
        QDialog::reject();
    }
    bool confirmResign(const QString &question)
    {
        return QMessageBox::question(this,tr("Connect Four"),question,
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No) == QMessageBox::Yes;
    }
    void resign(bool withSound)
    {
        // The game may have ended while the confirmation was open.
        if (phase != Playing) return;
        if (onResign) onResign();
        finish(tr("You resigned"),withSound ? QStringLiteral("qrc:/sound/Defeat.mp3") : QString());
    }
    void refresh()
    {
        accept.setVisible(phase == Offered);
        leave.setText(phase == Offered ? tr("Decline") : phase == Playing ? tr("Resign") : tr("Close"));
        for (int r = 0; r < 6; ++r)
            for (int c = 0; c < 7; ++c) {
                auto *button = area.cells[r][c];
                button->setEnabled(phase == Playing && !linkDown && board.turn() == me && board.landing(c) >= 0);
                button->player = board.at(r,c);
                button->preview = r == board.landing(c) ? me : 0;
                button->update();
                button->setAccessibleName(tr("Row %1, column %2").arg(r+1).arg(c+1));
            }
        if (phase == Ended) return; // finish() owns the final status text
        QString text;
        switch (phase) {
        case Connecting: text = tr("Connecting to opponent…"); break;
        case Waiting:    text = tr("Waiting for your opponent to accept"); break;
        case Offered:    text = tr("Invited you to play Connect Four"); break;
        case Confirming: text = tr("Confirming game start…"); break;
        case Playing:    text = board.turn() == me ? tr("Your turn — choose a column") : tr("Opponent’s turn"); break;
        case Ended:      break;
        }
        if (linkDown) text = tr("Connection lost — reconnecting…");
        status.setText(text);
    }
    void playSound(const QString &source, bool enabled)
    {
        if (!enabled || SoundManager::isMute()) return;
        if (!audio) {
            audio = new QMediaPlayer(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            auto *output = new QAudioOutput(audio);
            output->setVolume(1.0f);
            audio->setAudioOutput(output);
            connect(audio,&QMediaPlayer::errorOccurred,audio,[] { QApplication::beep(); });
#else
            audio->setVolume(100);
            connect(audio,QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error),
                    audio,[] { QApplication::beep(); });
#endif
        }
        audio->stop();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        audio->setSource(QUrl(source));
#else
        audio->setMedia(QUrl(source));
#endif
        audio->play();
    }
    void finish(const QString &message, const QString &sound = QString())
    {
        if (phase == Ended) return;
        phase = Ended;
        linkDown = false;
        status.setText(message);
        result.setText(message);
        result.setStyleSheet(QStringLiteral(
            "QLabel { font-size: 24px; font-weight: bold; padding: 18px;"
            " color: white; background: #34343c; border: 3px solid #babac8; border-radius: 10px; }"));
        result.show();
        setWindowTitle(tr("Connect Four — %1").arg(message));
        refresh();
        if (!sound.isEmpty()) playSound(sound,RetroChessSettings::gameResultSoundEnabled());
    }
    void move(int column)
    {
        const int player = board.turn();
        board.drop(column);
        history.addItem(tr("%1. Col %2, %3").arg(board.moves).arg(column+1)
                        .arg((player == me ? self : opponent)->nickname()));
        history.scrollToBottom();
        if (board.winner)
            finish(player == me ? tr("You won!") : tr("You lost"),
                   player == me ? QStringLiteral("qrc:/sound/victory.mp3")
                                : QStringLiteral("qrc:/sound/Defeat.mp3"));
        else if (board.moves == 42)
            finish(tr("Draw"),QStringLiteral("qrc:/sound/Draw.mp3"));
        else {
            refresh();
            playSound(QStringLiteral("qrc:/sound/Move.mp3"),RetroChessSettings::moveSoundEnabled());
        }
    }
    PlayerStrip *self, *opponent;
    FourBoard board;
    int me;
    Phase phase = Connecting;
    bool linkDown = false; // tunnel reported down, waiting for it to come back
    int linkEpoch = 0;     // invalidates older connection-lost timers
    std::function<void()> onResign;
    QByteArray token;
    QLabel status, result;
    QMediaPlayer *audio = nullptr;
    QListWidget history;
    QPushButton accept{tr("Accept")}, leave;
    BoardArea area;
};

class Client : public RsGxsTunnelService::RsGxsTunnelClientService
{
public:
    RsGxsTunnelService *net = nullptr;
    std::map<RsGxsTunnelId,Match*> games;
    std::map<RsGxsTunnelId,QByteArray> idle;
    void releaseWhenFlushed(const RsGxsTunnelId &id, const QByteArray &token, int retries)
    {
        RsGxsTunnelService::GxsTunnelInfo info;
        auto it = idle.find(id);
        if (it == idle.end() || it->second != token) return;
        if (!net->getTunnelInfo(id,info)) { idle.erase(it); return; }
        if (info.pending_data_packets && retries > 0) {
            QTimer::singleShot(1000,qApp,[=] { releaseWhenFlushed(id,token,retries-1); });
            return;
        }
        idle.erase(it);
        net->closeExistingTunnel(id,serviceId);
    }
    void connectToGxsTunnelService(RsGxsTunnelService *s) override
    {
        if (s && s != net && s->registerClientService(serviceId,this)) net = s;
    }
    // Packets are filtered in receiveData(): the core does not free a buffer
    // refused here, so refusing would leak it.
    bool acceptDataFromPeer(const RsGxsId&,const RsGxsTunnelId&,bool) override { return true; }
    // Same rules as chess: never our own identity, never a banned identity,
    // and only on one of our identities enabled for chess.
    static bool allowedPeer(const RsGxsTunnelService::GxsTunnelInfo &info)
    {
        const RsGxsId &peer = info.destination_gxs_id, &local = info.source_gxs_id;
        if (peer.isNull() || local.isNull() || peer == local) return false;
        if (rsIdentity && rsIdentity->isOwnId(peer)) return false;
        if (rsReputations && rsReputations->isIdentityBanned(peer)) return false;
        if (rsRetroChess) {
            const auto ids = rsRetroChess->chessIdentities();
            if (std::find(ids.begin(),ids.end(),local) == ids.end()) return false;
        }
        return true;
    }
    // Each packet carries a match UUID, operation, and zero-based ply/column.
    bool send(const RsGxsTunnelId &id,const QByteArray &token,char op,int ply=0,int col=0)
    {
        QByteArray packet = token;
        packet.append(op).append(char(ply)).append(char(col));
        return net->sendData(id,serviceId,reinterpret_cast<const uint8_t*>(packet.constData()),packet.size());
    }
    // Drops the current match of a tunnel so a new one can take its place.
    // The old window is closed without sending anything.
    void retire(const RsGxsTunnelId &id)
    {
        auto it = games.find(id);
        if (it == games.end()) return;
        Match *old = it->second;
        games.erase(it);
        old->phase = Ended; // no resign prompt, no 'X'
        old->close();
    }
    Match *make(const RsGxsTunnelId &id,const RsGxsId &peer,const RsGxsId &local,int color,const QByteArray &token)
    {
        auto *m = new Match(peer,local,color);
        m->token = token;
        idle.erase(id);
        games[id] = m;
        m->onResign = [=] {
            send(id,token,'R');
            send(id,token,'X'); // older peers only understand 'X'
        };
        QObject::connect(m,&QDialog::finished,m,[=] {
            auto it = games.find(id);
            if (it == games.end() || it->second != m) return; // replaced by a newer match
            if (m->phase != Ended && m->phase != Connecting) send(id,token,'X');
            games.erase(it);
            // Keep the tunnel available for a quick rematch. A new match
            // invalidates this close timer even if it has since ended too.
            idle[id] = token;
            QTimer::singleShot(30000,qApp,[=] { releaseWhenFlushed(id,token,60); });
        });
        QObject::connect(&m->accept,&QPushButton::clicked,m,[=] {
            if (m->phase != Offered) return;
            if (!send(id,token,'A')) { m->finish(QObject::tr("Could not send acceptance")); return; }
            m->phase = Confirming;
            m->refresh();
            auto *retry = new QTimer(m);
            QObject::connect(retry,&QTimer::timeout,m,[=] {
                if (m->phase != Confirming) { retry->stop(); return; }
                send(id,token,'A');
            });
            retry->start(3000);
            QTimer::singleShot(120000,m,[=] {
                if (m->phase == Confirming) {
                    send(id,token,'X');
                    m->finish(QObject::tr("Game start timed out"));
                }
            });
        });
        for (auto &row : m->area.cells)
            for (int c = 0; c < 7; ++c)
                QObject::connect(row[c],&QPushButton::clicked,m,[=] {
                    if (m->phase != Playing || m->linkDown || m->board.turn() != color
                            || m->board.landing(c) < 0) return;
                    if (send(id,token,'M',m->board.moves,c)) m->move(c);
                    else m->finish(QObject::tr("Could not send move"));
                });
        m->show();
        return m;
    }
    void ready(const RsGxsTunnelId &id,Match *m)
    {
        if (m->phase != Connecting) return;
        if (!send(id,m->token,'I')) { m->finish(QObject::tr("Could not send invitation")); return; }
        m->phase = Waiting;
        m->refresh();
    }
    void invite(const RsGxsId &peer,const RsGxsId &local)
    {
        RsGxsTunnelId id;
        uint32_t error = 0;
        if (!net->requestSecuredTunnel(peer,local,id,serviceId,error)) {
            QMessageBox::warning(nullptr,QObject::tr("Connect Four"),QObject::tr("Could not open tunnel (error %1)").arg(error));
            return;
        }
        auto existing = games.find(id);
        if (existing != games.end()) {
            Match *old = existing->second;
            if (old->phase != Ended) { old->raise(); old->activateWindow(); return; }
            retire(id); // rematch: the finished board makes room for the new one
        }
        auto *m = make(id,peer,local,1,QUuid::createUuid().toRfc4122());
        m->refresh();
        // The core queues packets until the tunnel is ready. This also covers
        // reusing a chess tunnel whose initial status callback already ran.
        ready(id,m);
        QTimer::singleShot(120000,m,[=] {
            if (m->phase == Waiting) {
                send(id,m->token,'X');
                m->finish(QObject::tr("Invitation expired"));
            }
        });
    }
    void offer(const RsGxsTunnelId &id,const QByteArray &token,
               const RsGxsTunnelService::GxsTunnelInfo &info,bool autoAccept)
    {
        auto *m = make(id,info.destination_gxs_id,info.source_gxs_id,2,token);
        m->phase = Offered;
        m->refresh();
        if (autoAccept) {
            // Both players invited each other: start right away.
            m->accept.click();
            return;
        }
        m->playSound(QStringLiteral("qrc:/sound/ping.mp3"),
                     RetroChessSettings::invitationSoundEnabled());
    }
    void notifyTunnelStatus(const RsGxsTunnelId &id,uint32_t state) override
    {
        QTimer::singleShot(0,qApp,[=] {
            auto it = games.find(id);
            if (it == games.end()) return;
            Match *m = it->second;
            if (state == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_CAN_TALK) {
                if (m->linkDown) { m->linkDown = false; m->refresh(); }
                ready(id,m);
            } else if (state == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_TUNNEL_DN) {
                // The core re-digs the tunnel by itself; give it time, then
                // give up instead of waiting for a move forever.
                if (m->phase == Ended || m->linkDown) return;
                m->linkDown = true;
                const int epoch = ++m->linkEpoch;
                m->refresh();
                QTimer::singleShot(linkGraceMs,m,[m,epoch] {
                    if (m->linkDown && m->linkEpoch == epoch)
                        m->finish(QObject::tr("Connection to opponent lost"));
                });
            } else if (state == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_REMOTELY_CLOSED) {
                if (m->phase != Ended) m->finish(QObject::tr("Opponent disconnected"));
            }
        });
    }
    void receiveData(const RsGxsTunnelId &id,unsigned char *data,uint32_t size) override
    {
        QByteArray packet;
        if (size == 19 && data) packet = QByteArray(reinterpret_cast<char*>(data),19);
        std::free(data);
        if (packet.isEmpty()) return;
        QTimer::singleShot(0,qApp,[=] {
            const QByteArray token = packet.left(16);
            const char op = packet.at(16);
            const int ply = packet.at(17), col = packet.at(18);
            auto it = games.find(id);
            Match *m = it == games.end() ? nullptr : it->second;
            if (!m || token != m->token) {
                if (op != 'I') return; // late packet of an earlier match
                auto last = idle.find(id);
                if (last != idle.end() && last->second == token) return; // late copy of a closed match
                // Crossed invitations: the larger token is kept and the other
                // side accepts it, so exactly one match starts.
                const bool crossed = m && m->me == 1 && m->phase == Waiting;
                if (crossed && token < m->token) return;
                RsGxsTunnelService::GxsTunnelInfo info;
                if (!net->getTunnelInfo(id,info) || !allowedPeer(info)) return;
                // A new invitation replaces an older match on this tunnel:
                // a finished one (rematch) or one the opponent has abandoned.
                retire(id);
                offer(id,token,info,crossed);
                return;
            }
            // Any packet of this match proves the tunnel works again.
            if (m->linkDown) { m->linkDown = false; m->refresh(); }
            if (op == 'R' || op == 'X') {
                if (m->phase == Playing)
                    m->finish(op == 'R' ? QObject::tr("Opponent resigned — you won!")
                                        : QObject::tr("Opponent left — you won!"),
                              QStringLiteral("qrc:/sound/victory.mp3"));
                else if (m->phase != Ended)
                    m->finish(QObject::tr("Opponent left the match"));
                return;
            }
            if (op == 'A' && m->me == 1 && (m->phase == Waiting || m->phase == Playing)) {
                send(id,token,'S');
                m->phase = Playing;
                m->refresh();
            }
            // An opening move also confirms acceptance for older peers.
            if (m->phase == Confirming && (op == 'S' ||
                    (op == 'M' && ply == 0 && m->board.landing(col) >= 0))) {
                m->phase = Playing;
                m->refresh();
            }
            if (op == 'M' && m->phase == Playing && m->board.turn() != m->me &&
                ply == m->board.moves && m->board.landing(col) >= 0) m->move(col);
        });
    }
};
Client client;
}
void FourConnect::registerService(RsGxsTunnelService *service) { client.connectToGxsTunnelService(service); }
void FourConnect::addContactAction(QMenu &menu,const RsGxsId &peer,const RsGxsId &local,bool online)
{
    auto *action = menu.addAction(QObject::tr("Invite to Connect Four"));
    action->setEnabled(online && client.net && !local.isNull() && !peer.isNull() && peer != local);
    QObject::connect(action,&QAction::triggered,&menu,[=] { client.invite(peer,local); });
}
