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
#include <QMessageBox>


#include "interface/rsRetroChess.h"

#include "gui/chat/ChatWidget.h"

#include "RetroChessChatWidgetHolder.h"

#include <retroshare/rsstatus.h>
#include <retroshare/rspeers.h>
#include <util/rsdebug.h>

#define IMAGE_RetroChess ":/images/chess.png"

RetroChessChatWidgetHolder::RetroChessChatWidgetHolder(ChatWidget *chatWidget, RetroChessNotify *notify)
	: QObject(), ChatWidgetHolder(chatWidget), mRetroChessNotify(notify)
{
	Q_INIT_RESOURCE(RetroChess_images); // <--- ASSURE LE CHARGEMENT DE L'ICONE

	RsDbg() << "CHESS: RetroChessChatWidgetHolder has been instantiated!" << std::endl;

	QIcon icon ;
	icon.addPixmap(QPixmap(IMAGE_RetroChess)) ;

	playChessButton = new QToolButton ;
	playChessButton->setIcon(icon) ;
	playChessButton->setToolTip(tr("Invite Friend to Chess"));
	playChessButton->setIconSize(QSize(28,28)) ;
	playChessButton->setAutoRaise(true) ;

	ChatId chatId = mChatWidget->getChatId();
	RsDbg() << "CHESS: chessnotify for " << chatId.toStdString() 
	        << " From: " << rsRetroChess->hasInviteFrom_chat(chatId)
	        << " To: " << rsRetroChess->hasInviteTo_chat(chatId);

	if (rsRetroChess->hasInviteFrom_chat(chatId))
	{
		// On évite les popups en boucle
		static ChatId lastInviteChatId;
		if (lastInviteChatId.toStdString() != chatId.toStdString()) {
			lastInviteChatId = chatId;
			
			QString buttonName = "Unknown GXS Friend";
			DistantChatPeerInfo info;
			if (rsChats->getDistantChatStatus(chatId.toDistantChatId(), info)) {
				buttonName = QString::fromUtf8(rsRetroChess->getGxsName(info.to_id).c_str());
			}

			// POPUP RADICALE pour garantir le clic
			QMessageBox::StandardButton reply;
			reply = QMessageBox::question(mChatWidget, tr("Chess Invitation"),
			                              tr("%1 is inviting you to a game of Chess. Accept?").arg(buttonName),
			                              QMessageBox::Yes|QMessageBox::No);
			
			if (reply == QMessageBox::Yes) {
				RsDbg() << "CHESS: User accepted via popup";
				chessStart();
			} else {
				RsDbg() << "CHESS: User rejected via popup";
			}
		}
		playChessButton->setEnabled(true); 
	}
	else if (rsRetroChess->hasInviteTo_chat(chatId))
	{
		RsDbg() << "CHESS: Button DISABLED (Waiting for opponent)";
		playChessButton->setText(tr("Invite Sent..."));
		playChessButton->setEnabled(false);
		playChessButton->setToolTip(tr("Waiting for oponent..."));
	}
	else
	{
		bool ready = rsRetroChess->isPeerReady_chat(chatId);
		RsDbg() << "CHESS: isPeerReady_chat for " << chatId.toStdString() << " returned " << (ready?"TRUE":"FALSE");
		playChessButton->setText(tr("Play Chess"));
		playChessButton->setEnabled(ready);
		playChessButton->setToolTip(ready ? tr("Play Chess") : tr("Chess handshake is only possible if a message has been previously sent to the friend. Checking..."));
	}

	mChatWidget->addTitleBarWidget(playChessButton); // <--- PLACE LE BOUTON EN HAUT
	connect(playChessButton, SIGNAL(clicked()), this, SLOT(chessPressed()));
	connect(notify, SIGNAL(chessInvited(RsPeerId)), this, SLOT(chessnotify(RsPeerId)));

	// Vérifier immédiatement s'il y a une invitation en attente à l'ouverture de la fenêtre
	chessnotify(mChatWidget->getChatId().toPeerId());

	// POKER le tunnel GXS immédiatement pour déclencher le handshake
	rsRetroChess->pokeTunnel_chat(mChatWidget->getChatId());
}

RetroChessChatWidgetHolder::~RetroChessChatWidgetHolder()
{

	button_map::iterator it = buttonMapTakeChess.begin();
	while (it != buttonMapTakeChess.end())
	{
		it = buttonMapTakeChess.erase(it);
	}
}

void RetroChessChatWidgetHolder::chessnotify(RsPeerId /*from_peer_id*/)
{
	ChatId chatId = mChatWidget->getChatId();
	
	if (rsRetroChess->hasInviteFrom_chat(chatId))
	{
		// On évite les popups en boucle
		static ChatId lastInviteChatId;
		if (lastInviteChatId.toStdString() != chatId.toStdString()) {
			lastInviteChatId = chatId;
			
			QString buttonName = "Unknown GXS Friend";
			DistantChatPeerInfo info;
			if (rsChats->getDistantChatStatus(chatId.toDistantChatId(), info)) {
				buttonName = QString::fromUtf8(rsRetroChess->getGxsName(info.to_id).c_str());
			}

			RsDbg() << "CHESS: Showing popup for " << buttonName.toStdString();
			// POPUP RADICALE pour garantir le clic
			QMessageBox::StandardButton reply;
			reply = QMessageBox::question(mChatWidget, tr("Chess Invitation"),
			                              tr("%1 is inviting you to a game of Chess. Accept?").arg(buttonName),
			                              QMessageBox::Yes|QMessageBox::No);
			
			if (reply == QMessageBox::Yes) {
				RsDbg() << "CHESS: User accepted via popup";
				chessStart();
			} else {
				RsDbg() << "CHESS: User rejected via popup";
			}
		}
		playChessButton->setText(tr("Accept Invite"));
		playChessButton->setEnabled(true); 
	}
	else if (rsRetroChess->hasInviteTo_chat(chatId))
	{
		RsDbg() << "CHESS: Button DISABLED (Waiting for opponent)";
		playChessButton->setText(tr("Invite Sent..."));
		playChessButton->setEnabled(false);
		playChessButton->setToolTip(tr("Waiting for oponent..."));
	}
	else
	{
		bool ready = rsRetroChess->isPeerReady_chat(chatId);
		RsDbg() << "CHESS: isPeerReady_chat for " << chatId.toStdString() << " returned " << (ready?"TRUE":"FALSE");
		playChessButton->setText(tr("Play Chess"));
		playChessButton->setEnabled(ready);
		playChessButton->setToolTip(ready ? tr("Play Chess") : tr("Chess handshake is only possible if a message has been previously sent to the friend. Checking..."));
	}
}

void RetroChessChatWidgetHolder::chessPressed()
{
	ChatId chatId = mChatWidget->getChatId();
	RsDbg() << "CHESS: chessPressed() for chatId: " << chatId.toStdString();

	if (rsRetroChess->hasInviteFrom_chat(chatId))
	{
		rsRetroChess->acceptedInvite_chat(chatId);
		// Note: we might need to handle RsPeerId vs RsGxsId in notification
		mRetroChessNotify->notifyChessStart(chatId.toPeerId(), 1); // Accept = Guest (1)
		return;
	}
	rsRetroChess->sendInvite_chat(chatId);

	QString peerName = chatId.isPeerId() ? 
		QString::fromUtf8(rsPeers->getPeerName(chatId.toPeerId()).c_str()) :
		tr("GXS Friend");

	mChatWidget->addChatMsg(true, tr("Chess Status"), QDateTime::currentDateTime(), QDateTime::currentDateTime()
	                        , tr("You're now inviting %1 to play Chess").arg(peerName), ChatWidget::MSGTYPE_SYSTEM);

}

void RetroChessChatWidgetHolder::chessStart()
{
	RsDbg() << "CHESS: UI Accept button clicked!";
	ChatId chatId = mChatWidget->getChatId();
	RsPeerId targetId = chatId.toPeerId();

	if (chatId.isDistantChatId()) {
		RsDbg() << "CHESS: Handling GXS invite for " << chatId.toStdString();
		DistantChatPeerInfo info;
		if (rsChats->getDistantChatStatus(chatId.toDistantChatId(), info)) {
			targetId = RsPeerId(info.to_id.toStdString());
			RsDbg() << "CHESS: Using pseudo-ID for GXS start: " << targetId;
		}
	}

	rsRetroChess->acceptedInvite_chat(chatId);
	mRetroChessNotify->notifyChessStart(targetId, 1); // Accept = Guest (1)
}

void RetroChessChatWidgetHolder::chessReject()
{
	RsDbg() << "CHESS: UI Reject button clicked!";
	// Pour l'instant on se contente de logger, on pourra ajouter rsRetroChess->rejectInvite_chat plus tard
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
		                      .append("padding: 3px;") // <--- AJOUT DU PADDING
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
		                      .append("padding: 3px;") // <--- AJOUT DU PADDING
		                      .append("background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 0.67, "
		                              "stop: 0 #22c70d, stop: 1 #116a06);")

		                     );
		//source->setDown(false);
	}
}
