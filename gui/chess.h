/*******************************************************************************
 * gui/chess.h                                                                 *
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

#ifndef CHESS_H
#define CHESS_H

#include "tile.h"
#include "validation.h"

#include <QWidget>

#include "retroshare/rspeers.h"
#include "retroshare/rsidentity.h"

#include <QQueue>
#include <QStringList>

class QLabel;
class QTableWidget;
class QMediaPlayer;

namespace Ui
{
class RetroChessWindow;
};

class RetroChessWindow : public QWidget
{
	Q_OBJECT

private:
	Ui::RetroChessWindow *m_ui;	//ui

	void initAccessories();
	void initChessBoard();
	RsPeerId p1id;
	RsPeerId p2id;
	std::string p1name;
	std::string p2name;

protected:
    void closeEvent(QCloseEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

public:
	std::string mPeerId;
	explicit RetroChessWindow(std::string peerid, int player = 0, QWidget *parent = 0);
	explicit RetroChessWindow(const RsGxsId &gxsId, int player = 0, QWidget *parent = 0);
	~RetroChessWindow();
	int currentplayer;
	int myid;
	RsGxsId mGxsId; // Store GXS identity if using a tunnel
	RsGxsId mOwnGxsId; // Exact local identity used by this GXS tunnel
	bool mIsGxs;
	bool m_suppressLeave;
	bool m_resultPopupShown;
	bool m_rematchRequested;

	//from global

	int wR,wC,bR,bC;

	Tile *click1;
	Tile *tile[8][8];

	int count;	// click counter
    int turn;	// player turn (0: black turn / 1: white turn)
    int m_localplayer_turn;	// record local player's character (0: black / 1: white)
	int max;
	int *texp;
    int m_flag_finished;	// finish flag: (0: not finish / non-zero: finished)

	void disOrange();
	void validate_tile(int row, int col, int c);

	int flag,retVal;
	int chooser(Tile *temp);
	int validateBishop(Tile *temp);
	int validateQueen(Tile *temp);
	int validateKing(Tile *temp);
	int validateHorse(Tile *temp);
	int validateRook(Tile *temp);
	int validatePawn(Tile *temp);

	void orange();	// draw orange background represent avaiable movement of tiles
	int check(Tile *temp);

    QQueue<int> m_last_move_que;	// record last move numbers
	QStringList m_move_history;
	QString m_capturedBlack;
	QString m_capturedWhite;
	QLabel *m_capturedBlackLabel;
	QLabel *m_capturedWhiteLabel;
	QTableWidget *m_moveTable;
	QMediaPlayer *m_moveSound;
	QMediaPlayer *m_captureSound;
	QMediaPlayer *m_victorySound;
    void recordLastMove( int tile_num );
	void recordMove(int fromTile, int toTile, char pieceName, bool capture);
	void recordCapturedPiece(char pieceName, int pieceColor);
	void playMoveSound(bool capture);
	void sendGameAction(const QString &action);
	void applyGameAction(const QString &action, bool remote);
	void showGameStatus(const QString &status);
	void refreshBoardTheme();
    void drawLastMove();
    void clearLastMove();

    int resultJudge();	// judge result (slow method)
    void showPlayerLeaveMsg();	// show player leave message
    void playerTurnNotice();
	void closeForRematch();
	void showGameResultDialog(bool localWon, bool draw = false);

signals:
	void rematchRequested(const RsGxsId &gxsId, int localColor);
	void rematchRequestedPeer(QString peerId, int localColor);
	void gameClosed(QString gameId);
	void gameEnded(QString gameId);
};


extern QWidget* make_board();

#endif // CHESS_H
