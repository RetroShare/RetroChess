/*******************************************************************************
 * gui/RetroChessNotify.cpp                                                    *
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

#include "RetroChessNotify.h"

RetroChessNotify::RetroChessNotify(QObject *parent) : QObject(parent)
{

}

void RetroChessNotify::notifyReceivedPaint(const RsPeerId &peer_id, int x, int y)
{
	std::cout << "pNotify Recvd paint from: " << peer_id;
	std::cout << " at " << x << " , " << y;
	std::cout << std::endl;
}


void RetroChessNotify::notifyReceivedMsg(const RsPeerId& peer_id, QString str)
{
	std::cout << "pNotify Recvd Packet from: " << peer_id;
	std::cout << " saying " << str.toStdString();
	std::cout << std::endl;
	emit NeMsgArrived(peer_id, str) ;
}

void RetroChessNotify::notifyChessStart(const RsPeerId &peer_id)
{
	emit chessStart(peer_id) ;

}
void RetroChessNotify::notifyChessInvite(const RsPeerId &peer_id)
{
	emit chessInvited(peer_id) ;

}
