/*******************************************************************************
 * gui/NEMainpage.h                                                            *
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

/* This is the main page displayed by the plugin
 *
 * For:
 * 	event handle
 * 	peer check
 *  game launching
*/

#ifndef NEMAINPAGE_H
#define NEMAINPAGE_H

#include <retroshare-gui/mainpage.h>
#include <retroshare/rsfiles.h>
#include <retroshare/rspeers.h>
#include "gui/RetroChessNotify.h"

#include "gui/chess.h"

#include <QWidget>

namespace Ui
{
class NEMainpage;
}

class NEMainpage : public MainPage
{
	Q_OBJECT

public:
	explicit NEMainpage(QWidget *parent, RetroChessNotify *notify);
	~NEMainpage();

private slots:
	void setupMenuActions();
	void NeMsgArrived(const RsPeerId &peer_id, QString str);
	void chessStart(const RsPeerId &peer_id);
	void chessStartGxs(const RsGxsId &gxs_id);
	void chessStartGxsAsBlack(const RsGxsId &gxs_id);
	void chessMoveGxs(const RsGxsId &gxs_id, int col, int row, int count);
	void chessPlayerLeftGxs(const RsGxsId &gxs_id);
	void chessRematchGxs(const RsGxsId &gxs_id, int remoteColor);
	void chessGameActionGxs(const RsGxsId &gxs_id, QString action);
	void requestRematchGxs(const RsGxsId &gxs_id, int localColor);
	void requestRematchPeer(QString peerId, int localColor);
	void removeActiveGame(QString gameId);
	void removeActiveGameListing(QString gameId);

	void on_broadcastButton_clicked();
private:
	Ui::NEMainpage *ui;
	RetroChessNotify *mNotify;

	QMap<std::string, RetroChessWindow*> activeGames;
	void create_chess_window(std::string peer_id, int player_id);
    void create_chess_window_gxs(const RsGxsId &gxs_id, int player_id);
};

#endif // NEMAINPAGE_H
