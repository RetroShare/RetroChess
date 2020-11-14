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

class QAction;

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
	void friendSelectionChanged();
	void NeMsgArrived(const RsPeerId &peer_id, QString str);
	void chessStart(const RsPeerId &peer_id);

	void on_broadcastButton_clicked();

    //--- test invite button, try to invite p2p friend
    void enable_inviteButton();	// enable the invite button when selected a friend

    void on_inviteButton_clicked();

	void on_filterPeersButton_clicked();


private:
	Ui::NEMainpage *ui;
	RetroChessNotify *mNotify;

	QAction *mActionPlayChess;
	//RetroChessWindow *tempwindow;

	QMap<std::string, RetroChessWindow*> activeGames;
	void create_chess_window(std::string peer_id, int player_id);
};

#endif // NEMAINPAGE_H
