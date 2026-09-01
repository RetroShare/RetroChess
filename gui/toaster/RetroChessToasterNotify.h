/*******************************************************************************
 * gui/toaster/RetroChessToasterNotify.h                                       *
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

#ifndef RETROCHESSTOASTERNOTIFY_H
#define RETROCHESSTOASTERNOTIFY_H

#include "gui/common/ToasterNotify.h"
#include <retroshare/rstypes.h>
#include <QList>

class RetroChessNotify;
class QMediaPlayer;

class RetroChessToasterNotify : public ToasterNotify
{
	Q_OBJECT
public:
	explicit RetroChessToasterNotify(RetroChessNotify *notify, QObject *parent = nullptr);
	bool hasSetting(QString &name) override;
	bool notifyEnabled() override;
	void setNotifyEnabled(bool enabled) override;
	ToasterItem *toasterItem() override;
	ToasterItem *testToasterItem() override;

private slots:
	void chessInvited(const RsPeerId &peerId);
	void chessInvitedGxs(const RsGxsId &gxsId);

private:
	struct Invitation { RsPeerId peerId; RsGxsId gxsId; };
	RetroChessNotify *mNotify;
	QMediaPlayer *mInviteSound;
	QList<Invitation> mPending;
	void playInviteSound();
};

#endif
