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
#include "../interface/rsRetroChess.h"

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

    if(this->pieceColor && this->piece)
    {
        switch(elem)
        {
        case 'P':
            this->setPixmap(QPixmap(":/images/pawn_white.svg"));
            break;
        case 'R':
            this->setPixmap(QPixmap(":/images/rook_white.svg"));
            break;
        case 'H':
            this->setPixmap(QPixmap(":/images/knight_white.svg"));
            break;
        case 'K':
            this->setPixmap(QPixmap(":/images/king_white.svg"));
            break;
        case 'Q':
            this->setPixmap(QPixmap(":/images/queen_white.svg"));
            break;
        case 'B':
            this->setPixmap(QPixmap(":/images/bishop_white.svg"));
            break;
        }
    }

    else if(this->piece)
    {
        switch(elem)
        {
        case 'P':
            this->setPixmap(QPixmap(":/images/pawn_black.svg"));
            break;
        case 'R':
            this->setPixmap(QPixmap(":/images/rook_black.svg"));
            break;
        case 'H':
            this->setPixmap(QPixmap(":/images/knight_black.svg"));
            break;
        case 'K':
            this->setPixmap(QPixmap(":/images/king_black.svg"));
            break;
        case 'Q':
            this->setPixmap(QPixmap(":/images/queen_black.svg"));
            break;
        case 'B':
            this->setPixmap(QPixmap(":/images/bishop_black.svg"));
            break;
        }
    }
    else
        this->clear();
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
                (chess_window_p)->click1->piece=0;
                tile_p->piece=1;

                tile_p->pieceColor=(chess_window_p)->click1->pieceColor;
                tile_p->pieceName=(chess_window_p)->click1->pieceName;

                (chess_window_p)->click1->display((chess_window_p)->click1->pieceName);
                tile_p->display((chess_window_p)->click1->pieceName);

                (chess_window_p)->click1->tileDisplay();
                tile_p->pawnLevelupCheck();
                tile_p->tileDisplay();

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

    if(this->tileColor)
        this->setStyleSheet("QLabel {background-color: rgb(120, 120, 90);}:hover{background-color: rgb(170,85,127);}");
    else
        this->setStyleSheet("QLabel {background-color: rgb(211, 211, 158);}:hover{background-color: rgb(170,95,127);}");
}

void Tile::pawnLevelupCheck()
{
    if( this->pieceName == 'P')
    {
        // white
        if( this->pieceColor && this->row == 0)
            this->display( 'Q' );
        // black
        else if( this->pieceColor == 0 && this->row == 7)
            this->display( 'Q' );
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
