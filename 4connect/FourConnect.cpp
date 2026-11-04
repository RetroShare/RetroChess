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
#include "gui/SoundManager.h"

#include "gui/gxs/GxsIdDetails.h"
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
#include <cstdlib>
#include <map>

namespace {
constexpr uint32_t serviceId = 0x34434e54; // "4CNT"
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
        layout->addWidget(&area,1,0);
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
        connect(&leave,&QPushButton::clicked,this,&QDialog::close);
    }
    void refresh()
    {
        accept.setVisible(phase == Offered);
        leave.setText(phase == Offered ? tr("Decline") : phase == Playing ? tr("Resign") : tr("Close"));
        for (int r = 0; r < 6; ++r)
            for (int c = 0; c < 7; ++c) {
                auto *button = area.cells[r][c];
                button->setEnabled(phase == Playing && board.turn() == me && board.landing(c) >= 0);
                button->player = board.at(r,c);
                button->preview = r == board.landing(c) ? me : 0;
                button->update();
                button->setAccessibleName(tr("Row %1, column %2").arg(r+1).arg(c+1));
            }
        if (phase == Playing)
            status.setText(board.turn() == me ? tr("Your turn — choose a column") : tr("Opponent’s turn"));
    }
    void finish(const QString &message) { phase = Ended; status.setText(message); refresh(); }
    void move(int column)
    {
        const int player = board.turn();
        board.drop(column);
        history.addItem(tr("%1. Col %2, %3").arg(board.moves).arg(column+1)
                        .arg((player == me ? self : opponent)->nickname()));
        history.scrollToBottom();
        if (board.winner) finish(player == me ? tr("You won!") : tr("Opponent won"));
        else if (board.moves == 42) finish(tr("Draw"));
        else refresh();
    }
    PlayerStrip *self, *opponent;
    FourBoard board;
    int me;
    Phase phase = Connecting;
    QByteArray token;
    QLabel status;
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
    bool acceptDataFromPeer(const RsGxsId&,const RsGxsTunnelId&,bool) override { return true; }
    // Each packet carries a match UUID, operation, and zero-based ply/column.
    bool send(const RsGxsTunnelId &id,const QByteArray &token,char op,int ply=0,int col=0)
    {
        QByteArray packet = token;
        packet.append(op).append(char(ply)).append(char(col));
        return net->sendData(id,serviceId,reinterpret_cast<const uint8_t*>(packet.constData()),packet.size());
    }
    Match *make(const RsGxsTunnelId &id,const RsGxsId &peer,const RsGxsId &local,int color,const QByteArray &token)
    {
        auto *m = new Match(peer,local,color);
        m->token = token;
        idle.erase(id);
        games[id] = m;
        QObject::connect(m,&QDialog::finished,m,[=] {
            if (m->phase != Ended && m->phase != Connecting) send(id,token,'X');
            games.erase(id);
            // Keep the tunnel available for a quick rematch. A new match
            // invalidates this close timer even if it has since ended too.
            idle[id] = token;
            QTimer::singleShot(30000,qApp,[=] { releaseWhenFlushed(id,token,60); });
        });
        QObject::connect(&m->accept,&QPushButton::clicked,m,[=] {
            if (m->phase != Offered) return;
            if (!send(id,token,'A')) { m->finish(QObject::tr("Could not send acceptance")); return; }
            m->phase = Confirming;
            m->status.setText(QObject::tr("Confirming game start…"));
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
                    if (m->phase != Playing || m->board.turn() != color || m->board.landing(c)<0) return;
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
        m->status.setText(QObject::tr("Waiting for your opponent to accept"));
    }
    void invite(const RsGxsId &peer,const RsGxsId &local)
    {
        RsGxsTunnelId id;
        uint32_t error = 0;
        if (!net->requestSecuredTunnel(peer,local,id,serviceId,error)) {
            QMessageBox::warning(nullptr,QObject::tr("Connect Four"),QObject::tr("Could not open tunnel (error %1)").arg(error));
            return;
        }
        if (games.count(id)) { games[id]->raise(); games[id]->activateWindow(); return; }
        auto *m = make(id,peer,local,1,QUuid::createUuid().toRfc4122());
        m->status.setText(QObject::tr("Connecting to opponent…"));
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
    void notifyTunnelStatus(const RsGxsTunnelId &id,uint32_t state) override
    {
        QTimer::singleShot(0,qApp,[=] {
            auto it = games.find(id);
            if (it == games.end()) return;
            if (state == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_CAN_TALK) ready(id,it->second);
            if (state == RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_REMOTELY_CLOSED && it->second->phase != Ended)
                it->second->finish(QObject::tr("Opponent disconnected"));
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
            auto it = games.find(id);
            if (it == games.end()) {
                RsGxsTunnelService::GxsTunnelInfo info;
                if (op != 'I' || !net->getTunnelInfo(id,info)) return;
                auto *m = make(id,info.destination_gxs_id,info.source_gxs_id,2,token);
                m->phase = Offered;
                if (!SoundManager::isMute() && RetroChessSettings::invitationSoundEnabled()) {
                    auto *ping = new QMediaPlayer(m);
                    const QUrl source(QStringLiteral("qrc:/sound/ping.mp3"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                    auto *output = new QAudioOutput(ping);
                    output->setVolume(1.0f);
                    ping->setAudioOutput(output);
                    QObject::connect(ping,&QMediaPlayer::errorOccurred,ping,[] { QApplication::beep(); });
                    ping->setSource(source);
#else
                    ping->setVolume(100);
                    QObject::connect(ping,QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error),
                                     ping,[] { QApplication::beep(); });
                    ping->setMedia(source);
#endif
                    ping->play();
                }
                m->status.setText(QObject::tr("Invited you to play Connect Four"));
                m->refresh();
                return;
            }
            auto *m = it->second;
            if (token != m->token) { if (op == 'I') send(id,token,'X'); return; }
            if (op == 'X' && m->phase != Ended) m->finish(QObject::tr("Opponent left the match"));
            if (op == 'A' && m->me == 1 && (m->phase == Waiting || m->phase == Playing)) {
                send(id,token,'S');
                m->phase = Playing;
                m->refresh();
            }
            // An opening move also confirms acceptance for older peers.
            if (m->phase == Confirming && (op == 'S' ||
                    (op == 'M' && packet.at(17) == 0 && m->board.landing(packet.at(18)) >= 0))) {
                m->phase = Playing;
                m->refresh();
            }
            const int col = packet.at(18);
            if (op == 'M' && m->phase == Playing && m->board.turn() != m->me &&
                packet.at(17) == m->board.moves && m->board.landing(col) >= 0) m->move(col);
        });
    }
};
Client client;
}
void FourConnect::registerService(RsGxsTunnelService *service) { client.connectToGxsTunnelService(service); }
void FourConnect::addContactAction(QMenu &menu,const RsGxsId &peer,const RsGxsId &local)
{
    auto *action = menu.addAction(QObject::tr("Invite to Connect Four"));
    action->setEnabled(client.net && !local.isNull() && !peer.isNull() && peer != local);
    QObject::connect(action,&QAction::triggered,&menu,[=] { client.invite(peer,local); });
}
