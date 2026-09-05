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

class ChessGameReviewDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ChessGameReviewDialog(const ChessGameRecord &game, QWidget *parent = nullptr);

private:
	void showPly(int ply);
	void updateControls();

	ChessGameRecord m_game;
	QLabel *m_squares[64];
	QTableWidget *m_moves;
	QPushButton *m_first;
	QPushButton *m_previous;
	QPushButton *m_next;
	QPushButton *m_last;
	QLabel *m_positionLabel;
	int m_ply;
};

#endif // CHESSGAMEREVIEWDIALOG_H
