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


#include <QWidget>

#include "retroshare/rspeers.h"
#include "retroshare/rsidentity.h"

#include <QQueue>
#include <QHash>
#include <QStringList>
#include <QVector>

#include "ChessGameHistory.h"
#include "ChessPosition.h"
#include "ChessTimeControl.h"

class QLabel;
class QTableWidget;
class QMediaPlayer;
class QPushButton;
class QStatusBar;
class QTimer;
class ChessDebugWidget;
class ChessBoard;
class ChessClockWidget;
class RetroChessLeaderboard;

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
	void layoutChessBoard();
	RsPeerId p1id;
	RsPeerId p2id;
	RsGxsId mSpectatorWhiteId;
	RsGxsId mSpectatorBlackId;
	// GXS identity backing m_player1_name / m_player2_name, when known (for rating lookup).
	// Left null for legacy RsPeerId-based games, which have no leaderboard entry.
	RsGxsId mPlayer1GxsId;
	RsGxsId mPlayer2GxsId;
	RetroChessLeaderboard *mLeaderboard = nullptr;
	void refreshPlayerRatings();
	std::string p1name;
	std::string p2name;

protected:
    void closeEvent(QCloseEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

public:
	std::string mPeerId;
	explicit RetroChessWindow(std::string peerid, int player = 0, QWidget *parent = 0);
	explicit RetroChessWindow(const RsGxsId &gxsId, int player = 0, QWidget *parent = 0);
	explicit RetroChessWindow(const RsGxsId &hostId, const QString &gameKey,
	                          const QString &whiteId, const QString &whiteName,
	                          const QString &blackId, const QString &blackName,
	                          QWidget *parent = 0);
	~RetroChessWindow();
	QString activeGameDescription() const;
	int currentplayer;
	int myid;
	RsGxsId mGxsId; // Store GXS identity if using a tunnel
	RsGxsId mOwnGxsId; // Exact local identity used by this GXS tunnel
	bool mIsGxs;
	bool m_isSpectator;
	bool m_flipped;
	bool m_suppressLeave;
	bool m_resultPopupShown;
	bool m_rematchRequested;
	bool m_resultSubmitted;
	QString mGameId;

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
	void showLivePosition();
	QString sessionFen() const;
	uint32_t sessionMoveSequence() const;
	bool restoreSessionPosition(
	        const QString &fen, uint32_t moveSequence,
	        const QStringList &moves,
	        QString *error = nullptr);
	bool restoreSessionPosition(
	        const QString &fen, uint32_t moveSequence,
	        QString *error = nullptr);

	int chooser(Tile *temp);
	int validateBishop(Tile *temp);
	int validateQueen(Tile *temp);
	int validateKing(Tile *temp);
	int validateHorse(Tile *temp);
	int validateRook(Tile *temp);
	int validatePawn(Tile *temp);
	bool isSquareAttacked(int row, int col, int attackingColor) const;
	bool isKingInCheck(int color) const;
	bool isPseudoLegalMove(int fromRow, int fromCol, int toRow, int toCol, int color) const;
	bool isLegalMove(int fromRow, int fromCol, int toRow, int toCol, int color);
	bool hasAnyLegalMove(int color);
	bool isDeadPosition() const;
	QString currentPositionKey();
	void recordCurrentPosition();
	bool canClaimThreefoldRepetition();
	void updateHalfmoveClock(char movedPiece, bool capture);
	bool canClaimFiftyMoveRule() const;
	bool isEnPassantMove(int fromRow, int fromCol, int toRow, int toCol, int color) const;
	void updateEnPassantTarget(int fromTile, int toTile, char movedPiece);
	bool canCastle(int color, bool kingSide) const;
	bool isCastlingMove(int fromRow, int fromCol, int toRow, int toCol, int color) const;
	void performCastlingRookMove(int row, bool kingSide);
	void updateCastlingRights(
	        int fromTile, int toTile, char movedPiece,
	        char capturedPiece, int capturedColor);
	char promotionChoiceForPawn(int color);
	void highlightCheckedKing(int color);
	void clearKingCheckHighlight();
	int m_checkedKingTile;
	int m_enPassantPawnTile;
	char m_pendingPromotionChoice;
	bool m_kingMoved[2];
	bool m_rookMoved[2][2]; // [color][0 queenside, 1 kingside]
	int m_halfmoveClock;

	void orange();	// draw orange background represent avaiable movement of tiles
	int check(Tile *temp);

    QQueue<int> m_last_move_que;	// record last move numbers
	QStringList m_move_history;
	QHash<QString, int> m_positionOccurrences;
	QString m_capturedBlack;
	QString m_capturedWhite;
	QLabel *m_capturedBlackLabel;
	QLabel *m_capturedWhiteLabel;
	QLabel *m_drawBadges[2];
	QLabel *m_winnerBadge;
	QLabel *m_loserBadge;
	QWidget *m_resultBar;
	QLabel *m_resultTextLabel;
	QLabel *m_resultInfoIcon;
	void updateEndGameBadges(int ply);
	QTableWidget *m_moveTable;
	QPushButton *m_historyFirstButton;
	QPushButton *m_historyPreviousButton;
	QPushButton *m_historyNextButton;
	QPushButton *m_historyLatestButton;
	QVector<QString> m_boardHistory;
	QVector<QPair<int, int>> m_boardHistoryMoves;
	int m_viewedHistoryPly;
	void recordBoardSnapshot(int fromTile = -1, int toTile = -1);
	void showHistoryPly(int ply);
	void updateHistoryControls();
	void updateCapturedPiecesDisplay();
	void updateCapturedPiecesForPly(int ply);
	void renderCapturedDisplays(const QString &capturedBlack, const QString &capturedWhite);
	static int calculatePiecePoints(const QString &pieces);
	static QPixmap renderCapturedStrip(
	        const QString &capturedPieces,
	        int advantage,
	        bool isWhitePieces,
	        int targetWidth,
	        int targetHeight,
	        qreal dpr = 1.0);
	QMediaPlayer *m_moveSound;
	QMediaPlayer *m_captureSound;
	QMediaPlayer *m_victorySound;
	QMediaPlayer *m_drawSound;
	QMediaPlayer *m_defeatSound;
	QStatusBar *m_gameStatusBar;
	ChessDebugWidget *m_debugWidget;
	ChessBoard *m_chessBoard;
	ChessPosition m_position;
	int m_fullmoveNumber;
	bool m_desynchronized;
    void recordLastMove( int tile_num );
	void recordMove(int fromTile, int toTile, char pieceName, bool capture, char promotion = 0);
	void recordCapturedPiece(char pieceName, int pieceColor);
	void playMoveSound(bool capture);
	// Returns false when the action could not be delivered right now; it is then
	// queued and resent (in order) once the tunnel to the opponent is back.
	bool sendGameAction(const QString &action);
	void queueUnsentAction(const QString &action);
	void flushUnsentActions();
	QStringList m_unsentActions;
	QTimer *m_resendTimer = nullptr;
	int m_resendAttempts = 0;
	void sendMoveAction(int fromTile, int toTile, char promotion);
	void applyGameAction(const QString &action, bool remote);
	QString currentFen() const;
	QString positionHash() const;
	bool loadFen(const QString &fen, QString *error = nullptr);
	void appendDebugEvent(const QString &event);
	void showChessDebugWindow();
	void updateDebugWindow();
	void stopForDesynchronization(const QString &reason);
	void showGameStatus(const QString &status);
	void refreshBoardTheme();
	ChessGameRecord historyRecord() const;
	QStringList moveHistory() const { return m_move_history; }
	void setMoveHistory(const QStringList &moves);
    void drawLastMove();
    void clearLastMove();
    void setLastMove(int fromTile, int toTile);
    int lastMoveFrom() const;
    int lastMoveTo() const;

    int resultJudge();	// judge result (slow method)
    void showPlayerLeaveMsg();	// show player leave message
	void submitRatedResult(bool localWon, bool draw = false);
    void playerTurnNotice();
	void closeForRematch();
	void showGameResultDialog(bool localWon, bool draw = false, const QString &reason = QString());
	void showSpectatorResult(const QString &result, const QString &reason);
	void completeGameHistory(const QString &result, const QString &reason);
	void activateBoardSquare(int square);
	void setupClocks();
	void onClockExpired(int color);
	void setTimeControl(const ChessTimeControl &tc);
	ChessTimeControl timeControl() const { return m_timeControl; }
	// Lets the window look up and display live ratings next to player names
	// (bold nickname followed by "(rating)"), and keep them updated as the
	// leaderboard recomputes. Safe to call with nullptr.
	void setLeaderboard(RetroChessLeaderboard *leaderboard);
	ChessTimeControl m_timeControl;
	ChessClockWidget *m_whiteClock = nullptr;
	ChessClockWidget *m_blackClock = nullptr;
	QDateTime m_gameStartedAt;
	QString m_gameResult;
	QString m_gameEndReason;
	bool m_gameArchived;
	bool m_drawOfferPending = false; // local side sent draw_offer and awaits an answer

signals:
	void ratedResult(QString gameId, RsGxsId white, RsGxsId black, QString result);
	void rematchRequested(const RsGxsId &gxsId, int localColor);
	void rematchRequestedPeer(QString peerId, int localColor);
	void gameClosed(QString gameId);
	void gameEnded(QString gameId);
	void gameReadyForHistory();
	void sessionStateChanged(const QString &fen, uint32_t moveSequence, int fromTile = -1, int toTile = -1, const QStringList &moves = QStringList());
	void spectatorClosed(const RsGxsId &hostId, const QString &gameKey);
};


extern QWidget* make_board();

#endif // CHESS_H
