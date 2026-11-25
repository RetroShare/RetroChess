/*******************************************************************************
 * gui/BackgammonGameWindow.h                                                  *
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

#include "BackgammonResultDialog.h"
#include "core/BgMatch.h"

#include <retroshare/rsids.h>

#include <QByteArray>
#include <QWidget>

#include <memory>

namespace Ui { class BackgammonGameWindow; }


// One match against one opponent: the board, the score, the game log and
// the buttons. The rules live in bg::Match; this window turns clicks into
// match actions and BackgammonService turns its signals into packets.
class BackgammonGameWindow : public QWidget
{
    Q_OBJECT

public:
    enum Phase {
        Connecting,  // opening the tunnel
        Waiting,     // invitation sent, waiting for the answer
        Offered,     // invitation received, waiting for Accept/Decline
        Confirming,  // accepted, waiting for the inviter to start
        Playing,
        Ended
    };

    BackgammonGameWindow(const RsGxsId &peer, const RsGxsId &local,
                         bg::Variant variant, int length, QWidget *parent = nullptr);
    ~BackgammonGameWindow() override;

    Phase phase() const { return m_phase; }
    void setPhase(Phase phase);
    // Progress of an invitation we sent, shown while Connecting/Waiting.
    void setWaitingDetail(const QString &text);

    RsGxsId peerId() const { return m_peer; }
    RsGxsId localId() const { return m_local; }
    bg::Variant variant() const { return m_variant; }
    int matchLength() const { return m_length; }
    QString opponentName() const { return m_opponentName; }
    QString summary() const;

    // Starts the match; the dice of every game come from the seed.
    void startMatch(const QByteArray &diceSeed, bool localIsWhite);
    // Accept / decline an invitation from outside the window (games list,
    // toaster). Accepting shows the window.
    void acceptInvitation();
    void declineInvitation();
    // The match is over (or never started). Shows a small result popup
    // when the window is open.
    void finish(const QString &message,
                BackgammonResultDialog::Outcome outcome = BackgammonResultDialog::Neutral);
    void setLinkDown(bool down);
    bool linkDown() const { return m_linkDown; }

    // Where the match stands: game number and turns played in it.
    int gameIndex() const { return m_match ? m_match->gameIndex() : -1; }
    int ply() const { return m_match ? m_match->ply() : 0; }

    // The opponent's actions, already parsed.
    void remoteDice(int gameIndex, int ply);
    void remoteTurn(int gameIndex, int ply, const QString &moves);
    void remoteDouble(int gameIndex, int ply);
    void remoteDoubleAnswer(bool take, int gameIndex, int ply);
    bool waitingForDoubleAnswer() const;

signals:
    void acceptRequested();
    void resignRequested();
    void closed();   // the window closed (any phase)
    void diceToSend(int gameIndex, int ply);
    void turnToSend(int gameIndex, int ply, const QString &moves);
    void doubleOffered(int gameIndex, int ply);
    void doubleAnswered(bool take, int gameIndex, int ply);
    void summaryChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onDeclineClicked();
    void onRollClicked();
    void onEndTurnClicked();
    void onUndoClicked();
    void onDoubleClicked();
    void onTakeClicked();
    void onDropClicked();
    void updateButtons();

private:
    bg::Side localSide() const { return m_localWhite ? bg::White : bg::Black; }
    bg::Side remoteSide() const { return m_localWhite ? bg::Black : bg::White; }
    QString sideName(bg::Side side) const;
    uint32_t seedFor(int gameIndex) const;

    void refresh();         // header texts
    void syncBoard();       // board, scores and buttons from the match
    void afterAction();     // game or match over? next game
    void startNextGame();
    void announceRoll(bg::Side side);
    void log(const QString &text);
    bool confirmResign(const QString &question);
    void setPlayerName(bool own, const QString &name);
    void updateHeader();   // names, colours, pips, scores, whose turn
    void updateHeaderTextColour();
    void updateWindowTheme();   // background in the board's colours (optional)   // the skin's header text colour for the labels

    Ui::BackgammonGameWindow *ui;
    RsGxsId m_peer;
    RsGxsId m_local;
    bg::Variant m_variant;
    int m_length;
    Phase m_phase { Connecting };
    bool m_linkDown { false };
    bool m_closing { false };   // no result popup while the window closes
    bool m_localWhite { true };
    QString m_opponentName;
    QString m_ownName;
    QString m_waitingDetail;

    std::unique_ptr<bg::Match> m_match;
    QByteArray m_seed;
};
