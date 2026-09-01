/*******************************************************************************
 * gui/toaster/RetroChessToasterNotify.cpp                                     *
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

#include "RetroChessToasterNotify.h"

#include "ChessToaster.h"
#include "gui/RetroChessNotify.h"
#include "gui/SoundManager.h"
#include "gui/settings/rsharesettings.h"
#include "gui/toaster/ToasterItem.h"
#include <retroshare/rspeers.h>
#include <QMediaPlayer>
#include <QUrl>

namespace
{
const char *SETTINGS_GROUP = "RetroChess";
const char *SETTINGS_KEY = "ToasterNotifyInvites";
}

RetroChessToasterNotify::RetroChessToasterNotify(
        RetroChessNotify *notify, QObject *parent)
    : ToasterNotify(parent), mNotify(notify), mInviteSound(new QMediaPlayer(this))
{
	mInviteSound->setMedia(QUrl("qrc:/sound/ping.mp3"));
	connect(notify, &RetroChessNotify::chessInvited,
	        this, &RetroChessToasterNotify::chessInvited, Qt::QueuedConnection);
	connect(notify, &RetroChessNotify::chessInvitedGxs,
	        this, &RetroChessToasterNotify::chessInvitedGxs, Qt::QueuedConnection);
}

void RetroChessToasterNotify::playInviteSound()
{
	if (SoundManager::isMute()) return;
	mInviteSound->stop();
	mInviteSound->setPosition(0);
	mInviteSound->play();
}

bool RetroChessToasterNotify::hasSetting(QString &name)
{
	name = tr("Chess invitations");
	return true;
}

bool RetroChessToasterNotify::notifyEnabled()
{
	return Settings->valueFromGroup(
	        SETTINGS_GROUP, SETTINGS_KEY, true).toBool();
}

void RetroChessToasterNotify::setNotifyEnabled(bool enabled)
{
	Settings->setValueToGroup(SETTINGS_GROUP, SETTINGS_KEY, enabled);
	Settings->sync();
	if (!enabled) mPending.clear();
}

ToasterItem *RetroChessToasterNotify::toasterItem()
{
	if (mPending.isEmpty()) return nullptr;
	const Invitation invitation = mPending.takeFirst();
	ToasterItem *item = nullptr;
	if (!invitation.peerId.isNull())
		item = new ToasterItem(new ChessToaster(invitation.peerId, mNotify));
	else
		item = new ToasterItem(new ChessToaster(invitation.gxsId, mNotify));

	// Give the user enough time to react instead of applying RetroShare's
	// three-second default.
	item->timeToLive = 15000;
	return item;
}

ToasterItem *RetroChessToasterNotify::testToasterItem()
{
	return new ToasterItem(new ChessToaster(rsPeers->getOwnId(), mNotify, false));
}

void RetroChessToasterNotify::chessInvited(const RsPeerId &peerId)
{
	if (!notifyEnabled() || peerId.isNull()) return;
	Invitation invitation; invitation.peerId = peerId; mPending.push_back(invitation);
	playInviteSound();
}

void RetroChessToasterNotify::chessInvitedGxs(const RsGxsId &gxsId)
{
	if (!notifyEnabled() || gxsId.isNull()) return;
	Invitation invitation; invitation.gxsId = gxsId; mPending.push_back(invitation);
	playInviteSound();
}
