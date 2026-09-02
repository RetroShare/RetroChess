/*******************************************************************************
 * gui/tile.cpp                                                                *
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

#include "tile.h"
#include "chess.h"
#include "RetroChessSettings.h"
#include "../interface/rsRetroChess.h"

#include <QIcon>

/*extern int count,turn;
extern QWidget *myWidget;
extern Tile *click1;
extern Tile *tile[8][8];
*/

void validate(Tile *tile_p,int c);
void disOrange();

Tile::Tile(QWidget* pParent, Qt::WindowFlags f) : QLabel(pParent, f)
{}

Tile::Tile(const QString& text, QWidget* pParent, Qt::WindowFlags f) : QLabel(text, pParent, f)
{}

void Tile::mousePressEvent(QMouseEvent *event)
{
    RetroChessWindow *chess_window_p = dynamic_cast<RetroChessWindow*>(m_chess_window_p );
    std::string peer_id = (chess_window_p)->mPeerId;

    if( (chess_window_p)->m_flag_finished == 0)	// not finish yet
    {
        // local player's turn
        if((chess_window_p)->m_localplayer_turn == (chess_window_p)->turn)
        {
            validate( ++(chess_window_p)->count );
                if ((chess_window_p)->mIsGxs) {
                    rsRetroChess->chess_click_gxs((chess_window_p)->mGxsId, this->row,this->col, (chess_window_p)->count);
                } else {
                    rsRetroChess->chess_click((chess_window_p)->mPeerId, this->row,this->col, (chess_window_p)->count);
                }
        }
        // not local player's turn
    }

    QLabel::mousePressEvent( event );
}

void Tile::display(char elem)
{
    this->pieceName=elem;
	if (!this->piece) {
		this->clear();
		return;
	}

	QChar pieceCode;
	switch (elem) {
	case 'P': pieceCode = 'P'; break;
	case 'R': pieceCode = 'R'; break;
	case 'H': pieceCode = 'N'; break;
	case 'K': pieceCode = 'K'; break;
	case 'Q': pieceCode = 'Q'; break;
	case 'B': pieceCode = 'B'; break;
	default: this->clear(); return;
	}

	const QChar colorCode = this->pieceColor ? 'w' : 'b';
	const QString resource = QString(":/piece/%1%2.svg").arg(colorCode).arg(pieceCode);
	this->setAlignment(Qt::AlignCenter);
	// QPixmap loads an SVG at its intrinsic 45x45 size. QIcon asks the SVG
	// engine to render directly at the 64x64 tile size, producing a larger,
	// sharper piece without bitmap upscaling.
	this->setPixmap(QIcon(resource).pixmap(this->size()));
}

// check click
void Tile::validate(int c)
{
    Tile *tile_p = this;

    int retValue,i;

    RetroChessWindow *chess_window_p = dynamic_cast< RetroChessWindow*> (m_chess_window_p );

    // click 1
    if(c == 1)
    {
        // clicked current player's piece
        if(tile_p->piece && (tile_p->pieceColor==(chess_window_p)->turn))
        {
            //texp[max++]=tile_p->tileNum;
            retValue = (chess_window_p)->chooser(tile_p);	// paint piece's next availalbe position

            if(retValue)
            {
                (chess_window_p)->click1= new Tile();
                tile_p->setStyleSheet("QLabel {background-color: green;}");
                (chess_window_p)->click1=tile_p;
            }
            else
            {
                //tile_p->setStyleSheet("QLabel {background-color: red;}");
                (chess_window_p)->count=0;
            }
        }

        // didn't clicked current player's piece
        else
        {
            //qDebug()<<"Rascel, clicking anywhere";
            (chess_window_p)->count=0;
        }
    }

    // click 0 or 2 times(piece moved)
    else
    {

        if(tile_p->tileNum==(chess_window_p)->click1->tileNum)
        {
            (chess_window_p)->click1->tileDisplay();
            (chess_window_p)->disOrange();
            (chess_window_p)->max=0;
            (chess_window_p)->count=0;
        }

        for(i=0; i<(chess_window_p)->max; i++)
        {
            // next postion is valiad, then move
			if(tile_p->tileNum==(chess_window_p)->texp[i])
			{
				const int fromTile = (chess_window_p)->click1->tileNum;
				const char movedPiece = (chess_window_p)->click1->pieceName;
				const bool wasCastling = movedPiece == 'K'
				        && qAbs(tile_p->col - (chess_window_p)->click1->col) == 2;
				const bool wasEnPassant = movedPiece == 'P'
				        && (chess_window_p)->isEnPassantMove(
				                (chess_window_p)->click1->row, (chess_window_p)->click1->col,
				                tile_p->row, tile_p->col, (chess_window_p)->click1->pieceColor);
				Tile *capturedTile = wasEnPassant
				        ? (chess_window_p)->tile[(chess_window_p)->click1->row][tile_p->col]
				        : tile_p;
				const bool wasCapture = capturedTile->piece;
				const char capturedPiece = capturedTile->pieceName;
				const int capturedColor = capturedTile->pieceColor;
				if (wasEnPassant) {
					capturedTile->piece = 0;
					capturedTile->display(capturedTile->pieceName);
					capturedTile->tileDisplay();
				}
                (chess_window_p)->click1->piece=0;
                tile_p->piece=1;

                tile_p->pieceColor=(chess_window_p)->click1->pieceColor;
                tile_p->pieceName=(chess_window_p)->click1->pieceName;
				if (wasCastling)
					(chess_window_p)->performCastlingRookMove(
					        tile_p->row, tile_p->col == 6);

                (chess_window_p)->click1->display((chess_window_p)->click1->pieceName);
                tile_p->display((chess_window_p)->click1->pieceName);

                (chess_window_p)->click1->tileDisplay();
                tile_p->pawnLevelupCheck();
                tile_p->tileDisplay();
				const char promotedPiece = movedPiece == 'P' && tile_p->pieceName != 'P'
				        ? tile_p->pieceName : 0;

                retValue=(chess_window_p)->check((chess_window_p)->click1);
                /*
                if(retValue)
                {
                    tile[wR][wC]->setStyleSheet("QLabel {background-color: red;}");
                }
                */

                (chess_window_p)->disOrange();

                (chess_window_p)->max=0;

                (chess_window_p)->turn=((chess_window_p)->turn+1)%2;
                (chess_window_p)->count=0;

                // ---- record last move
                (chess_window_p)->clearLastMove();

                (chess_window_p)->recordLastMove((chess_window_p)->click1->tileNum);
                (chess_window_p)->recordLastMove(tile_p->tileNum);
				(chess_window_p)->recordMove(
				        fromTile, tile_p->tileNum, movedPiece, wasCapture, promotedPiece);
				if (wasCapture)
					(chess_window_p)->recordCapturedPiece(capturedPiece, capturedColor);
				(chess_window_p)->updateCastlingRights(
				        fromTile, tile_p->tileNum, movedPiece,
				        wasCapture ? capturedPiece : 0, capturedColor);
				(chess_window_p)->updateEnPassantTarget(fromTile, tile_p->tileNum, movedPiece);
				(chess_window_p)->playMoveSound(wasCapture);

                (chess_window_p)->playerTurnNotice();
                // ----

                break;
            }
            // next postion is invalid
            else
                (chess_window_p)->count=1;
        }
        (chess_window_p)->drawLastMove();
    }

    (chess_window_p)->m_flag_finished = (chess_window_p)->resultJudge();
}

void Tile::tileDisplay()
{
	const RetroChessBoardTheme theme = RetroChessSettings::boardTheme();
	const QColor color = this->tileColor ? theme.dark : theme.light;
	const QColor hover = color.lighter(120);
	this->setStyleSheet(QString(
	        "QLabel { background-color: %1; } QLabel:hover { background-color: %2; }")
	        .arg(color.name(), hover.name()));
}

void Tile::pawnLevelupCheck()
{
    if( this->pieceName == 'P')
    {
        // white
        if( this->pieceColor && this->row == 0)
			this->display(dynamic_cast<RetroChessWindow*>(m_chess_window_p)
			        ->promotionChoiceForPawn(this->pieceColor));
        // black
        else if( this->pieceColor == 0 && this->row == 7)
			this->display(dynamic_cast<RetroChessWindow*>(m_chess_window_p)
			        ->promotionChoiceForPawn(this->pieceColor));
    }
}


void Tile::setChessWindow(QWidget *board)
{
    m_chess_window_p = board;
}

QWidget* Tile::getChessWindow() const
{
    return m_chess_window_p;
}
