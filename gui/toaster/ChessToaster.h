/*******************************************************************************
 * gui/toaster/ChessToaster.h                                                  *
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
 ******************************************************************************/

#ifndef CHESSTOASTER_H
#define CHESSTOASTER_H

#include "ui_ChessToaster.h"
#include <retroshare/rstypes.h>

class RetroChessNotify;

class ChessToaster : public QWidget
{
	Q_OBJECT
public:
	explicit ChessToaster(const RsPeerId &peerId, RetroChessNotify *notify,
	                      bool actionable = true);
	explicit ChessToaster(const RsGxsId &gxsId, RetroChessNotify *notify,
	                      bool actionable = true);

private slots:
	void acceptInvite();

private:
	void initialise(const QString &playerName, bool actionable);
	RsPeerId mPeerId;
	RsGxsId mGxsId;
	RetroChessNotify *mNotify;
	bool mActionable;
	Ui::ChessToaster ui;
};

#endif
