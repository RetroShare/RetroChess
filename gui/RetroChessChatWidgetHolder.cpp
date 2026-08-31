/*******************************************************************************
 * gui/RetroChessChatWidgetHolder.cpp                                          *
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

#include <QToolButton>
#include <QPropertyAnimation>
#include <QIcon>
#include <QLayout>
#include <QTimer>
#include <QPointer>
#include <QTextEdit>


#include "interface/rsRetroChess.h"

#include "gui/chat/ChatWidget.h"
#include <retroshare/rschats.h>

#include "RetroChessChatWidgetHolder.h"

#include <retroshare/rsidentity.h>
#include <retroshare/rsstatus.h>
#include <retroshare/rspeers.h>

#define IMAGE_RetroChess ":/images/chess.png"

RetroChessChatWidgetHolder::RetroChessChatWidgetHolder(ChatWidget *chatWidget, RetroChessNotify *notify)
	: QObject(), ChatWidgetHolder(chatWidget), mRetroChessNotify(notify)
{
	QIcon icon ;
	icon.addPixmap(QPixmap(IMAGE_RetroChess)) ;

	playChessButton = new QToolButton ;
	playChessButton->setIcon(icon) ;
	playChessButton->setToolTip(tr("Invite Friend to Chess"));
	playChessButton->setIconSize(QSize(28,28)) ;
	playChessButton->setAutoRaise(true) ;

	mChatWidget->addChatBarWidget(playChessButton);
	connect(playChessButton, SIGNAL(clicked()), this, SLOT(chessPressed()));
	connect(notify, SIGNAL(chessInvited(RsPeerId)), this, SLOT(chessnotify(RsPeerId)));
	connect(notify, SIGNAL(chessInvitedGxs(RsGxsId)), this, SLOT(chessnotifyGxs(RsGxsId)));
	// When the p3RetroChess service detects the tunnel is CONNECTED, 
	// it calls notifyGxsTunnelReady, which emits this signal:
	connect(notify, SIGNAL(gxsTunnelReady(RsGxsId)), this, SLOT(handleGxsTunnelReady(RsGxsId)));
	connect(notify, SIGNAL(gxsTunnelClosed(RsGxsId)), this, SLOT(handleGxsTunnelClosed(RsGxsId)));
	connect(notify, SIGNAL(chessPlayerLeftGxs(RsGxsId)), this, SLOT(handleChessPlayerLeftGxs(RsGxsId)));

	// A GXS invite can arrive before this holder is constructed. Recover it from
	// the service's persistent invite state once the chat metadata is available.
	QTimer::singleShot(0, this, [this]() { recoverPendingGxsInvite(10); });

}

RetroChessChatWidgetHolder::~RetroChessChatWidgetHolder()
{

	button_map::iterator it = buttonMapTakeChess.begin();
	while (it != buttonMapTakeChess.end())
	{
		it = buttonMapTakeChess.erase(it);
	}
}

void RetroChessChatWidgetHolder::chessnotify(RsPeerId from_peer_id)
{
	RsPeerId peer_id =  mChatWidget->getChatId().toPeerId();//TODO support GXSID
	//if (peer_id!=from_peer_id)return;//invite from another chat
	if (rsRetroChess->hasInviteFrom(peer_id))
	{
		if (mChatWidget)
		{
			QString buttonName = QString::fromUtf8(rsPeers->getPeerName(peer_id).c_str());
			if (buttonName.isEmpty()) buttonName = "Chess";//TODO maybe change all with GxsId
			//disable old buttons
			button_map::iterator it = buttonMapTakeChess.begin();
			while (it != buttonMapTakeChess.end())
			{
				it = buttonMapTakeChess.erase(it);
			}
			//button_map::iterator it = buttonMapTakeChess.find(buttonName);
			//if (it == buttonMapTakeChess.end()){
			mChatWidget->addChatMsg(true, tr("Chess Status"), QDateTime::currentDateTime(), QDateTime::currentDateTime()
			                        , tr("%1 inviting you to start Chess. Do you want to accept or decline the invitation?").arg(buttonName), ChatWidget::MSGTYPE_SYSTEM);
			RSButtonOnText *button = mChatWidget->getNewButtonOnTextBrowser(tr("Accept"));
			button->setToolTip(tr("Accept"));
			button->setStyleSheet(QString("border: 1px solid #199909;")
			                      .append("font-size: 12pt;  color: white;")
			                      .append("min-width: 128px; min-height: 24px;")
			                      .append("border-radius: 6px;")
			                      .append("background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 0.67, "
			                              "stop: 0 #22c70d, stop: 1 #116a06);")

			                     );

			button->updateImage();

			connect(button,SIGNAL(clicked()),this,SLOT(chessStart()));
			connect(button,SIGNAL(mouseEnter()),this,SLOT(botMouseEnter()));
			connect(button,SIGNAL(mouseLeave()),this,SLOT(botMouseLeave()));

			buttonMapTakeChess.insert(buttonName, button);
			//}
		}


	}
}

void RetroChessChatWidgetHolder::chessnotifyGxs(const RsGxsId &from_gxs_id)
{
    // A tunnel can deliver the first invite more than once while it is being
    // established. Keep the marker until the invite is accepted or the game
    // ends, otherwise every duplicate signal creates another chat button.
    showGxsInviteIfMatching(from_gxs_id, 10);
}

void RetroChessChatWidgetHolder::recoverPendingGxsInvite(int retriesLeft)
{
    ChatId chatId = mChatWidget->getChatId();
    if (!chatId.isDistantChatId()) return;

    DistantChatPeerInfo info;
    if (!rsChats->getDistantChatStatus(chatId.toDistantChatId(), info) || info.to_id.isNull()) {
        if (retriesLeft > 0) {
            QTimer::singleShot(250, this, [this, retriesLeft]() {
                recoverPendingGxsInvite(retriesLeft - 1);
            });
        }
        return;
    }

    if (rsRetroChess->hasInviteFromGxs(info.to_id))
        showGxsInviteIfMatching(info.to_id, 0);
}

void RetroChessChatWidgetHolder::showGxsInviteIfMatching(const RsGxsId &from_gxs_id, int retriesLeft)
{
    ChatId chatId = mChatWidget->getChatId();

    // Only handle distant (GXS) chats
    if (!chatId.isDistantChatId()) {
        return;
    }

    // Verify the invite is specifically for THIS chat window.
    // Each distant chat window has a unique tunnel — check the remote GXS ID matches.
    DistantChatPeerInfo dcpinfo;
    if (!rsChats->getDistantChatStatus(chatId.toDistantChatId(), dcpinfo)) {
        if (retriesLeft > 0) {
            QTimer::singleShot(250, this, [this, from_gxs_id, retriesLeft]() {
                showGxsInviteIfMatching(from_gxs_id, retriesLeft - 1);
            });
        }
        return;
    }
    if (dcpinfo.to_id != from_gxs_id) {
        return; // This invite is for a different chat window
    }

    if (!mChatWidget) {
        return;
    }
    if (!rsRetroChess->hasInviteFromGxs(from_gxs_id)
        || displayedGxsInvites.find(from_gxs_id) != displayedGxsInvites.end()) {
        return;
    }
    displayedGxsInvites.insert(from_gxs_id);

    // Get a display name for the button
    QString buttonName = QString::fromStdString(from_gxs_id.toStdString()).left(8);

    // Clear any old accept buttons to avoid duplicates
    button_map::iterator it = buttonMapTakeChess.begin();
    while (it != buttonMapTakeChess.end()) {
        it = buttonMapTakeChess.erase(it);
    }

    // Add the system message and the "Accept" button to the chat history
    mChatWidget->addChatMsg(true, tr("Chess Status"), QDateTime::currentDateTime(), QDateTime::currentDateTime(),
                            tr("%1 is inviting you to play chess. Do you want to accept?\n").arg(buttonName),
                            ChatWidget::MSGTYPE_SYSTEM);

    RSButtonOnText *button = mChatWidget->getNewButtonOnTextBrowser(tr("Accept chess invite"));
    button->setToolTip(tr("Accept chess invite"));
	QPointer<QTextEdit> chatTextEdit(qobject_cast<QTextEdit*>(button->parentWidget()));
	// ChatWidget inserts RSButtonOnText immediately, before our stylesheet is
	// applied. Remove that initial small image and append it again after styling,
	// so the document's image rectangle and clickable hit area have the same size.
	if (chatTextEdit)
		button->clear();

    button->setStyleSheet(QString("border: 1px solid #199909;")
                          .append("font-size: 12pt; color: white; padding-left: 10px; padding-right: 10px;")
                          .append("min-width: 128px; min-height: 24px;")
                          .append("border-radius: 6px;")
                          .append("background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 0.67, "
                                  "stop: 0 #22c70d, stop: 1 #116a06);"));

	if (chatTextEdit)
		button->appendToText(chatTextEdit);
	else
		button->updateImage();

	// RSButtonOnText is rendered as an image inside the chat QTextEdit. Force a
	// second layout pass after insertion; otherwise Qt may keep the old image
	// geometry until the user manually resizes the chat window.
	QPointer<RSButtonOnText> guardedButton(button);
	if (chatTextEdit) {
		QTimer::singleShot(0, chatTextEdit.data(), [guardedButton, chatTextEdit]() {
			if (!guardedButton || !chatTextEdit) return;
			guardedButton->updateImage();
			chatTextEdit->document()->markContentsDirty(
			        0, chatTextEdit->document()->characterCount());
			chatTextEdit->viewport()->update();
		});
	}

    connect(button, SIGNAL(clicked()),    this, SLOT(chessStart()));
    connect(button, SIGNAL(mouseEnter()), this, SLOT(botMouseEnter()));
    connect(button, SIGNAL(mouseLeave()), this, SLOT(botMouseLeave()));

    buttonMapTakeChess.insert(buttonName, button);
}


void RetroChessChatWidgetHolder::chessPressed()
{
	ChatId chatId = mChatWidget->getChatId();
	QString peerName;
	if (chatId.isDistantChatId()) {
		// sendInvite_chat() handles everything:
		// - if the GXS tunnel is ready: requests tunnel + queues invite immediately
		// - if not ready yet: stores chatId and retries automatically on each tick()
		// A first distant-chat packet is also needed to create the chat window on
		// the remote side. Send it automatically so the user does not need to type
		// an unrelated message before the Accept button can be displayed.
		rsChats->sendChat(chatId, tr("RetroChess invitation is being prepared...").toStdString());
		const bool queued = rsRetroChess->sendInvite_chat(chatId);
		if (!queued) {
			mChatWidget->addChatMsg(true, tr("RetroChess"), QDateTime::currentDateTime(),
			                        QDateTime::currentDateTime(),
			                        tr("Chess invite could not be sent. Please wait for the connection and try again."),
			                        ChatWidget::MSGTYPE_SYSTEM);
			return;
		}

		DistantChatPeerInfo dcpinfo;
		if (rsChats->getDistantChatStatus(chatId.toDistantChatId(), dcpinfo)
		    && !dcpinfo.to_id.isNull()) {
			peerName = QString::fromStdString(dcpinfo.to_id.toStdString()).left(8);
			mChatWidget->addChatMsg(true, tr("RetroChess"), QDateTime::currentDateTime(),
			                        QDateTime::currentDateTime(),
			                        tr("Chess invite queued for %1 — will send when connection is ready.").arg(peerName),
			                        ChatWidget::MSGTYPE_SYSTEM);
		} else {
			// Tunnel not up yet — service will retry automatically
			mChatWidget->addChatMsg(true, tr("RetroChess"), QDateTime::currentDateTime(),
			                        QDateTime::currentDateTime(),
			                        tr("Chess invite queued — connecting to friend, will send automatically..."),
			                        ChatWidget::MSGTYPE_SYSTEM);
		}
		return;
	} else {
		RsPeerId peer_id = chatId.toPeerId();

		if (rsRetroChess->hasInviteFrom(peer_id)){
			rsRetroChess->acceptedInvite(peer_id);
			mRetroChessNotify->notifyChessStart(peer_id);
			return;
		}

		rsRetroChess->sendInvite(peer_id);

		peerName = QString::fromUtf8(rsPeers->getPeerName(peer_id).c_str());
	}
	mChatWidget->addChatMsg(true, tr("Chess Status"), QDateTime::currentDateTime(), QDateTime::currentDateTime()
	                        , tr("You're now inviting %1 to play Chess").arg(peerName), ChatWidget::MSGTYPE_SYSTEM);
}


void RetroChessChatWidgetHolder::chessStart()
{
	ChatId chatId = mChatWidget->getChatId();
	if (chatId.isDistantChatId()) {
		// Remote side accepting an invite:
		// The tunnel is already open (the invite arrived over it).
		// Call acceptedInviteGxs() which will send chess_accept over the existing tunnel.
		DistantChatPeerInfo dcpinfo;
		if (!rsChats->getDistantChatStatus(chatId.toDistantChatId(), dcpinfo)) {
			std::cerr << "RetroChess: Failed to resolve distant chat status" << std::endl;
			return;
		}
		RsGxsId remoteGxsId = dcpinfo.to_id;

		rsRetroChess->acceptedInviteGxs(remoteGxsId);
		displayedGxsInvites.erase(remoteGxsId);

		// Open the chess window immediately — we're the server side, tunnel is already up
		mRetroChessNotify->notifyChessStartGxs(remoteGxsId);
		if (playChessButton) playChessButton->hide();

	} else {
		RsPeerId peer_id = chatId.toPeerId();
		rsRetroChess->acceptedInvite(peer_id);
		mRetroChessNotify->notifyChessStart(peer_id);
	}
	return;
}


void RetroChessChatWidgetHolder::handleGxsTunnelReady(const RsGxsId &gxs_id)
{
    ChatId chatId = mChatWidget->getChatId();
    if (chatId.isDistantChatId()) {
        // Verify this tunnel is for the current chat
        DistantChatPeerInfo dcpinfo;
        if (rsChats->getDistantChatStatus(chatId.toDistantChatId(), dcpinfo)) {
            if (dcpinfo.to_id == gxs_id) {
                // Transport readiness only means the invite can be delivered.
                // The game starts only after a chess_accept message is received.
            }
        }
    }
}

void RetroChessChatWidgetHolder::handleGxsTunnelClosed(const RsGxsId &gxs_id)
{
    ChatId chatId = mChatWidget->getChatId();
    if (!chatId.isDistantChatId())
        return;

    // Check that this closure is for the remote peer of THIS chat window
    DistantChatPeerInfo dcpinfo;
    if (rsChats->getDistantChatStatus(chatId.toDistantChatId(), dcpinfo)) {
        if (dcpinfo.to_id != gxs_id)
            return; // Belongs to a different chat window
    }

    // Restore the button so the user can start a new game
    if (playChessButton) {
        playChessButton->show();
    }

    if (mChatWidget) {
        mChatWidget->addChatMsg(true, tr("RetroChess"),
                                QDateTime::currentDateTime(),
                                QDateTime::currentDateTime(),
                                tr("Connection to chess partner was lost. You can invite them again."),
                                ChatWidget::MSGTYPE_SYSTEM);
    }
}

void RetroChessChatWidgetHolder::handleChessPlayerLeftGxs(const RsGxsId &gxs_id)
{
	ChatId chatId = mChatWidget->getChatId();
	if (!chatId.isDistantChatId()) return;

	DistantChatPeerInfo info;
	if (!rsChats->getDistantChatStatus(chatId.toDistantChatId(), info)
	    || info.to_id != gxs_id)
		return;
	displayedGxsInvites.erase(gxs_id);
	if (playChessButton) playChessButton->show();

	QString playerName = QString::fromStdString(gxs_id.toStdString()).left(8);
	RsIdentityDetails details;
	if (rsIdentity->getIdDetails(gxs_id, details) && !details.mNickname.empty())
		playerName = QString::fromUtf8(details.mNickname.c_str());

	mChatWidget->addChatMsg(
	        true, tr("Chess Status"), QDateTime::currentDateTime(),
	        QDateTime::currentDateTime(),
	        tr("%1 left the chess game. The game has ended.").arg(playerName),
	        ChatWidget::MSGTYPE_SYSTEM);
}


void RetroChessChatWidgetHolder::botMouseEnter()
{
	RSButtonOnText *source = qobject_cast<RSButtonOnText *>(QObject::sender());
	if (source)
	{
		source->setStyleSheet(QString("border: 1px solid #333333;")
		                      .append("font-size: 12pt; color: white; padding-left: 10px; padding-right: 10px;")
		                      .append("min-width: 128px; min-height: 24px;")
		                      .append("border-radius: 6px;")
		                      .append("background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 0.67, "
		                              "stop: 0 #444444, stop: 1 #222222);")

		                     );
		//source->setDown(true);
	}
}

void RetroChessChatWidgetHolder::botMouseLeave()
{
	RSButtonOnText *source = qobject_cast<RSButtonOnText *>(QObject::sender());
	if (source)
	{
		source->setStyleSheet(QString("border: 1px solid #199909;")
		                      .append("font-size: 12pt; color: white; padding-left: 10px; padding-right: 10px;")
		                      .append("min-width: 128px; min-height: 24px;")
		                      .append("border-radius: 6px;")
		                      .append("background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 0.67, "
		                              "stop: 0 #22c70d, stop: 1 #116a06);")

		                     );
		//source->setDown(false);
	}
}
