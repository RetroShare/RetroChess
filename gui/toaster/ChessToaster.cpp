/*******************************************************************************
 * gui/toaster/ChessToaster.cpp                                                *
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

#include "ChessToaster.h"

#include "gui/RetroChessNotify.h"
#include "interface/rsRetroChess.h"
#include <retroshare/rsidentity.h>
#include <retroshare/rspeers.h>

ChessToaster::ChessToaster(
        const RsPeerId &peerId, RetroChessNotify *notify, bool actionable)
    : QWidget(nullptr), mPeerId(peerId), mNotify(notify), mActionable(actionable)
{
	ui.setupUi(this);
	ui.avatarWidget->setId(ChatId(peerId));
	QString name = QString::fromUtf8(rsPeers->getPeerName(peerId).c_str());
	if (name.isEmpty()) name = tr("A contact");
	initialise(name, actionable);
}

ChessToaster::ChessToaster(
        const RsGxsId &gxsId, RetroChessNotify *notify, bool actionable)
    : QWidget(nullptr), mGxsId(gxsId), mNotify(notify), mActionable(actionable)
{
	ui.setupUi(this);
	ui.avatarWidget->setGxsId(gxsId);
	QString name;
	RsIdentityDetails details;
	if (rsIdentity && rsIdentity->getIdDetails(gxsId, details))
		name = QString::fromUtf8(details.mNickname.c_str());
	if (name.isEmpty()) name = QString::fromStdString(gxsId.toStdString()).left(8);
	if (name.isEmpty()) name = tr("An identity");
	initialise(name, actionable);
}

void ChessToaster::initialise(const QString &playerName, bool actionable)
{
	ui.avatarWidget->setFrameType(AvatarWidget::NO_FRAME);
	ui.avatarWidget->setDefaultAvatar(":/images/chess-notify.png");
	ui.toasterLabel->setText(tr("Chess invitation"));
	ui.textLabel->setText(tr("%1 is inviting you to play chess.").arg(playerName));
	ui.toasterButton->setText(
	        actionable ? tr("Accept chess invite") : tr("Close preview"));
	ui.toasterButton->setEnabled(true);
	if (actionable) {
		ui.toasterButton->setStyleSheet(
		        "QPushButton {"
		        " border: 1px solid #199909;"
		        " font-size: 12pt; color: white;"
		        " min-height: 24px;"
		        " border-radius: 6px;"
		        " background-color: qlineargradient("
		        " x1: 0, y1: 0, x2: 0, y2: 0.67,"
		        " stop: 0 #22c70d, stop: 1 #116a06);"
		        "}"
		        "QPushButton:hover { border-color: #35d51f; }"
		        "QPushButton:pressed { background-color: #116a06; }");
	}
	connect(ui.toasterButton, &QPushButton::clicked,
	        this, &ChessToaster::acceptInvite);
	connect(ui.closeButton, &QPushButton::clicked, this, &QWidget::hide);
}

void ChessToaster::acceptInvite()
{
	if (!mActionable) {
		hide();
		return;
	}
	if (!rsRetroChess || !mNotify) return;
	if (!mPeerId.isNull()) {
		if (!rsRetroChess->hasInviteFrom(mPeerId)) return;
		rsRetroChess->acceptedInvite(mPeerId);
		mNotify->notifyChessStart(mPeerId);
	} else if (!mGxsId.isNull()) {
		if (!rsRetroChess->hasInviteFromGxs(mGxsId)) {
			hide();
			return;
		}
		rsRetroChess->acceptedInviteGxs(mGxsId);
		mNotify->notifyChessStartGxs(mGxsId);
	} else return;
	ui.toasterButton->setEnabled(false);
	hide();
}
