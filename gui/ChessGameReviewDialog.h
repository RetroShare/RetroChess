/*******************************************************************************
 * gui/ChessGameReviewDialog.h                                                *
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

#ifndef CHESSGAMEREVIEWDIALOG_H
#define CHESSGAMEREVIEWDIALOG_H

#include "ChessGameHistory.h"

#include <QDialog>

class QLabel;
class QPushButton;
class QTableWidget;
class QTimer;
class QMediaPlayer;

class ChessGameReviewDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ChessGameReviewDialog(const ChessGameRecord &game, QWidget *parent = nullptr);
	virtual ~ChessGameReviewDialog();

private slots:
	void stepForward();
	void togglePlayPause();

private:
	void showPly(int ply, bool playSound = false);
	void updateControls();
	void updatePlayPauseButton();
	void startPlayback();
	void pausePlayback();
	void playMoveSound(bool capture);
	bool isCapture(int ply) const;
	void lastMoveSquares(int ply, int &fromSquare, int &toSquare) const;

	ChessGameRecord m_game;
	QLabel *m_squares[64];
	QLabel *m_borders[4];
	QLabel *m_winnerBadge;
	QLabel *m_loserBadge;
	QTableWidget *m_moves;
	QPushButton *m_first;
	QPushButton *m_previous;
	QPushButton *m_playPause;
	QPushButton *m_next;
	QPushButton *m_last;
	QLabel *m_positionLabel;
	QTimer *m_playTimer;
	QMediaPlayer *m_moveSound;
	QMediaPlayer *m_captureSound;
	int m_ply;
};

#endif // CHESSGAMEREVIEWDIALOG_H
