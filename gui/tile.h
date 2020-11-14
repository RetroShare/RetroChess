/*******************************************************************************
 * gui/tile.h                                                                  *
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

#ifndef TILE_H
#define TILE_H

#include <QLabel>
#include <QDebug>
#include <QWidget>

class Tile: public QLabel
{
public:
	//Constructors
	Tile(QWidget* pParent=0, Qt::WindowFlags f=0);
	Tile(const QString& text, QWidget* pParent = 0, Qt::WindowFlags f = 0);

	//Methods
protected:
	void mousePressEvent(QMouseEvent *event);

public:
	void display(char elem);
	void tileDisplay();
    void validate(int c);		// normal check(for sync)

    void pawnLevelupCheck();

	void setChessWindow( QWidget *board);
	QWidget* getChessWindow() const;

	//Fields
	int tileColor;	// "background" 0(black) : 1(white)
	int piece;		// 0(empty) : 1(piece occpied)
	int pieceColor;	// 0(black) : 1(white)
	int row,col;
	int tileNum;	// index in one-division array

	char pieceName;

private:
    QWidget *m_chess_window_p;	//parent chess board
};

void validate_tile(int row, int col, int c);

#endif // TILE_H
