/*******************************************************************************
 * gui/NEMainpage.cpp                                                          *
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

#include "NEMainpage.h"
#include "ui_NEMainpage.h"

#include "services/p3RetroChess.h"
#include "interface/rsRetroChess.h"
#include "services/rsRetroChessItems.h"
//#include "gui/notifyqt.h"
#include <qjsondocument.h>

#include <iostream>
#include <algorithm>
#include <string>
#include <QTime>
#include <QMenu>
#include <QMessageBox>
#include <QToolButton>
#include <QTimer>
#include <QShowEvent>

#include "gui/RetroChessSettings.h"
#include "gui/RetroChessUserNotify.h"

#include "gui/chat/ChatDialog.h"
#include <retroshare/rsidentity.h>


NEMainpage::NEMainpage(QWidget *parent, RetroChessNotify *notify) :
	MainPage(parent),
	ui(new Ui::NEMainpage),
	mNotify(notify),
	mOfficialLobbyTimer(new QTimer(this)),
	mOfficialLobbyDialog(nullptr),
	mLobbyUnreadCount(0)
{
	ui->setupUi(this);
	setupMenuActions();

	connect(mNotify, SIGNAL(NeMsgArrived(RsPeerId,QString)), this, SLOT(NeMsgArrived(RsPeerId,QString)));
	connect(mNotify, SIGNAL(chessStart(RsPeerId)), this, SLOT(chessStart(RsPeerId)));
	// The inviter is White; the participant who accepts is Black.
	connect(mNotify, SIGNAL(chessStartGxs(RsGxsId)), this, SLOT(chessStartGxsAsBlack(RsGxsId)));
	connect(mNotify, SIGNAL(chessAcceptedGxs(RsGxsId)), this, SLOT(chessStartGxs(RsGxsId)));
	connect(mNotify, SIGNAL(chessMoveGxs(RsGxsId,int,int,int)), this, SLOT(chessMoveGxs(RsGxsId,int,int,int)));
	connect(mNotify, SIGNAL(chessPlayerLeftGxs(RsGxsId)), this, SLOT(chessPlayerLeftGxs(RsGxsId)));
	connect(mNotify, SIGNAL(chessRematchGxs(RsGxsId,int)), this, SLOT(chessRematchGxs(RsGxsId,int)));
	connect(mNotify, SIGNAL(chessGameActionGxs(RsGxsId,QString)), this, SLOT(chessGameActionGxs(RsGxsId,QString)));

	connect(mOfficialLobbyTimer, SIGNAL(timeout()), this, SLOT(autoJoinOfficialLobby()));
	mOfficialLobbyTimer->setInterval(15000);
	mOfficialLobbyTimer->start();
	QTimer::singleShot(0, this, SLOT(autoJoinOfficialLobby()));

}

NEMainpage::~NEMainpage()
{
	delete ui;
}

UserNotify *NEMainpage::createUserNotify(QObject *parent)
{
	return new RetroChessUserNotify(this, parent);
}

namespace
{
const ChatLobbyId OFFICIAL_RETROCHESS_LOBBY_ID = 0x0174BD3E49231CDAULL;
}

void NEMainpage::autoJoinOfficialLobby()
{
	std::list<ChatLobbyId> subscribedLobbies;
	rsChats->getChatLobbyList(subscribedLobbies);
	if (std::find(subscribedLobbies.begin(), subscribedLobbies.end(),
	              OFFICIAL_RETROCHESS_LOBBY_ID) != subscribedLobbies.end()) {
		rsChats->setLobbyAutoSubscribe(OFFICIAL_RETROCHESS_LOBBY_ID, true);
		ui->officialLobbyStatus->setText(tr("Connected to the official RetroChess lobby."));
		mOfficialLobbyTimer->stop();
		showOfficialLobby();
		return;
	}

	std::vector<VisibleChatLobbyRecord> visibleLobbies;
	rsChats->getListOfNearbyChatLobbies(visibleLobbies);
	bool found = false;
	for (const VisibleChatLobbyRecord &lobby : visibleLobbies)
		if (lobby.lobby_id == OFFICIAL_RETROCHESS_LOBBY_ID) {
			found = true;
			break;
		}

	if (!found) {
		ui->officialLobbyStatus->setText(
		        tr("Searching for official lobby 0174BD3E49231CDA…"));
		return;
	}

	RsGxsId joinIdentity;
	rsChats->getDefaultIdentityForChatLobby(joinIdentity);
	RsIdentityDetails details;
	if (joinIdentity.isNull() || !rsIdentity->getIdDetails(joinIdentity, details)
	    || !(details.mFlags & RS_IDENTITY_FLAGS_PGP_LINKED)) {
		std::list<RsGxsId> ownIds;
		rsIdentity->getOwnIds(ownIds);
		for (const RsGxsId &id : ownIds)
			if (rsIdentity->getIdDetails(id, details)
			    && (details.mFlags & RS_IDENTITY_FLAGS_PGP_LINKED)) {
				joinIdentity = id;
				break;
			}
	}

	if (joinIdentity.isNull() || !(details.mFlags & RS_IDENTITY_FLAGS_PGP_LINKED)) {
		ui->officialLobbyStatus->setText(
		        tr("A PGP-linked GXS identity is required to join this lobby."));
		return;
	}

	if (rsChats->joinVisibleChatLobby(OFFICIAL_RETROCHESS_LOBBY_ID, joinIdentity)) {
		rsChats->setLobbyAutoSubscribe(OFFICIAL_RETROCHESS_LOBBY_ID, true);
		ui->officialLobbyStatus->setText(tr("Connected to the official RetroChess lobby."));
		mOfficialLobbyTimer->stop();
		showOfficialLobby();
	} else {
		ui->officialLobbyStatus->setText(tr("The official lobby was found, but joining failed. Retrying…"));
	}
}

void NEMainpage::showOfficialLobby()
{
	if (mOfficialLobbyDialog)
		return;

	mOfficialLobbyDialog = ChatDialog::getChat(
	        ChatId(OFFICIAL_RETROCHESS_LOBBY_ID), RsChatFlags::RS_CHAT_OPEN);
	if (!mOfficialLobbyDialog) {
		ui->officialLobbyStatus->setText(tr("The official lobby could not be opened."));
		return;
	}

	mOfficialLobbyDialog->setParent(ui->embeddedLobbyContainer);
	ui->embeddedLobbyLayout->addWidget(mOfficialLobbyDialog);
	mOfficialLobbyDialog->addToParent(ui->embeddedLobbyContainer);
	mOfficialLobbyDialog->show();
	connect(mOfficialLobbyDialog->getChatWidget(), SIGNAL(newMessage(ChatWidget*)),
	        this, SLOT(officialLobbyNewMessage(ChatWidget*)), Qt::UniqueConnection);
	ui->officialLobbyTitle->hide();
	ui->officialLobbyDescription->hide();
	ui->officialLobbyStatus->hide();
	ui->officialLobbyTopSpacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
	ui->officialLobbyBottomSpacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
}

void NEMainpage::officialLobbyNewMessage(ChatWidget *)
{
	if (isVisible())
		return;
	++mLobbyUnreadCount;
	emit lobbyUnreadCountChanged();
}

void NEMainpage::showEvent(QShowEvent *event)
{
	if (mLobbyUnreadCount) {
		mLobbyUnreadCount = 0;
		emit lobbyUnreadCountChanged();
	}
	MainPage::showEvent(event);
}

void NEMainpage::chessStart(const RsPeerId &peer_id)
{
	// This signal is emitted on the participant who accepted the invitation.
	// The participant is Black; the inviter is White.
	create_chess_window(peer_id.toStdString(), 1);
}

void NEMainpage::chessStartGxs(const RsGxsId &gxs_id)
{
	// The remote participant accepted our invitation: we are White and move first.
	create_chess_window_gxs(gxs_id, 0);
}

void NEMainpage::chessStartGxsAsBlack(const RsGxsId &gxs_id)
{
	// We accepted the remote participant's invitation: we are Black.
	create_chess_window_gxs(gxs_id, 1);
}

void NEMainpage::chessMoveGxs(const RsGxsId &gxs_id, int col, int row, int count)
{
	std::string key = gxs_id.toStdString();
	if (activeGames.find(key) != activeGames.end()) {
		RetroChessWindow* rcw = activeGames.value(key);
		if (rcw->m_flag_finished == 0)
			rcw->validate_tile(row, col, count);
	} else {
		std::cerr << "RetroChess: Received GXS move but no active game for " << key << std::endl;
	}
}

void NEMainpage::chessPlayerLeftGxs(const RsGxsId &gxs_id)
{
	const std::string key = gxs_id.toStdString();
	if (activeGames.find(key) != activeGames.end()) {
		activeGames.value(key)->showPlayerLeaveMsg();
		removeActiveGame(QString::fromStdString(key));
	}
}

void NEMainpage::removeActiveGame(QString gameId)
{
	const std::string key = gameId.toStdString();
	activeGames.remove(key);
	removeActiveGameListing(gameId);
}

void NEMainpage::removeActiveGameListing(QString gameId)
{
	for (int row = ui->active_games->count() - 1; row >= 0; --row) {
		if (ui->active_games->item(row)->text() == gameId)
			delete ui->active_games->takeItem(row);
	}
}

void NEMainpage::requestRematchGxs(const RsGxsId &gxs_id, int localColor)
{
	if (!rsRetroChess->sendRematchGxs(gxs_id, localColor))
		return;

	const std::string key = gxs_id.toStdString();
	if (activeGames.contains(key)) {
		RetroChessWindow *window = activeGames.value(key);
		window->m_rematchRequested = true;
		window->showGameStatus(tr("Waiting for opponent to accept rematch"));
	}
}

void NEMainpage::chessRematchGxs(const RsGxsId &gxs_id, int remoteColor)
{
	const std::string key = gxs_id.toStdString();
	if (!activeGames.contains(key)) {
		rsRetroChess->sendGameActionGxs(gxs_id, "rematch_decline");
		return;
	}
	RetroChessWindow *window = activeGames.value(key);
	const bool alreadyRequested = window->m_rematchRequested;
	if (!alreadyRequested && QMessageBox::question(
	        window, tr("Rematch"), tr("Your opponent requests a rematch. Accept?")) != QMessageBox::Yes) {
		rsRetroChess->sendGameActionGxs(gxs_id, "rematch_decline");
		return;
	}
	if (!alreadyRequested)
		rsRetroChess->sendRematchGxs(gxs_id, window->m_localplayer_turn);
	window->closeForRematch();
	const int localColor = remoteColor == 0 ? 1 : 0;
	create_chess_window_gxs(gxs_id, localColor == 0 ? 1 : 0);
}

void NEMainpage::chessGameActionGxs(const RsGxsId &gxs_id, QString action)
{
	const std::string key = gxs_id.toStdString();
	if (activeGames.contains(key))
		activeGames.value(key)->applyGameAction(action, true);
}

// decode received message here
void NEMainpage::NeMsgArrived(const RsPeerId &peer_id, QString str)
{
	QJsonDocument jdoc = QJsonDocument::fromJson(str.toUtf8());
	QVariantMap vmap = jdoc.toVariant().toMap();
	std::cout << "GUI got Packet from: " << peer_id;
	std::cout << " saying " << str.toStdString();
	std::cout << std::endl;
	QString type = vmap.value("type").toString();
	if (type == "chessclick")
	{
		int row = vmap.value("row").toInt();
		int col = vmap.value("col").toInt();
		int count = vmap.value("count").toInt();
		RetroChessWindow* rcw = activeGames.value(peer_id.toStdString(), nullptr);
		if (rcw && rcw->m_flag_finished == 0)
			rcw->validate_tile(row,col,count);
	}
    else if(type == "player_status_message")
    {
        // show player left message
		if( activeGames.find(peer_id.toStdString()) != activeGames.end())	// check has active games
		{
			RetroChessWindow* rcw = activeGames.value(peer_id.toStdString());
			QString status_str = vmap.value("player_status").toString();

			if( status_str == "leave")
			{
				rcw->showPlayerLeaveMsg();
				removeActiveGame(QString::fromStdString(peer_id.toStdString()));
			}
		}
    }
	else if (type == "chess_init")
    {
        create_chess_window(peer_id.toStdString(), 1);
    }
	else if (type == "chess_invite")
	{
		ChatDialog::chatFriend(ChatId(peer_id));
		rsRetroChess->gotInvite(peer_id);
		mNotify->notifyChessInvite(peer_id);
	}
	else if (type == "chess_accept")
	{
		if (rsRetroChess->hasInviteTo(peer_id))
		{
			// We sent the invitation, so we are White and move first.
			rsRetroChess->clearInvite(peer_id);
			create_chess_window(peer_id.toStdString(), 0);
		}
	}
	else if (type == "chess_rematch")
	{
		const std::string key = peer_id.toStdString();
		if (!activeGames.contains(key)) {
			QVariantMap decline;
			decline.insert("type", "game_action");
			decline.insert("action", "rematch_decline");
			rsRetroChess->qvm_msg_peer(peer_id, decline);
			return;
		}
		RetroChessWindow *window = activeGames.value(key);
		const bool alreadyRequested = window->m_rematchRequested;
		if (!alreadyRequested && QMessageBox::question(
		        window, tr("Rematch"), tr("Your opponent requests a rematch. Accept?")) != QMessageBox::Yes) {
			QVariantMap decline;
			decline.insert("type", "game_action");
			decline.insert("action", "rematch_decline");
			rsRetroChess->qvm_msg_peer(peer_id, decline);
			return;
		}
		if (!alreadyRequested) {
			QVariantMap accept;
			accept.insert("type", "chess_rematch");
			accept.insert("color", window->m_localplayer_turn);
			rsRetroChess->qvm_msg_peer(peer_id, accept);
		}
		window->closeForRematch();
		const int localColor = vmap.value("color").toInt() == 0 ? 1 : 0;
		create_chess_window(key, localColor == 0 ? 1 : 0);
	}
	else if (type == "game_action")
	{
		const std::string key = peer_id.toStdString();
		if (activeGames.contains(key))
			activeGames.value(key)->applyGameAction(vmap.value("action").toString(), true);
	}
	// else if( type == "chess_reject") // other player rejected your invite (need to be finish)
	else
	{
		QString output = QTime::currentTime().toString() +" ";
		output+= QString::fromStdString(rsPeers->getPeerName(peer_id));
		output+=": ";
		output+=str;
		ui->netLogWidget->addItem(output);
	}

	/*
	{
		QString output = QTime::currentTime().toString() +" ";
		output+= QString::fromStdString(rsPeers->getPeerName(peer_id));
		output+=": ";
		output+=str;
		ui->netLogWidget->addItem(output);
	}
	*/
}

void NEMainpage::create_chess_window(std::string peer_id, int player_id)
{
	RetroChessWindow *rcw = new RetroChessWindow(peer_id, player_id);
	connect(rcw, SIGNAL(rematchRequestedPeer(QString,int)),
	        this, SLOT(requestRematchPeer(QString,int)));
	connect(rcw, SIGNAL(gameClosed(QString)), this, SLOT(removeActiveGame(QString)));
	connect(rcw, SIGNAL(gameEnded(QString)), this, SLOT(removeActiveGameListing(QString)));
	rcw->show();

	activeGames.insert(peer_id, rcw);
	ui->active_games->addItem(QString::fromStdString(peer_id));
}

void NEMainpage::requestRematchPeer(QString peerId, int localColor)
{
	QVariantMap map;
	map.insert("type", "chess_rematch");
	map.insert("color", localColor);
	rsRetroChess->qvm_msg_peer(RsPeerId(peerId.toStdString()), map);

	const std::string key = peerId.toStdString();
	if (activeGames.contains(key)) {
		activeGames.value(key)->m_rematchRequested = true;
		activeGames.value(key)->showGameStatus(tr("Waiting for opponent to accept rematch"));
	}
}

void NEMainpage::create_chess_window_gxs(const RsGxsId &gxs_id, int player_id)
{
    // Open the window with the GXS constructor
    RetroChessWindow *win = new RetroChessWindow(gxs_id, player_id); 
    connect(win, SIGNAL(rematchRequested(RsGxsId,int)),
            this, SLOT(requestRematchGxs(RsGxsId,int)));
    connect(win, SIGNAL(gameClosed(QString)), this, SLOT(removeActiveGame(QString)));
    connect(win, SIGNAL(gameEnded(QString)), this, SLOT(removeActiveGameListing(QString)));
    win->show();

    // Track the game so GXS moves can be routed to it
    std::string key = gxs_id.toStdString();
    activeGames.insert(key, win);
    ui->active_games->addItem(QString::fromStdString(key));
}

void NEMainpage::setupMenuActions()
{
	QToolButton *settingsButton = new QToolButton(this);
	settingsButton->setIcon(QIcon(":/icons/png/settings.png"));
	settingsButton->setToolTip(tr("RetroChess settings"));
	settingsButton->setAutoRaise(true);
	settingsButton->setFocusPolicy(Qt::NoFocus);
	ui->horizontalLayout_2->insertWidget(ui->horizontalLayout_2->count() - 1, settingsButton);
	connect(settingsButton, &QToolButton::clicked, this, [this]() {
		RetroChessSettingsDialog dialog(this);
		if (dialog.exec() != QDialog::Accepted)
			return;

		for (RetroChessWindow *window : activeGames)
			if (window)
				window->refreshBoardTheme();
	});

}

