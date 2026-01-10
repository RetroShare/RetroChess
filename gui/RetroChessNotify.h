/*******************************************************************************
 * gui/RetroChessNotify.h                                                      *
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

// This class is a Qt object to get notification from the plugin's service threads,
// and responsible to pass the info the the GUI part.
//
// Because the GUI part is async-ed with the service, it is crucial to use the
// QObject connect system to communicate between the p3Service and the gui part (handled by Qt)
//
#ifndef NETEXAMPLENOTIFY_H
#define NETEXAMPLENOTIFY_H

#include <retroshare/rstypes.h>

#include <QObject>

class RetroChessNotify : public QObject
{
	Q_OBJECT
public:
	explicit RetroChessNotify(QObject *parent = 0);
	void notifyReceivedPaint(const RsPeerId &peer_id, int x, int y) ;
	void notifyReceivedMsg(const RsPeerId &peer_id, QString str) ;
	void notifyChessStart(const RsPeerId &peer_id) ;
	void notifyChessInvite(const RsPeerId &peer_id) ;

signals:
	void NeMsgArrived(const RsPeerId &peer_id, QString str) ; // emitted when the peer gets a msg

	void chessStart(const RsPeerId &peer_id) ;
	void chessInvited(const RsPeerId &peer_id) ;

	void chessMoveGxs(const RsGxsId &gxs_id, int col, int row, int count);
	void gxsTunnelReady(const RsGxsId &gxs_id);

public slots:
};

#endif // NETEXAMPLENOTIFY_H
