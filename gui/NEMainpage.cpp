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
#include "retroshare/rsservicecontrol.h"
//#include "gui/notifyqt.h"
#include <qjsondocument.h>
#include <qtreewidget.h>

#include <iostream>
#include <string>
#include <QTime>
#include <QMenu>
#include <QMessageBox>
#include <QToolButton>

#include "gui/RetroChessSettings.h"

#include "gui/chat/ChatDialog.h"

#include "gui/common/AvatarDefs.h"
// COLUMN_NAME_INDEX shoule equal to FriendSelectionWidget.cpp's COLUMN_NAME
#define COLUMN_NAME_INDEX 0

NEMainpage::NEMainpage(QWidget *parent, RetroChessNotify *notify) :
	MainPage(parent),
	mNotify(notify),
	ui(new Ui::NEMainpage)
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
	connect(ui->friendSelectionWidget, SIGNAL(itemSelectionChanged()), this, SLOT(friendSelectionChanged()));

    // enable/disable the invite button
    connect(ui->friendSelectionWidget, SIGNAL(itemSelectionChanged()), this, SLOT(enable_inviteButton()));

	ui->friendSelectionWidget->start();
	ui->friendSelectionWidget->setModus(FriendSelectionWidget::MODUS_SINGLE);
	ui->friendSelectionWidget->setShowType(FriendSelectionWidget::SHOW_SSL);

	connect(ui->friendSelectionWidget, SIGNAL(contentChanged()), this, SLOT(on_filterPeersButton_clicked()));
	//connect(NotifyQt::getInstance(), SIGNAL(peerStatusChanged(const QString&,int)), this, SLOT(on_filterPeersButton_clicked()));

	QString welcomemessage = QTime::currentTime().toString() +" ";
	welcomemessage+= tr("Welcome to RetroChess lobby");
	ui->listWidget->addItem(welcomemessage);

}

NEMainpage::~NEMainpage()
{
	delete ui;
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
	if (type == "chat")
	{
		QString output = QTime::currentTime().toString() +" ";
		output+= QString::fromStdString(rsPeers->getPeerName(peer_id));
		output+=": ";
		output+=vmap.value("message").toString();
		ui->listWidget->addItem(output);
	}
	else if (type == "chessclick")
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
		// enable to be invite
		if( this->ui->check_enableBeInvited->isChecked() )
		{
			ChatDialog::chatFriend(ChatId(peer_id));
			rsRetroChess->gotInvite(peer_id);
			mNotify->notifyChessInvite(peer_id);
		}
		else	// refuse all invites
		{
			//mNotify->notifyChessInvite(peer_id);
		}
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
		ui->listWidget->addItem(output);
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

void NEMainpage::on_broadcastButton_clicked()
{
	rsRetroChess->msg_all(ui->msgInput->text().toStdString());
	NeMsgArrived(rsPeers->getOwnId(),ui->msgInput->text());
	ui->msgInput->clear();
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

// enable the invite button when selected a friend
void NEMainpage::enable_inviteButton()
{
    // get peer
    FriendSelectionWidget::IdType idType;
    std::string fid = ui->friendSelectionWidget->selectedId(idType);

    if( fid == "")	// haven't selected any friend, disable the invite button
        ui->inviteButton->setEnabled(false);
    else
        ui->inviteButton->setEnabled(true);
}

void NEMainpage::on_inviteButton_clicked()
{
	//get peer
	FriendSelectionWidget::IdType idType;
	std::string fid = ui->friendSelectionWidget->selectedId(idType);

    if( fid != "")	// selected a friend
    {
		QVariantMap map;
		map.insert("type", "chess_invite");

        rsRetroChess->qvm_msg_peer(RsPeerId(fid),map);

        std::cout << fid;
    }
    else
    {
        std::cout << "please select a friend";
    }
}

void NEMainpage::on_filterPeersButton_clicked()
{
	std::cout << "\n\n filter peers \n";

	std::list<RsPeerId> ssllist ;
	rsPeers->getFriendList(ssllist);


	RsPeerServiceInfo ownServices;
	rsServiceControl->getOwnServices(ownServices);

	std::vector<RsPeerId> peer_ids ;
	std::vector<uint32_t> service_ids ;

	for(std::list<RsPeerId>::const_iterator it(ssllist.begin()); it!=ssllist.end(); ++it)
		peer_ids.push_back(*it) ;
	service_ids.clear() ;
	uint32_t service_id;
	for(std::map<uint32_t, RsServiceInfo>::const_iterator sit(ownServices.mServiceList.begin()); sit!=ownServices.mServiceList.end(); ++sit)
	{
		RsServiceInfo rsi = sit->second;
		service_ids.push_back(sit->first) ;
		std::cout << rsi.mServiceName << rsi.mServiceType << "\n";
		if (strcmp(rsi.mServiceName.c_str(), "RetroChess") == 0)
		{
			service_id = rsi.mServiceType;
			std::cout << "setting service ID\n";
		}
	}

	for(std::list<RsPeerId>::const_iterator it(ssllist.begin()); it!=ssllist.end(); ++it)
	{
		RsPeerServiceInfo local_service_perms ;
		RsPeerServiceInfo remote_service_perms ;
		RsPeerId id = *it;

        // get friend 's avatar
        QPixmap friend_avatar;
        AvatarDefs::getAvatarFromSslId( id, friend_avatar);

		rsServiceControl->getServicesAllowed (*it, local_service_perms) ;
		rsServiceControl->getServicesProvided(*it,remote_service_perms) ;

		bool  local_allowed =  local_service_perms.mServiceList.find(service_id) !=  local_service_perms.mServiceList.end() ;
		bool remote_allowed = remote_service_perms.mServiceList.find(service_id) != remote_service_perms.mServiceList.end() ;
		bool allowed = (local_allowed && remote_allowed);
		//QString la =
		QString serviceinfos = QString("peerlocal: ") + QString(local_allowed?"yes":"no") + QString(" remote: ") + QString(remote_allowed?"yes":"no");
		ui->netLogWidget->addItem(serviceinfos);
		std::cout << serviceinfos.toStdString() << "\n";
		//if (allowed){
		QList<QTreeWidgetItem*> items;

		ui->friendSelectionWidget->itemsFromId(FriendSelectionWidget::IDTYPE_SSL,id.toStdString(),items);

		std::cout << items.size() << "\n";
		if (items.size())
		{
            // setup ICON COLUMN_NAME=0 (add avatar)
            items.first()->setIcon( COLUMN_NAME_INDEX, QIcon(friend_avatar));

			QTreeWidgetItem* item = items.first();
			item->setHidden(!allowed);
		}
	}
}

void NEMainpage::setupMenuActions()
{
	mActionPlayChess = new QAction(QIcon(), tr("Play Chess"), this);
	connect(mActionPlayChess, SIGNAL(triggered(bool)), this, SLOT(on_playButton_clicked()));

	ui->friendSelectionWidget->addContextMenuAction(mActionPlayChess);

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

void NEMainpage::friendSelectionChanged()
{
	std::set<RsPeerId> peerIds;
	ui->friendSelectionWidget->selectedIds<RsPeerId,FriendSelectionWidget::IDTYPE_SSL>(peerIds, false);

	std::set<RsGxsId> gxsIds;
	ui->friendSelectionWidget->selectedIds<RsGxsId,FriendSelectionWidget::IDTYPE_GXS>(gxsIds, false);

	int selectedCount = peerIds.size() + gxsIds.size();

	mActionPlayChess->setEnabled(selectedCount);

	FriendSelectionWidget::IdType idType;
	ui->friendSelectionWidget->selectedId(idType);

}
