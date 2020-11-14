/*******************************************************************************
 * gui/RetroChessChatWidgetHolder.h                                            *
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

#ifndef RETROCHESSCHATWIDGETHOLDER_H
#define RETROCHESSCHATWIDGETHOLDER_H

// #include "RetroChessChatWidgetHolder.h"	// why include him self ?? (O.O;
#include <gui/chat/ChatWidget.h>
#include <gui/common/RsButtonOnText.h>
#include <gui/RetroChessNotify.h>

class RetroChessChatWidgetHolder : public QObject, public ChatWidgetHolder
{
	Q_OBJECT

public:
	RetroChessChatWidgetHolder(ChatWidget *chatWidget, RetroChessNotify *notify);
	virtual ~RetroChessChatWidgetHolder();

public slots:
	void chessPressed();
	void chessStart();
	void chessnotify(RsPeerId from_peer_id);


private slots:
	void botMouseEnter();
	void botMouseLeave();

protected:
	QToolButton *playChessButton ;
	RetroChessNotify *mRetroChessNotify;

	typedef QMap<QString, RSButtonOnText*> button_map;
	button_map buttonMapTakeChess;
};

#endif // RETROCHESSCHATWIDGETHOLDER_H
