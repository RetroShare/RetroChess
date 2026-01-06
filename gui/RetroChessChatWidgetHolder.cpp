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


#include "interface/rsRetroChess.h"

#include "gui/chat/ChatWidget.h"

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

void RetroChessChatWidgetHolder::chessPressed()
{
	ChatId chatId = mChatWidget->getChatId();
	if (chatId.isDistantChatId()) {
		rsRetroChess->sendGxsInvite(chatId.toGxsId());
	} else {
		RsPeerId peer_id = chatId.toPeerId();

		if (rsRetroChess->hasInviteFrom(peer_id)){
			rsRetroChess->acceptedInvite(peer_id);
			mRetroChessNotify->notifyChessStart(peer_id);
			return;
		}

		rsRetroChess->sendInvite(peer_id);

		QString peerName = QString::fromUtf8(rsPeers->getPeerName(peer_id).c_str());
		mChatWidget->addChatMsg(true, tr("Chess Status"), QDateTime::currentDateTime(), QDateTime::currentDateTime()
	                        , tr("You're now inviting %1 to play Chess").arg(peerName), ChatWidget::MSGTYPE_SYSTEM);
	}

}

void RetroChessChatWidgetHolder::chessStart()
{
	ChatId chatId = mChatWidget->getChatId();
	if (chatId.isDistantChatId()) {
		rsRetroChess->acceptedInviteGxs(chatId.toDistantChatId());
	} else {
		RsPeerId peer_id = chatId.toPeerId();
		rsRetroChess->acceptedInvite(peer_id);
		mRetroChessNotify->notifyChessStart(peer_id);
	}
	return;
}

void RetroChessChatWidgetHolder::botMouseEnter()
{
	RSButtonOnText *source = qobject_cast<RSButtonOnText *>(QObject::sender());
	if (source)
	{
		source->setStyleSheet(QString("border: 1px solid #333333;")
		                      .append("font-size: 12pt; color: white;")
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
		                      .append("font-size: 12pt; color: white;")
		                      .append("min-width: 128px; min-height: 24px;")
		                      .append("border-radius: 6px;")
		                      .append("background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 0.67, "
		                              "stop: 0 #22c70d, stop: 1 #116a06);")

		                     );
		//source->setDown(false);
	}
}
