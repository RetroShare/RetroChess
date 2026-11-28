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
#include "ui_FourResultDialog.h"
#include "../gui/RetroChessSettings.h"
#include "../gui/RetroChessSoundPlayer.h"
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
enum Outcome { NoResult, Won, Lost, Drawn };

// Glossy disc of player 1 (red) or 2 (yellow), filling rect.
void paintPiece(QPainter &painter, const QRectF &rect, int player)
{
    const QColor light[] = {QColor(), QColor("#ff9b93"), QColor("#ffeaa6")};
    const QColor base[]  = {QColor(), QColor("#e8464f"), QColor("#f2be35")};
    const QColor dark[]  = {QColor(), QColor("#a5202b"), QColor("#c08510")};
    const QColor rim[]   = {QColor(), QColor("#8c1a24"), QColor("#a2700c")};
    QRadialGradient gradient(rect.center() - QPointF(rect.width()*0.18, rect.height()*0.22),
                             rect.width()*0.72);
    gradient.setColorAt(0, light[player]);
    gradient.setColorAt(0.5, base[player]);
    gradient.setColorAt(1, dark[player]);
    painter.setBrush(gradient);
    painter.setPen(QPen(rim[player], qMax<qreal>(1.0, rect.width()*0.03)));
    painter.drawEllipse(rect);
    // Inner groove like a real counter.
    const qreal inset = rect.width()*0.17;
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(0,0,0,45), qMax<qreal>(1.0, rect.width()*0.025)));
    painter.drawEllipse(rect.adjusted(inset,inset,-inset,-inset));
}

QPixmap pieceIcon(int player, int size)
{
    const qreal ratio = qApp->devicePixelRatio();
    QPixmap pixmap(QSize(size,size)*ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    paintPiece(painter, QRectF(1,1,size-2,size-2), player);
    painter.end(); // finish painting before the pixmap is copied out
    return pixmap;
}

class PlayerStrip : public QWidget
{
public:
    QString nickname() const { return name.text(); }
    PlayerStrip(const RsGxsId &id, const QString &role, int player)
    {
        auto *row = new QHBoxLayout(this);
        row->setContentsMargins(0,0,0,0);
        row->addWidget(&avatar);
        row->addWidget(&name,1);
        auto *piece = new QLabel;
        piece->setPixmap(pieceIcon(player,16));
        row->addWidget(piece);
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
    bool last = false;      // most recent move: white dot
    bool highlight = false; // part of the winning line: white ring
    bool dim = false;       // game won, but not part of the winning line
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const qreal margin = width()*0.07;
        const QRectF hole = QRectF(rect()).adjusted(margin,margin,-margin,-margin);
        // Recessed hole, a little lighter while its column is hovered.
        const bool hot = isEnabled() && columnHovered;
        QLinearGradient depth(hole.topLeft(), hole.bottomLeft());
        depth.setColorAt(0, hot ? QColor("#0e1a30") : QColor("#070d18"));
        depth.setColorAt(1, hot ? QColor("#26385a") : QColor("#1a2740"));
        painter.setPen(Qt::NoPen);
        painter.setBrush(depth);
        painter.drawEllipse(hole);
        const bool showPreview = !player && hot && preview;
        if (player || showPreview) {
            painter.setOpacity(showPreview ? 0.35 : dim ? 0.3 : 1.0);
            paintPiece(painter, hole.adjusted(1,1,-1,-1), player ? player : preview);
            painter.setOpacity(1.0);
        }
        if (highlight) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(255,255,255,70), margin*1.3));
            painter.drawEllipse(hole);
            painter.setPen(QPen(Qt::white, qMax<qreal>(2.0, width()*0.035)));
            painter.drawEllipse(hole);
        }
        if (last && player) {
            const qreal radius = qMax<qreal>(2.0, width()*0.045);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255,255,255,235));
            painter.drawEllipse(hole.center(), radius, radius);
        }
        if (isEnabled() && hasFocus() && !columnHovered) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(255,255,255,160),1,Qt::DashLine));
            painter.drawEllipse(hole.adjusted(3,3,-3,-3));
        }
    }
};

class BoardArea : public QWidget
{
public:
    Disc *cells[6][7];
    BoardArea()
    {
        setMinimumSize(7*28,6*28);
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
        // Leave a frame of 0.2 cell around the grid and room for the shadow.
        const int side = qMax(1, int(qMin(width()/7.4, (height()-shadow)/6.4)));
        const int pad = side/5;
        const int left = (width()-7*side)/2, top = (height()-shadow-6*side)/2;
        panel = QRectF(left-pad, top-pad, 7*side+2*pad, 6*side+2*pad);
        for (int r = 0; r < 6; ++r)
            for (int c = 0; c < 7; ++c)
                cells[r][c]->setGeometry(left+c*side,top+r*side,side,side);
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const qreal radius = qMax<qreal>(10.0, panel.width()*0.035);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0,0,0,110));
        painter.drawRoundedRect(panel.translated(0,shadow), radius, radius);
        QLinearGradient body(panel.topLeft(), panel.bottomRight());
        body.setColorAt(0, QColor("#4b689a"));
        body.setColorAt(1, QColor("#22385f"));
        painter.setBrush(body);
        painter.setPen(QPen(QColor("#6280b3"), 1));
        painter.drawRoundedRect(panel, radius, radius);
    }
private:
    static constexpr int shadow = 6;
    QRectF panel;
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
        // Own widgets are matched by object name so the discs (also
        // QPushButtons) keep their custom painting.
        setStyleSheet(QStringLiteral(
            "QDialog { background: #111a2b; }"
            "QLabel { color: #e3e8f0; background: transparent; }"
            "QLabel#fourStatus { font-size: 14px; font-weight: bold; color: white; }"
            "QListWidget { background: #18233a; color: #d3dbe8; border: 1px solid #2b3c5e;"
            " border-radius: 8px; padding: 4px; }"
            "QPushButton#fourLeave, QPushButton#fourAccept { min-height: 30px; padding: 3px 18px;"
            " color: white; background: #2a3a58; border: 1px solid #415a86; border-radius: 6px; }"
            "QPushButton#fourLeave:hover { background: #3c5a8e; }"
            "QPushButton#fourAccept { background: #3f7d32; border-color: #56a145; }"
            "QPushButton#fourAccept:hover { background: #4f9a3f; }"));
        opponent = new PlayerStrip(peer,me == 1 ? tr("Opponent · Yellow") : tr("Opponent · Red"),3-me);
        layout->addWidget(opponent,0,0);
        layout->addWidget(&area,1,0);
        self = new PlayerStrip(local,me == 1 ? tr("You · Red") : tr("You · Yellow"),me);
        layout->addWidget(self,2,0);
        status.setObjectName(QStringLiteral("fourStatus"));
        status.setWordWrap(true);
        layout->addWidget(&status,0,1);
        layout->addWidget(&history,1,1);
        history.setMinimumWidth(130);
        layout->setColumnStretch(0,3);
        layout->setColumnStretch(1,1);
        layout->setRowStretch(1,1);
        accept.setObjectName(QStringLiteral("fourAccept"));
        leave.setObjectName(QStringLiteral("fourLeave"));
        auto *actions = new QHBoxLayout;
        actions->addWidget(&accept);
        actions->addWidget(&leave);
        layout->addLayout(actions,2,1);
        connect(&leave,&QPushButton::clicked,this,[this] {
            if (phase != Playing) { close(); return; }
            // Resigning keeps the window open and shows the result popup.
            if (confirmResign(tr("Resign this game?"))) resign(false);
        });
    }
    // Esc, the title bar close button and close() all end up here.
    void reject() override
    {
        if (phase == Playing) {
            if (!confirmResign(tr("Leaving now resigns the game. Leave anyway?"))) return;
            resign(true);
        }
        QDialog::reject();
    }
    bool confirmResign(const QString &question)
    {
        return QMessageBox::question(this,tr("Connect Four"),question,
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No) == QMessageBox::Yes;
    }
    // quiet: the window is closing, so no sound and no popup.
    void resign(bool quiet)
    {
        // The game may have ended while the confirmation was open.
        if (phase != Playing) return;
        if (onResign) onResign();
        finish(tr("You resigned."),Lost,quiet);
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
                button->last = r == board.lastRow && c == board.lastColumn;
                button->highlight = board.winning(r,c);
                button->dim = board.winner && !board.winning(r,c);
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
            audio = new RetroChessSoundPlayer(this);
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
    QString headline(Outcome outcome, const QString &reason) const
    {
        switch (outcome) {
        case Won:      return tr("You won!");
        case Lost:     return tr("You lost");
        case Drawn:    return tr("Draw");
        case NoResult: break;
        }
        return reason;
    }
    // Ends the match. Results, and any game cut short after it started,
    // are announced in a popup like RetroChess does.
    void finish(const QString &reason, Outcome outcome = NoResult, bool quiet = false)
    {
        if (phase == Ended) return;
        const bool started = phase == Playing;
        phase = Ended;
        linkDown = false;
        status.setText(headline(outcome,reason));
        setWindowTitle(tr("Connect Four — %1").arg(headline(outcome,reason)));
        refresh();
        if (quiet) return;
        const char *sounds[] = {nullptr, "qrc:/sound/victory.mp3", "qrc:/sound/Defeat.mp3", "qrc:/sound/Draw.mp3"};
        if (outcome != NoResult)
            playSound(QString::fromLatin1(sounds[outcome]),RetroChessSettings::gameResultSoundEnabled());
        if (outcome != NoResult || started) showResult(outcome,reason);
    }
    void showResult(Outcome outcome, const QString &reason)
    {
        auto *dialog = new QDialog(this);
        Ui::FourResultDialog ui;
        ui.setupUi(dialog);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        // Blocks only this game window, not the rest of RetroShare.
        dialog->setWindowModality(Qt::WindowModal);
        const int winnerColor = outcome == Won ? me : outcome == Lost ? 3-me : 0;
        if (winnerColor) {
            ui.resultIcon->setPixmap(pieceIcon(winnerColor,52));
        } else if (outcome == Drawn) {
            const qreal ratio = qApp->devicePixelRatio();
            QPixmap both(QSize(86,52)*ratio);
            both.setDevicePixelRatio(ratio);
            both.fill(Qt::transparent);
            QPainter painter(&both);
            painter.setRenderHint(QPainter::Antialiasing);
            paintPiece(painter,QRectF(1,1,50,50),1);
            paintPiece(painter,QRectF(35,1,50,50),2);
            painter.end();
            ui.resultIcon->setPixmap(both);
        } else {
            ui.resultIcon->hide();
        }
        ui.resultTitle->setText(outcome == NoResult ? tr("Game over") : headline(outcome,reason));
        ui.resultText->setText(reason);
        ui.rematchButton->setVisible(outcome != NoResult && onRematch);
        connect(ui.closeButton,&QPushButton::clicked,this,[this,dialog] {
            dialog->close();
            close();
        });
        connect(ui.rematchButton,&QPushButton::clicked,this,[this,dialog] {
            dialog->close();
            // Call a copy: invite() can end up closing this window (and so
            // destroying onRematch) while it is still running, e.g. when an
            // invitation from the opponent arrives during an error message box.
            const auto rematch = onRematch;
            if (rematch) rematch(); // replaces this window with a new invitation
        });
        dialog->show();
    }
    void move(int column)
    {
        const int player = board.turn();
        board.drop(column);
        history.addItem(tr("%1. Col %2, %3").arg(board.moves).arg(column+1)
                        .arg((player == me ? self : opponent)->nickname()));
        history.scrollToBottom();
        if (board.winner)
            finish(player == me ? tr("Four in a row — well played.") : tr("Your opponent connected four."),
                   player == me ? Won : Lost);
        else if (board.moves == 42)
            finish(tr("The board is full."),Drawn);
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
    std::function<void()> onResign, onRematch;
    QByteArray token;
    QLabel status;
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
        // Same tunnel, fresh invitation: invite() retires this finished match.
        m->onRematch = [=] { invite(peer,local); };
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
                    m->finish(op == 'R' ? QObject::tr("Your opponent resigned.")
                                        : QObject::tr("Your opponent left the game."),Won);
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
