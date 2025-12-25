/*******************************************************************************
 * gui/chess.cpp                                                               *
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

#include <QApplication>

#include "chess.h"
#include "ui_chess.h"

#include "gui/common/AvatarDefs.h"
#include "../services/p3RetroChess.h"

RetroChessWindow::RetroChessWindow(std::string peerid, int player, QWidget *parent) :
	QWidget(parent),
	m_ui( new Ui::RetroChessWindow() ),
	mPeerId(peerid)
	//ui(new Ui::RetroChessWindow)
{
	m_ui->setupUi( this );
    m_ui->m_player1_result->hide();
    m_ui->m_player2_result->hide();
    m_ui->m_status_bar->hide();

    m_flag_finished = 0;	// set as unfinish

	//tile = { { NULL } };
	count=0;
    turn=1;	// white first
	max=0;
	texp = new int[60];

	setGeometry(0,0,1370,700);

	QString player_str;
    if (player )	// local player as black
	{
		p1id = rsPeers->getOwnId();
		p2id = RsPeerId(peerid);
		player_str = " (1)";

        m_localplayer_turn = 0;
	}
    else	// local player as white
	{
		p1id = RsPeerId(peerid);
		p2id = rsPeers->getOwnId();
		player_str = " (2)";

        m_localplayer_turn = 1;
    }

	p1name = rsPeers->getPeerName(p1id);
	p2name = rsPeers->getPeerName(p2id);

	QString title = QString::fromStdString(p2name);
	title += " Playing Chess against ";
	title += QString::fromStdString(p1name);
	title+=player_str;


	this->setWindowTitle(title);

	this->initAccessories();
	this->initChessBoard();

    this->playerTurnNotice();
}

RetroChessWindow::~RetroChessWindow()
{
	delete m_ui;
}

class Border
{
public:
	Border();
	void outline(QWidget *baseWidget, int xPos, int yPos, int Pos)
	{
		QLabel *outLabel = new QLabel(baseWidget);

		if(!Pos)
			outLabel->setGeometry(xPos,yPos,552,20);        //Horizontal Borders

		else
			outLabel->setGeometry(xPos,yPos,20,512);        //Vertical Borders

		outLabel->setStyleSheet("QLabel { background-color :rgb(170, 170, 127); color : black; }");
	}
};

void RetroChessWindow::initAccessories()
{
	// display player's name
	m_ui->m_player1_name->setText( p1name.c_str() );
	m_ui->m_player2_name->setText( p2name.c_str() );

	QPixmap p1avatar;
	AvatarDefs::getAvatarFromSslId(p1id, p1avatar);
	m_ui->m_player1_avatar->setPixmap(p1avatar);//QPixmap(":/images/profile.png"));

	QPixmap p2avatar;
	AvatarDefs::getAvatarFromSslId(p2id, p2avatar);
	m_ui->m_player2_avatar->setPixmap(p2avatar);//QPixmap(":/images/profile.png"));

	//m_ui->m_move_record->setStyleSheet("QLabel {background-color: white;}");
}

void RetroChessWindow::closeEvent(QCloseEvent *event)
{
    // send leave message
    if (mIsGxs) {
        rsRetroChess->player_leave_gxs(this->mGxsId);
    } else {
        rsRetroChess->player_leave(mPeerId);
    }

    QWidget::closeEvent(event);
}

void RetroChessWindow::disOrange()
{
	int i;

	for(i=0; i<max; i++)
		tile[texp[i]/8][texp[i]%8]->tileDisplay();

}

void RetroChessWindow::validate_tile(int row, int col, int c)
{
	Tile *clickedtile = tile[col][row];
	//if (!click1)click1=clickedtile;

    clickedtile->validate(++count);
}

void RetroChessWindow::initChessBoard()
{
	//QWidget *baseWidget, Tile *tile[8][8]
	QWidget *baseWidget = m_ui->m_chess_board;

	int i,j,k = 0,hor,ver;

	Border *border[4] = { NULL };

	//borderDisplay (border size: 552 * 552)
	{
		border[0]->outline( baseWidget,	0,		0,		0);
		border[1]->outline( baseWidget,	0,		532,	0);
		border[2]->outline( baseWidget,	0,		20,		1);
		border[2]->outline( baseWidget,	532,	20,		1);
	}

	//Create 64 tiles (allocating memories to the objects of Tile class)
	ver = 20;

	for(i = 0; i < 8; i++)
	{
		hor = 20;
		for(j=0; j<8; j++)
		{
			tile[i][j] = new Tile(baseWidget);
			tile[i][j]->setChessWindow( this );
			tile[i][j]->tileColor=(i+j)%2;
			tile[i][j]->piece=0;
			tile[i][j]->row=i;
			tile[i][j]->col=j;
			tile[i][j]->tileNum=k++;
			tile[i][j]->tileDisplay();
			tile[i][j]->setGeometry(hor,ver,64,64);
			tile[i][j]->resize( 64, 64 );

			hor+=64;
		}
		ver+=64;
	}

	//white pawns
	for(j=0; j<8; j++)
	{
		tile[1][j]->piece=1;
		tile[1][j]->pieceColor=0;
		tile[1][j]->display('P');
	}

	//black pawns
	for(j=0; j<8; j++)
	{
		tile[6][j]->piece=1;
		tile[6][j]->pieceColor=1;
		tile[6][j]->display('P');
	}

	//white and black remaining elements
	for(j=0; j<8; j++)
	{
		tile[0][j]->piece=1;
		tile[0][j]->pieceColor=0;
		tile[7][j]->piece=1;
		tile[7][j]->pieceColor=1;
	}

	{
		tile[0][0]->display('R');
		tile[0][1]->display('H');
		tile[0][2]->display('B');
		tile[0][3]->display('Q');
		tile[0][4]->display('K');
		tile[0][5]->display('B');
		tile[0][6]->display('H');
		tile[0][7]->display('R');
	}


	{
		tile[7][0]->display('R');
		tile[7][1]->display('H');
		tile[7][2]->display('B');
		tile[7][3]->display('Q');
		tile[7][4]->display('K');
		tile[7][5]->display('B');
		tile[7][6]->display('H');
		tile[7][7]->display('R');
	}

	wR=7;
	wC=4;

	bR=0;
	bC=4;
}


int RetroChessWindow::chooser(Tile *tile_p)
{
	switch(tile_p->pieceName)
	{
	case 'P':
		flag=validatePawn(tile_p);
		break;

	case 'R':
		flag=validateRook(tile_p);
		break;

	case 'H':
		flag=validateHorse(tile_p);
		break;

	case 'K':
		flag=validateKing(tile_p);
		break;

	case 'Q':
		flag=validateQueen(tile_p);
		break;

	case 'B':
		flag=validateBishop(tile_p);
		break;

	}

	orange();

	return flag;
}

//PAWN
int RetroChessWindow::validatePawn(Tile *tile_p)
{
	int row,col;

	row=tile_p->row;
	col=tile_p->col;
	retVal=0;

	//White Pawn
	if(tile_p->pieceColor)
	{
		if(row-1>=0 && !tile[row-1][col]->piece)
		{
			/*int tnum = tile[row-1][col]->tileNum;
			std::cout << "tile: " << texp[max] << std::endl;
			int a = texp[max];
			texp[max] = tnum;
			max++;*/
			texp[max++]=tile[row-1][col]->tileNum;
			retVal=1;
		}

		if(row==6 && !tile[5][col]->piece && !tile[4][col]->piece)
		{
			texp[max++]=tile[row-2][col]->tileNum;
			retVal=1;
		}

		if(row-1>=0 && col-1>=0)
		{
			if(tile[row-1][col-1]->pieceColor!=tile_p->pieceColor && tile[row-1][col-1]->piece)
			{
				texp[max++]=tile[row-1][col-1]->tileNum;
				retVal=1;
			}
		}

		if(row-1>=0 && col+1<=7)
		{
			if(tile[row-1][col+1]->pieceColor!=tile_p->pieceColor && tile[row-1][col+1]->piece)
			{
				texp[max++]=tile[row-1][col+1]->tileNum;
				retVal=1;
			}
		}
	}
	else
	{
		if(row+1<=7 && !tile[row+1][col]->piece)
		{
			texp[max++]=tile[row+1][col]->tileNum;
			retVal=1;
		}

		if(row==1 && !tile[2][col]->piece && !tile[3][col]->piece)
		{
			texp[max++]=tile[row+2][col]->tileNum;
			retVal=1;
		}

		if(row+1<=7 && col-1>=0)
		{
			if(tile[row+1][col-1]->pieceColor!=tile_p->pieceColor && tile[row+1][col-1]->piece)
			{
				texp[max++]=tile[row+1][col-1]->tileNum;
				retVal=1;
			}
		}

		if(row+1<=7 && col+1<=7)
		{
			if(tile[row+1][col+1]->pieceColor!=tile_p->pieceColor && tile[row+1][col+1]->piece)
			{
				texp[max++]=tile[row+1][col+1]->tileNum;
				retVal=1;
			}
		}
	}

	return retVal;
}


//ROOK
int RetroChessWindow::validateRook(Tile *tile_p)
{
	int r,c;

	retVal=0;

	r=tile_p->row;
	c=tile_p->col;
	while(r-->0)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r++<7)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(c++<7)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(c-->0)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}


	return retVal;
}


//HORSE
int RetroChessWindow::validateHorse(Tile *tile_p)
{
	int r,c;
	retVal=0;

	r=tile_p->row;
	c=tile_p->col;

	if(r-2>=0 && c-1>=0)
	{
		if(tile[r-2][c-1]->pieceColor!=tile_p->pieceColor || !tile[r-2][c-1]->piece)
		{
			texp[max++]=tile[r-2][c-1]->tileNum;
			retVal=1;
		}
	}

	if(r-2>=0 && c+1<=7)
	{
		if(tile[r-2][c+1]->pieceColor!=tile_p->pieceColor || !tile[r-2][c+1]->piece)
		{
			texp[max++]=tile[r-2][c+1]->tileNum;
			retVal=1;
		}
	}

	if(r-1>=0 && c-2>=0)
	{
		if(tile[r-1][c-2]->pieceColor!=tile_p->pieceColor || !tile[r-1][c-2]->piece)
		{
			texp[max++]=tile[r-1][c-2]->tileNum;
			retVal=1;
		}
	}

	if(r-1>=0 && c+2<=7)
	{
		if(tile[r-1][c+2]->pieceColor!=tile_p->pieceColor || !tile[r-1][c+2]->piece)
		{
			texp[max++]=tile[r-1][c+2]->tileNum;
			retVal=1;
		}
	}

	if(r+2<=7 && c+1<=7)
	{
		if(tile[r+2][c+1]->pieceColor!=tile_p->pieceColor || !tile[r+2][c+1]->piece)
		{
			texp[max++]=tile[r+2][c+1]->tileNum;
			retVal=1;
		}
	}

	if(r+2<=7 && c-1>=0)
	{
		if(tile[r+2][c-1]->pieceColor!=tile_p->pieceColor || !tile[r+2][c-1]->piece)
		{
			texp[max++]=tile[r+2][c-1]->tileNum;
			retVal=1;
		}
	}

	if(r+1<=7 && c-2>=0)
	{
		if(tile[r+1][c-2]->pieceColor!=tile_p->pieceColor || !tile[r+1][c-2]->piece)
		{
			texp[max++]=tile[r+1][c-2]->tileNum;
			retVal=1;
		}
	}

	if(r+1<=7 && c+2<=7)
	{
		if(tile[r+1][c+2]->pieceColor!=tile_p->pieceColor || !tile[r+1][c+2]->piece)
		{
			texp[max++]=tile[r+1][c+2]->tileNum;
			retVal=1;
		}
	}

	return retVal;
}


//KING
int RetroChessWindow::validateKing(Tile *tile_p)
{
	int r,c;
	retVal=0;

	r=tile_p->row;
	c=tile_p->col;

	if(r-1>=0)
	{
		if(!tile[r-1][c]->piece || tile[r-1][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r-1][c]->tileNum;
			retVal=1;
		}
	}

	if(r+1<=7)
	{
		if(!tile[r+1][c]->piece || tile[r+1][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r+1][c]->tileNum;
			retVal=1;
		}
	}

	if(c-1>=0)
	{
		if(!tile[r][c-1]->piece || tile[r][c-1]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c-1]->tileNum;
			retVal=1;
		}
	}

	if(c+1<=7)
	{
		if(!tile[r][c+1]->piece || tile[r][c+1]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c+1]->tileNum;
			retVal=1;
		}
	}

	if(r-1>=0 && c-1>=0)
	{
		if(!tile[r-1][c-1]->piece || tile[r-1][c-1]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r-1][c-1]->tileNum;
			retVal=1;
		}
	}

	if(r-1>=0 && c+1<=7)
	{
		if(!tile[r-1][c+1]->piece || tile[r-1][c+1]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r-1][c+1]->tileNum;
			retVal=1;
		}
	}

	if(r+1<=7 && c-1>=0)
	{
		if(!tile[r+1][c-1]->piece || tile[r+1][c-1]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r+1][c-1]->tileNum;
			retVal=1;
		}
	}

	if(r+1<=7 && c+1<=7)
	{
		if(!tile[r+1][c+1]->piece || tile[r+1][c+1]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r+1][c+1]->tileNum;
			retVal=1;
		}
	}

	return retVal;
}


//QUEEN
int RetroChessWindow::validateQueen(Tile *tile_p)
{
	int r,c;

	retVal=0;

	r=tile_p->row;
	c=tile_p->col;
	while(r-->0)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r++<7)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(c++<7)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(c-->0)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r-->0 && c++<7)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r-->0 && c-->0)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r++<7 && c++<7)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r++<7 && c-->0)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}


	return retVal;
}

//BISHOP
int RetroChessWindow::validateBishop(Tile *tile_p)
{
	int r,c;
	retVal=0;

	r=tile_p->row;
	c=tile_p->col;
	while(r-->0 && c++<7)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r-->0 && c-->0)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r++<7 && c++<7)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r++<7 && c-->0)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	return retVal;
}

// seems like "check" method is check "King"'s status in current situation. (alive or done)
// for help player to make decition to keep "King" alive.
int RetroChessWindow::check(Tile *tile_p)
{
	int r,c,flag;
	retVal=0;

	return retVal;
}

void RetroChessWindow::orange()
{
    for(int i = 0; i < max; ++i)
		tile[texp[i]/8][texp[i]%8]->setStyleSheet("QLabel {background-color: orange;}");
}

void RetroChessWindow::recordLastMove(int tile_num)
{
    this->m_last_move_que.push_back(tile_num);
}

void RetroChessWindow::drawLastMove()
{
    // draw last move
    for( QQueue<int>::iterator it = this->m_last_move_que.begin();
         it != this->m_last_move_que.end();
         ++it)
    {
        int tile_num = *it;
        tile[ tile_num / 8][ tile_num % 8]->setStyleSheet("QLabel {background-color: lightGray;}");
    }
}

void RetroChessWindow::clearLastMove()
{
    while( !this->m_last_move_que.empty())
    {
        int tile_num = this->m_last_move_que.front();
        this->m_last_move_que.pop_front();

        tile[ tile_num / 8][ tile_num % 8]->tileDisplay();	// revoery tile's background color
    }	// clear the last move queue
}

// judge result (slow method)
// 0: not finish yet
// 1: black win
// 2: white win
int RetroChessWindow::resultJudge()
{
    bool flag_white_king_alive = false;
    bool flag_black_king_alive = false;
    for(int i = 0; i < 8; ++i)
    {
        for(int j = 0; j < 8; ++j)
        {
            if( tile[i][j]->piece)
            {
                if( tile[i][j]->pieceName == 'K' && tile[i][j]->pieceColor == 0)
                    flag_black_king_alive = true;
                else if( tile[i][j]->pieceName == 'K' && tile[i][j]->pieceColor == 1)
                    flag_white_king_alive = true;
            }
        }
    }


    if ( flag_white_king_alive == false)
    {
        m_ui->m_player1_result->setText("Win");
        m_ui->m_player1_result->setStyleSheet("QLabel {color: green;}");

        m_ui->m_player2_result->setText("Defeat");
        m_ui->m_player2_result->setStyleSheet("QLabel {color: red;}");

        m_ui->m_player1_result->setVisible(true);
        m_ui->m_player2_result->setVisible(true);
        return 1;
    }
    else if( flag_black_king_alive == false)
    {
        m_ui->m_player1_result->setText("Defeat");
        m_ui->m_player1_result->setStyleSheet("QLabel {color: red;}");

        m_ui->m_player2_result->setText("Win");
        m_ui->m_player2_result->setStyleSheet("QLabel {color: green;}");

        m_ui->m_player1_result->setVisible(true);
        m_ui->m_player2_result->setVisible(true);
        return 2;
    }
    else
        return 0;
}

void RetroChessWindow::showPlayerLeaveMsg()
{
    std::string player_name = rsPeers->getPeerName( RsPeerId(mPeerId ));
    QString status_msg(player_name.c_str());
    status_msg += " has left";
    m_ui->m_status_bar->setText( status_msg );
    m_ui->m_status_bar->setVisible(true);
}

void RetroChessWindow::playerTurnNotice()
{
    if( this->turn )
    {
        m_ui->m_player1_result->setText("Waiting for opponent");
        m_ui->m_player1_result->setStyleSheet("QLabel {font: 14pt; color: gray;}");

        m_ui->m_player2_result->setText("Turn");
        m_ui->m_player2_result->setStyleSheet("QLabel {font: 14pt; color: green; text:bold;}");

        m_ui->m_player1_result->setVisible(true);
        m_ui->m_player2_result->setVisible(true);
    }
    else
    {
        m_ui->m_player1_result->setText("Turn");
        m_ui->m_player1_result->setStyleSheet("QLabel {font: 14pt;color: green;}");

        m_ui->m_player2_result->setText("Waiting for opponent");
        m_ui->m_player2_result->setStyleSheet("QLabel {font: 14pt;color: gray; text:bold;}");

        m_ui->m_player1_result->setVisible(true);
        m_ui->m_player2_result->setVisible(true);
    }
}
