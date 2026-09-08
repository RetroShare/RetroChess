/*******************************************************************************
 * gui/NEMainpage.cpp                                                          *
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
 *                                                                             *
 *******************************************************************************/

#include "NEMainpage.h"
#include "ui_NEMainpage.h"

#include "services/p3RetroChess.h"
#include "interface/rsRetroChess.h"
#include "services/rsRetroChessItems.h"

#include <qjsondocument.h>

#include <QPointer>

#include <iostream>
#include <algorithm>
#include <string>
#include <QMenu>
#include <QMessageBox>
#include <QMessageBox>
#include <QToolButton>
#include <QTimer>
#include <QShowEvent>
#include <QDateTime>
#include <QCoreApplication>
#include <QLocale>
#include <QHeaderView>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTreeWidgetItem>
#include <QFileDialog>
#include <QSaveFile>

#include "gui/RetroChessSettings.h"
#include "gui/RetroChessSessionService.h"
#include "gui/ChessGameHistory.h"
#include "gui/ChessGameReviewDialog.h"
#include "gui/RetroChessUserNotify.h"
#include "gui/gxs/GxsIdTreeWidgetItem.h"
#include "gui/settings/rsharesettings.h"
#include "gui/common/AvatarDefs.h"
#include "util/HandleRichText.h"

#include "gui/chat/ChatDialog.h"
#include <retroshare/rsidentity.h>
#include <retroshare/rsevents.h>
#include <retroshare/rschats.h>
#include "util/qtthreadsutils.h"


NEMainpage::NEMainpage(QWidget *parent, RetroChessNotify *notify) :
	MainPage(parent),
	ui(new Ui::NEMainpage),
	mNotify(notify),
	mGameSessions(new RetroChessSessionService(this)),
	mOfficialLobbyDialog(nullptr),
	mLobbyUnreadCount(0),
	mEventHandlerId_identity(0),
	mEventHandlerId_chat(0)
{
	ui->setupUi(this);
	setupMenuActions();
	connect(mGameSessions, &RetroChessSessionService::gameAdded,
	        this, [this](const QString &key) {
		for (int row = 0; row < ui->active_games->topLevelItemCount(); ++row)
			if (ui->active_games->topLevelItem(row)->data(0, Qt::UserRole).toString() == key)
				return;
		RetroChessWindow *window = mGameSessions->game(key);
		QTreeWidgetItem *item = new QTreeWidgetItem(ui->active_games);
		item->setText(0, window ? window->activeGameDescription() : key);
		item->setText(1, key);
		item->setData(0, Qt::UserRole, key);
		if (window) item->setToolTip(0, window->windowTitle());
	});
	ui->active_games->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	ui->active_games->header()->setSectionResizeMode(1, QHeaderView::Interactive);
	ui->active_games->setColumnWidth(1, 260);
	connect(mGameSessions, &RetroChessSessionService::gameRemoved,
	        this, [this](const QString &key) {
		removeActiveGameListing(key);
		if (!QCoreApplication::closingDown())
			rsRetroChess->unregisterGameSession(key);
	});
	connect(mGameSessions, &RetroChessSessionService::unroutableEvent,
	        this, [](const QString &key, const QString &eventType) {
		std::cerr << "RetroChess: Received " << eventType.toStdString()
		          << " but no active game exists for " << key.toStdString()
		          << std::endl;
	});
	connect(mGameSessions, &RetroChessSessionService::peerInviteReceived,
	        this, &NEMainpage::chessInvitePeer);
	connect(mGameSessions, &RetroChessSessionService::peerInviteAccepted,
	        this, &NEMainpage::chessAcceptedPeer);
	connect(mGameSessions, &RetroChessSessionService::peerRematchRequested,
	        this, &NEMainpage::chessRematchPeer);
	connect(mGameSessions, &RetroChessSessionService::invalidPeerPacket,
	        this, [](const RsPeerId &peer, const QString &reason) {
		std::cerr << "RetroChess: Invalid packet from " << peer.toStdString()
		          << ": " << reason.toStdString() << std::endl;
	});

	connect(mNotify, SIGNAL(NeMsgArrived(RsPeerId,QString)), this, SLOT(NeMsgArrived(RsPeerId,QString)));
	connect(mNotify, SIGNAL(chessStart(RsPeerId)), this, SLOT(chessStart(RsPeerId)));
	connect(mNotify, SIGNAL(chessInvitedGxs(RsGxsId)), this, SLOT(chessInviteReceivedGxs(RsGxsId)));
	connect(mNotify, SIGNAL(gxsTunnelClosed(RsGxsId)), this, SLOT(chessTunnelClosed(RsGxsId)));
	// The inviter is White; the participant who accepts is Black.
	connect(mNotify, SIGNAL(chessStartGxs(RsGxsId)), this, SLOT(chessStartGxsAsBlack(RsGxsId)));
	connect(mNotify, SIGNAL(chessAcceptedGxs(RsGxsId)), this, SLOT(chessStartGxs(RsGxsId)));
	connect(mNotify, &RetroChessNotify::chessRejectedGxs, this,
	        [this](const RsGxsId &gxsId) {
		QMessageBox *message = new QMessageBox(QMessageBox::Information,
		        tr("Chess invitation"), tr("Invitation declined."),
		        QMessageBox::Ok, this);
		message->setInformativeText(QString::fromStdString(gxsId.toStdString()));
		message->setAttribute(Qt::WA_DeleteOnClose);
		message->show();
	});
	connect(mNotify, SIGNAL(chessMoveGxs(RsGxsId,int,int,int)), this, SLOT(chessMoveGxs(RsGxsId,int,int,int)));
	connect(mNotify, SIGNAL(chessPlayerLeftGxs(RsGxsId)), this, SLOT(chessPlayerLeftGxs(RsGxsId)));
	connect(mNotify, SIGNAL(chessRematchGxs(RsGxsId,int)), this, SLOT(chessRematchGxs(RsGxsId,int)));
	connect(mNotify, SIGNAL(chessGameActionGxs(RsGxsId,QString)), this, SLOT(chessGameActionGxs(RsGxsId,QString)));

	connect(mNotify, &RetroChessNotify::availablePeersChanged,
	        this, &NEMainpage::refreshAvailablePlayers, Qt::QueuedConnection);
	connect(mNotify, &RetroChessNotify::gxsTunnelReady,
	        this, &NEMainpage::refreshAvailablePlayers, Qt::QueuedConnection);
	connect(mNotify, &RetroChessNotify::gxsTunnelClosed,
	        this, &NEMainpage::refreshAvailablePlayers, Qt::QueuedConnection);
	connect(mNotify, &RetroChessNotify::chessInvitedGxs,
	        this, &NEMainpage::refreshAvailablePlayers, Qt::QueuedConnection);
	connect(mNotify, &RetroChessNotify::chessInviteClearedGxs,
	        this, &NEMainpage::refreshAvailablePlayers, Qt::QueuedConnection);
	connect(mGameSessions, &RetroChessSessionService::gameAdded,
	        this, &NEMainpage::refreshAvailablePlayers, Qt::QueuedConnection);
	connect(mGameSessions, &RetroChessSessionService::gameRemoved,
	        this, &NEMainpage::refreshAvailablePlayers, Qt::QueuedConnection);

	if (rsEvents) {
		mEventHandlerId_identity = 0;
		rsEvents->registerEventsHandler(
			[this](std::shared_ptr<const RsEvent> event) {
				RsQThreadUtils::postToObject([this, event]() {
					handleEvent_identity_main_thread(event);
				}, this);
			},
			mEventHandlerId_identity,
			RsEventType::GXS_IDENTITY
		);

		mEventHandlerId_chat = 0;
		rsEvents->registerEventsHandler(
			[this](std::shared_ptr<const RsEvent> event) {
				RsQThreadUtils::postToObject([this, event]() {
					handleEvent_chat_main_thread(event);
				}, this);
			},
			mEventHandlerId_chat,
			RsEventType::CHAT_SERVICE
		);
	}

	QTimer::singleShot(0, this, SLOT(autoJoinOfficialLobby()));
	ui->pendingInvites->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	ui->pendingInvites->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	ui->pendingInvites->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	QFontMetricsF fontMetrics(ui->contactstreeWidget->font());

	int iconHeight = fontMetrics.height() * 2.0;
	ui->availablePlayers->setIconSize(QSize(iconHeight, iconHeight));
	ui->pendingInvites->setIconSize(QSize(iconHeight, iconHeight));

	ui->contactstreeWidget->setFixedWidth(260);
	ui->contactstreeWidget->setIconSize(QSize(iconHeight, iconHeight));
	ui->contactstreeWidget->setRootIsDecorated(false);
	ui->contactstreeWidget->setAlternatingRowColors(false);
	ui->contactstreeWidget->setSortingEnabled(true);
	ui->contactstreeWidget->sortItems(0, Qt::AscendingOrder);
	ui->contactstreeWidget->setToolTip(tr("Right-click a contact to invite them to chess."));
	ui->contactstreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->contactstreeWidget, &QTreeWidget::customContextMenuRequested, this,
	        &NEMainpage::ContactsCustomPopupMenu);

	ui->availablePlayers->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	ui->availablePlayers->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	ui->availablePlayers->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	ui->availablePlayers->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	connect(ui->tabWidget, &QTabWidget::currentChanged,
	        this, [this](int) {
		refreshAvailablePlayers();
		refreshContacts();
	});
	refreshContacts();
	QHeaderView *historyHeader = ui->gameHistory->header();
	historyHeader->setSectionResizeMode(QHeaderView::Interactive);
	historyHeader->setSectionsMovable(true);
	historyHeader->setMinimumSectionSize(45);
	const QByteArray historyHeaderState = Settings->valueFromGroup(
	        "RetroChess", "GameHistoryHeaderState", QByteArray()).toByteArray();
	if (!historyHeaderState.isEmpty())
		historyHeader->restoreState(historyHeaderState);
	else {
		historyHeader->resizeSection(0, 165);
		historyHeader->resizeSection(1, 210);
		historyHeader->resizeSection(2, 210);
		historyHeader->resizeSection(3, 85);
		historyHeader->resizeSection(4, 75);
	}
	historyHeader->setStretchLastSection(true);
	connect(historyHeader, &QHeaderView::sectionResized, this,
	        [historyHeader](int, int, int) {
		Settings->setValueToGroup(
		        "RetroChess", "GameHistoryHeaderState", historyHeader->saveState());
	});
	connect(historyHeader, &QHeaderView::sectionMoved, this,
	        [historyHeader](int, int, int) {
		Settings->setValueToGroup(
		        "RetroChess", "GameHistoryHeaderState", historyHeader->saveState());
	});
	ui->gameHistory->setIconSize(QSize(32, 32));
	ui->gameHistory->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->reviewGameButton, &QPushButton::clicked,
	        this, &NEMainpage::reviewSelectedGame);
	connect(ui->exportGameButton, &QPushButton::clicked,
	        this, &NEMainpage::exportSelectedGame);
	connect(ui->deleteGameButton, &QPushButton::clicked,
	        this, &NEMainpage::deleteSelectedGame);
	connect(ui->gameHistory, &QTreeWidget::itemDoubleClicked,
	        this, [this](QTreeWidgetItem *, int) { reviewSelectedGame(); });
	connect(ui->gameHistory, &QTreeWidget::customContextMenuRequested,
	        this, [this](const QPoint &position) {
		QTreeWidgetItem *item = ui->gameHistory->itemAt(position);
		if (!item) return;
		ui->gameHistory->setCurrentItem(item);
		QMenu menu(ui->gameHistory);
		QAction *review = menu.addAction(tr("Review"));
		QAction *exportPgn = menu.addAction(tr("Export PGN"));
		menu.addSeparator();
		QAction *remove = menu.addAction(tr("Delete"));
		QAction *selected = menu.exec(ui->gameHistory->viewport()->mapToGlobal(position));
		if (selected == review) reviewSelectedGame();
		else if (selected == exportPgn) exportSelectedGame();
		else if (selected == remove) deleteSelectedGame();
	});
	refreshGameHistory();
	refreshAvailablePlayers();
	for (const RsRetroChessGameSession &session : rsRetroChess->gameSessions()) {
		if (session.gxs)
			create_chess_window_gxs(
			        RsGxsId(session.endpointId.toStdString()), session.localColor);
		else
			create_chess_window(session.endpointId.toStdString(), session.localColor);
		if (RetroChessWindow *window = mGameSessions->game(session.endpointId)) {
			QString error;
			if (!session.fen.isEmpty()
			        && !window->restoreSessionPosition(
			                session.fen, session.moveSequence, &error))
				std::cerr << "RetroChess: Could not restore session "
				          << session.endpointId.toStdString() << ": "
				          << error.toStdString() << std::endl;
		}
	}
}

void NEMainpage::ContactsCustomPopupMenu(QPoint position)
{
	QTreeWidgetItem *item = ui->contactstreeWidget->itemAt(position);
	if (!item) return;
	ui->contactstreeWidget->setCurrentItem(item);
	// Copy the identity: contact refreshes may remove the row while the menu is open.
	const QString endpoint = item->data(0, Qt::UserRole).toString();
	QMenu contextMnu(this);
	QAction *invite = contextMnu.addAction(tr("Invite to chess"));
	invite->setEnabled(!mGameSessions->contains(endpoint));
	if (contextMnu.exec(ui->contactstreeWidget->viewport()->mapToGlobal(position)) != invite)
		return;
	const RsGxsId targetGxsId(endpoint.toStdString());
	if (!rsIdentity || rsIdentity->isOwnId(targetGxsId)
	        || mGameSessions->contains(endpoint)) return;
	if (!rsRetroChess->sendInviteToGxs(targetGxsId)) {
		QMessageBox::warning(this, tr("Chess invitation"),
		        tr("The chess invitation could not be sent."));
		return;
	}
	refreshAvailablePlayers();
}

void NEMainpage::refreshContacts()
{
	if (!rsIdentity) return;
	std::list<RsGroupMetaData> identities;
	if (!rsIdentity->getIdentitiesSummaries(identities)) return;
	QMap<QString, RsGxsId> contacts;
	for (const auto &identity : identities) {
		const RsGxsId id(identity.mGroupId.toStdString());
		if (!id.isNull() && !rsIdentity->isOwnId(id) && rsIdentity->isARegularContact(id))
			contacts.insert(QString::fromStdString(id.toStdString()), id);
	}
	// Retain rows and refresh their pixmaps directly, without the identity item's
	// font-sized decoration callback overwriting the larger avatar.
	QMap<QString, QTreeWidgetItem *> rows;
	for (int row = ui->contactstreeWidget->topLevelItemCount() - 1; row >= 0; --row) {
		QTreeWidgetItem *item = ui->contactstreeWidget->topLevelItem(row);
		const QString key = item->data(0, Qt::UserRole).toString();
		if (!contacts.contains(key)) delete item;
		else rows.insert(key, item);
	}
	ui->contactstreeWidget->setSortingEnabled(false);
	for (auto it = contacts.constBegin(); it != contacts.constEnd(); ++it) {
		QTreeWidgetItem *item = rows.value(it.key());
		if (!item) item = new QTreeWidgetItem(ui->contactstreeWidget);
		const RsGxsId &id = it.value();
		RsIdentityDetails details;
		const bool detailsAvailable = rsIdentity->getIdDetails(id, details);
		QString name = it.key();
		if (detailsAvailable && !details.mNickname.empty())
			name = QString::fromUtf8(details.mNickname.c_str());
		QPixmap pixmap;
		if (!detailsAvailable || details.mAvatar.mSize == 0
		        || !GxsIdDetails::loadPixmapFromData(details.mAvatar.mData,
		                details.mAvatar.mSize, pixmap, GxsIdDetails::MEDIUM))
			pixmap = GxsIdDetails::makeDefaultIcon(id, GxsIdDetails::MEDIUM);
		item->setText(0, name);
		item->setIcon(0, QIcon(pixmap));
		item->setData(0, Qt::UserRole, it.key());
		QPixmap tooltipPixmap;
		if (!detailsAvailable || details.mAvatar.mSize == 0
		        || !GxsIdDetails::loadPixmapFromData(details.mAvatar.mData,
		                details.mAvatar.mSize, tooltipPixmap, GxsIdDetails::LARGE))
			tooltipPixmap = GxsIdDetails::makeDefaultIcon(id, GxsIdDetails::LARGE);
		QString tooltip = detailsAvailable ? GxsIdDetails::getComment(details) : QString();
		if (tooltip.isEmpty())
			tooltip = tr("Identity name: %1<br/>Identity Id: %2")
			        .arg(name.toHtmlEscaped(), it.key().toHtmlEscaped());
		QString embeddedImage;
		if (RsHtml::makeEmbeddedImage(tooltipPixmap.scaled(QSize(96, 96),
		        Qt::KeepAspectRatio, Qt::SmoothTransformation).toImage(), embeddedImage, -1))
			tooltip = QString("<table><tr><td>%1</td><td>%2</td></tr></table>")
			        .arg(embeddedImage, tooltip);
		item->setToolTip(0, tooltip);
	}
	ui->contactstreeWidget->setSortingEnabled(true);
}

void NEMainpage::refreshAvailablePlayers()
{
	ui->availablePlayers->setSortingEnabled(false);
	ui->availablePlayers->clear();
	const auto peers = rsRetroChess->availableChessPeers();
	for (const RsRetroChessAvailablePeer &peer : peers) {
		if (!peer.gxs) continue;
		QTreeWidgetItem *item = new QTreeWidgetItem(ui->availablePlayers);
		const RsGxsId id(peer.endpointId.toStdString());
		RsIdentityDetails details;
		const bool detailsAvailable = rsIdentity && rsIdentity->getIdDetails(id, details);
		QString name = peer.endpointId;
		if (detailsAvailable && !details.mNickname.empty())
			name = QString::fromUtf8(details.mNickname.c_str());
		QPixmap pixmap;
		if (!detailsAvailable || details.mAvatar.mSize == 0
		        || !GxsIdDetails::loadPixmapFromData(details.mAvatar.mData,
		                details.mAvatar.mSize, pixmap, GxsIdDetails::MEDIUM))
			pixmap = GxsIdDetails::makeDefaultIcon(id, GxsIdDetails::MEDIUM);
		item->setText(0, name);
		item->setIcon(0, QIcon(pixmap));
		if (detailsAvailable) item->setToolTip(0, GxsIdDetails::getComment(details));
		item->setData(0, Qt::UserRole, peer.endpointId);
		item->setText(3, tr("GXS identity"));
		item->setText(2, peer.tunnelReady ? tr("Ready") : tr("Connecting"));

		QPushButton *invite = new QPushButton(tr("Invite"), ui->availablePlayers);
		invite->setFixedHeight(28);
		invite->setEnabled(peer.tunnelReady
		        && !mGameSessions->contains(peer.endpointId));
		if (mGameSessions->contains(peer.endpointId)) invite->setText(tr("Playing"));
		QWidget *actionCell = new QWidget(ui->availablePlayers);
		QHBoxLayout *actionLayout = new QHBoxLayout(actionCell);
		actionLayout->setContentsMargins(0, 0, 0, 0);
		actionLayout->addWidget(invite, 1, Qt::AlignVCenter);
		ui->availablePlayers->setItemWidget(item, 1, actionCell);
		connect(invite, &QPushButton::clicked, this, [this, peer]() {
			bool sent = false;
			sent = rsRetroChess->sendInviteToGxs(
			        RsGxsId(peer.endpointId.toStdString()));
			if (!sent)
				QMessageBox::warning(
				        this, tr("Chess invitation"),
				        tr("The chess invitation could not be sent."));
		});
	}
	ui->availablePlayersDescription->setText(peers.empty()
	        ? tr("No RetroChess identities are currently available.")
	        : tr("Select a RetroChess identity to invite."));
	ui->availablePlayers->setSortingEnabled(true);
}

NEMainpage::~NEMainpage()
{
	if (rsEvents) {
		if (mEventHandlerId_identity != 0)
			rsEvents->unregisterEventsHandler(mEventHandlerId_identity);
		if (mEventHandlerId_chat != 0)
			rsEvents->unregisterEventsHandler(mEventHandlerId_chat);
	}
	mGameSessions->closeAll();
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
	if (mLobbyUnreadCount || !mUnreadInviteKeys.isEmpty()) {
		mLobbyUnreadCount = 0;
		mUnreadInviteKeys.clear();
		emit lobbyUnreadCountChanged();
	}
	MainPage::showEvent(event);
	refreshContacts();
	refreshAvailablePlayers();
	autoJoinOfficialLobby();
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
	removePendingInvitation("gxs:" + QString::fromStdString(gxs_id.toStdString()));
	// We accepted the remote participant's invitation: we are Black.
	create_chess_window_gxs(gxs_id, 1);
}

namespace
{
QString invitationButtonStyle()
{
	return "QPushButton { border: 1px solid #199909; font-size: 11pt;"
	       " color: white; padding: 3px 12px; min-height: 22px;"
	       " border-radius: 6px; background-color: qlineargradient("
	       " x1: 0, y1: 0, x2: 0, y2: 0.67, stop: 0 #22c70d,"
	       " stop: 1 #116a06); }"
	       " QPushButton:hover { border-color: #35d51f; }"
	       " QPushButton:pressed { background-color: #116a06; }";
}
}

void NEMainpage::chessInviteReceivedGxs(const RsGxsId &gxs_id)
{
	addGxsInvitation(gxs_id);
}

void NEMainpage::chessTunnelClosed(const RsGxsId &gxs_id)
{
	removePendingInvitation("gxs:" + QString::fromStdString(gxs_id.toStdString()));
}

void NEMainpage::addGxsInvitation(const RsGxsId &gxs_id)
{
	if (!rsRetroChess || !rsRetroChess->hasInviteFromGxs(gxs_id)) return;
	const QString key = "gxs:" + QString::fromStdString(gxs_id.toStdString());

	removePendingInvitation(key);
	GxsIdRSTreeWidgetItem *item = new GxsIdRSTreeWidgetItem(
	        nullptr, GxsIdDetails::ICON_TYPE_AVATAR, true, ui->pendingInvites);
	QFont identityFont = item->font(0);
	identityFont.setPointSize(qMax(identityFont.pointSize() + 4, 18));
	item->setFont(0, identityFont);
	item->setId(gxs_id, 0, true);
	item->setText(1, QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat));
	item->setSizeHint(0, QSize(44, 48));

	QWidget *actions = new QWidget(ui->pendingInvites);
	QHBoxLayout *actionsLayout = new QHBoxLayout(actions);
	actionsLayout->setContentsMargins(2, 0, 2, 0);
	actionsLayout->setSpacing(5);
	QPushButton *accept = new QPushButton(tr("Accept"), actions);
	accept->setStyleSheet(invitationButtonStyle());
	accept->setFixedHeight(28);
	QPushButton *reject = new QPushButton(tr("Reject"), actions);
	reject->setFixedHeight(28);
	actionsLayout->addWidget(accept);
	actionsLayout->addWidget(reject);
	ui->pendingInvites->setItemWidget(item, 2, actions);
	mPendingInvites.insert(key, item);
	ui->pendingInvitesBox->show();
	if (!isVisible()) mUnreadInviteKeys.insert(key);
	emit lobbyUnreadCountChanged();
	connect(accept, &QPushButton::clicked, this, [this, gxs_id, key]() {
		if (!rsRetroChess->hasInviteFromGxs(gxs_id)) {
			removePendingInvitation(key);
			return;
		}
		rsRetroChess->acceptedInviteGxs(gxs_id);
		removePendingInvitation(key);
		mNotify->notifyChessStartGxs(gxs_id);
	});
	connect(reject, &QPushButton::clicked, this, [this, gxs_id, key]() {
		if (!rsRetroChess->hasInviteFromGxs(gxs_id)) {
			removePendingInvitation(key);
			return;
		}
		if (!rsRetroChess->rejectedInviteGxs(gxs_id)) {
			QMessageBox::warning(this, tr("Chess invitation"),
			        tr("The rejection could not be sent. Please try again."));
			return;
		}
		removePendingInvitation(key);
	});
}

void NEMainpage::removePendingInvitation(const QString &key)
{
	QTreeWidgetItem *item = mPendingInvites.take(key);
	const bool wasUnread = mUnreadInviteKeys.remove(key) > 0;
	if (item) {
		delete item;
		emit lobbyUnreadCountChanged();
	} else if (wasUnread) emit lobbyUnreadCountChanged();
}

void NEMainpage::chessMoveGxs(const RsGxsId &gxs_id, int col, int row, int count)
{
	mGameSessions->routeMove(
	        QString::fromStdString(gxs_id.toStdString()), col, row, count);
}

void NEMainpage::chessPlayerLeftGxs(const RsGxsId &gxs_id)
{
	mGameSessions->routePlayerLeft(
	        QString::fromStdString(gxs_id.toStdString()));
}

void NEMainpage::removeActiveGame(QString gameId)
{
	mGameSessions->unregisterGame(gameId);
}

void NEMainpage::removeActiveGameListing(QString gameId)
{
	for (int row = ui->active_games->topLevelItemCount() - 1; row >= 0; --row) {
		if (ui->active_games->topLevelItem(row)->data(0, Qt::UserRole).toString() == gameId)
			delete ui->active_games->takeTopLevelItem(row);
	}
}

void NEMainpage::requestRematchGxs(const RsGxsId &gxs_id, int localColor)
{
	if (!rsRetroChess->sendRematchGxs(gxs_id, localColor))
		return;

	const QString key = QString::fromStdString(gxs_id.toStdString());
	if (RetroChessWindow *window = mGameSessions->game(key)) {
		window->m_rematchRequested = true;
		window->showGameStatus(tr("Waiting for opponent to accept rematch"));
	}
}

void NEMainpage::chessRematchGxs(const RsGxsId &gxs_id, int remoteColor)
{
	const QString key = QString::fromStdString(gxs_id.toStdString());
	if (!mGameSessions->contains(key)) {
		rsRetroChess->sendGameActionGxs(gxs_id, "rematch_decline");
		return;
	}
	QPointer<RetroChessWindow> window = mGameSessions->game(key);
	const bool alreadyRequested = window->m_rematchRequested;
	if (!alreadyRequested && QMessageBox::question(
	        window, tr("Rematch"), tr("Your opponent requests a rematch. Accept?")) != QMessageBox::Yes) {
		rsRetroChess->sendGameActionGxs(gxs_id, "rematch_decline");
		return;
	}
	// The question box spins a nested event loop: other network slots run
	// meanwhile and may have destroyed the window (e.g. a concurrent rematch
	// or close). Never touch it again without checking.
	if (!window)
		return;
	if (!alreadyRequested)
		rsRetroChess->sendRematchGxs(gxs_id, window->m_localplayer_turn);
	window->closeForRematch();
	const int localColor = remoteColor == 0 ? 1 : 0;
	create_chess_window_gxs(gxs_id, localColor == 0 ? 1 : 0);
}

void NEMainpage::chessGameActionGxs(const RsGxsId &gxs_id, QString action)
{
	mGameSessions->routeAction(
	        QString::fromStdString(gxs_id.toStdString()), action);
}

// decode received message here
void NEMainpage::NeMsgArrived(const RsPeerId &peer_id, QString str)
{
	mGameSessions->processPeerPacket(peer_id, str);
}

void NEMainpage::chessInvitePeer(const RsPeerId &peer_id)
{
	ChatDialog::chatFriend(ChatId(peer_id));
	rsRetroChess->gotInvite(peer_id);
	mNotify->notifyChessInvite(peer_id);
}

void NEMainpage::chessAcceptedPeer(const RsPeerId &peer_id)
{
	if (!rsRetroChess->hasInviteTo(peer_id)) return;
	rsRetroChess->clearInvite(peer_id);
	create_chess_window(peer_id.toStdString(), 0);
}

void NEMainpage::chessRematchPeer(const RsPeerId &peer_id, int remoteColor)
{
	const QString key = QString::fromStdString(peer_id.toStdString());
	QPointer<RetroChessWindow> window = mGameSessions->game(key);
	QVariantMap reply;
	if (!window) {
		reply.insert("type", "game_action");
		reply.insert("action", "rematch_decline");
		rsRetroChess->qvm_msg_peer(peer_id, reply);
		return;
	}
	const bool alreadyRequested = window->m_rematchRequested;
	if (!alreadyRequested && QMessageBox::question(
	        window, tr("Rematch"), tr("Your opponent requests a rematch. Accept?"))
	        != QMessageBox::Yes) {
		reply.insert("type", "game_action");
		reply.insert("action", "rematch_decline");
		rsRetroChess->qvm_msg_peer(peer_id, reply);
		return;
	}
	if (!window) return;
	if (!alreadyRequested) {
		reply.insert("type", "chess_rematch");
		reply.insert("color", window->m_localplayer_turn);
		rsRetroChess->qvm_msg_peer(peer_id, reply);
	}
	window->closeForRematch();
	const int localColor = remoteColor == 0 ? 1 : 0;
	create_chess_window(key.toStdString(), localColor == 0 ? 1 : 0);
}

void NEMainpage::create_chess_window(std::string peer_id, int player_id)
{
	const QString key = QString::fromStdString(peer_id);
	if (RetroChessWindow *existing = mGameSessions->game(key)) {
		if (existing->m_flag_finished == 0) {
			// A game with this opponent is already running. Overwriting the
			// map entry would orphan the old window (it stays open but stops
			// receiving moves) — just bring it to the front instead.
			existing->raise();
			existing->activateWindow();
			return;
		}
		// Finished game: close it silently before opening the new one.
		existing->closeForRematch();
	}

	RetroChessWindow *rcw = new RetroChessWindow(peer_id, player_id);
	connect(rcw, SIGNAL(rematchRequestedPeer(QString,int)),
	        this, SLOT(requestRematchPeer(QString,int)));
	connect(rcw, SIGNAL(gameClosed(QString)), this, SLOT(removeActiveGame(QString)));
	connect(rcw, SIGNAL(gameEnded(QString)), this, SLOT(removeActiveGameListing(QString)));
	connect(rcw, SIGNAL(gameReadyForHistory()), this, SLOT(archiveFinishedGame()));
	rcw->show();

	mGameSessions->registerGame(
	        key, RetroChessSessionService::PeerEndpoint, rcw);
	RsRetroChessGameSession session;
	session.endpointId = key;
	session.localColor = player_id;
	session.fen = rcw->sessionFen();
	rsRetroChess->registerGameSession(session);
	connect(rcw, &RetroChessWindow::sessionStateChanged, this,
	        [key](const QString &fen, uint32_t sequence) {
		rsRetroChess->updateGameSession(key, fen, sequence);
	});
}

void NEMainpage::requestRematchPeer(QString peerId, int localColor)
{
	QVariantMap map;
	map.insert("type", "chess_rematch");
	map.insert("color", localColor);
	rsRetroChess->qvm_msg_peer(RsPeerId(peerId.toStdString()), map);

	if (RetroChessWindow *window = mGameSessions->game(peerId)) {
		window->m_rematchRequested = true;
		window->showGameStatus(tr("Waiting for opponent to accept rematch"));
	}
}

void NEMainpage::create_chess_window_gxs(const RsGxsId &gxs_id, int player_id)
{
	const QString key = QString::fromStdString(gxs_id.toStdString());
    if (RetroChessWindow *existing = mGameSessions->game(key)) {
        if (existing->m_flag_finished == 0) {
            // Same as create_chess_window(): never orphan a running game.
            existing->raise();
            existing->activateWindow();
            return;
        }
        existing->closeForRematch();
    }

    // Open the window with the GXS constructor
    RetroChessWindow *win = new RetroChessWindow(gxs_id, player_id);
    connect(win, SIGNAL(rematchRequested(RsGxsId,int)),
            this, SLOT(requestRematchGxs(RsGxsId,int)));
    connect(win, SIGNAL(gameClosed(QString)), this, SLOT(removeActiveGame(QString)));
    connect(win, SIGNAL(gameEnded(QString)), this, SLOT(removeActiveGameListing(QString)));
	connect(win, SIGNAL(gameReadyForHistory()), this, SLOT(archiveFinishedGame()));
    win->show();

    // Track the game so GXS moves can be routed to it
	mGameSessions->registerGame(
	        key, RetroChessSessionService::GxsEndpoint, win);
	RsRetroChessGameSession session;
	session.endpointId = key;
	session.gxs = true;
	session.localIdentityId = QString::fromStdString(win->mOwnGxsId.toStdString());
	session.localColor = player_id;
	session.fen = win->sessionFen();
	rsRetroChess->registerGameSession(session);
	connect(win, &RetroChessWindow::sessionStateChanged, this,
	        [key](const QString &fen, uint32_t sequence) {
		rsRetroChess->updateGameSession(key, fen, sequence);
	});
}

void NEMainpage::archiveFinishedGame()
{
	RetroChessWindow *window = qobject_cast<RetroChessWindow *>(sender());
	if (!window) return;
	ChessGameHistory::addGame(window->historyRecord());
	refreshGameHistory();
}

void NEMainpage::refreshGameHistory()
{
	ui->gameHistory->clear();
	const QVector<ChessGameRecord> games = ChessGameHistory::games();
	for (const ChessGameRecord &game : games) {
		QTreeWidgetItem *item = nullptr;
		if (!game.whiteGxsId.isEmpty()) {
			item = new GxsIdRSTreeWidgetItem(
			        nullptr, GxsIdDetails::ICON_TYPE_AVATAR, true, ui->gameHistory);
		} else item = new QTreeWidgetItem(ui->gameHistory);
		item->setData(0, Qt::UserRole, game.id);
		item->setText(0, QLocale().toString(
		        game.endedAt.toLocalTime(), QLocale::ShortFormat));
		item->setText(1, game.whitePlayer);
		item->setText(2, game.blackPlayer);
		auto setGxsIdentity = [item](
		        int column, const QString &idText, const QString &storedName) {
			if (idText.isEmpty()) return;
			const RsGxsId id(idText.toStdString());
			RsIdentityDetails details;
			QPixmap pixmap;
			QString name = storedName;
			const bool detailsAvailable = rsIdentity->getIdDetails(id, details);
			if (detailsAvailable) {
				if (!details.mNickname.empty())
					name = QString::fromUtf8(details.mNickname.c_str());
				if (details.mAvatar.mSize == 0
				        || !GxsIdDetails::loadPixmapFromData(
				                details.mAvatar.mData, details.mAvatar.mSize,
				                pixmap, GxsIdDetails::MEDIUM))
					pixmap = GxsIdDetails::makeDefaultIcon(id, GxsIdDetails::MEDIUM);
			} else pixmap = GxsIdDetails::makeDefaultIcon(id, GxsIdDetails::MEDIUM);
			item->setText(column, name);
			item->setData(column, Qt::UserRole, idText);
			item->setIcon(column, QIcon(pixmap));

			QPixmap tooltipPixmap;
			if (!detailsAvailable || details.mAvatar.mSize == 0
			        || !GxsIdDetails::loadPixmapFromData(
			                details.mAvatar.mData, details.mAvatar.mSize,
			                tooltipPixmap, GxsIdDetails::LARGE))
				tooltipPixmap = GxsIdDetails::makeDefaultIcon(id, GxsIdDetails::LARGE);
			QString tooltip = detailsAvailable
			        ? GxsIdDetails::getComment(details) : QString();
			if (tooltip.isEmpty())
				tooltip = tr("Identity name: %1<br/>Identity Id: %2")
				        .arg(name.toHtmlEscaped(), idText.toHtmlEscaped());
			QString embeddedImage;
			if (RsHtml::makeEmbeddedImage(
			        tooltipPixmap.scaled(
			                QSize(96, 96), Qt::KeepAspectRatio,
			                Qt::SmoothTransformation).toImage(),
			        embeddedImage, -1))
				tooltip = QString("<table><tr><td>%1</td><td>%2</td></tr></table>")
				        .arg(embeddedImage, tooltip);
			item->setToolTip(column, tooltip);
		};
		setGxsIdentity(1, game.whiteGxsId, game.whitePlayer);
		setGxsIdentity(2, game.blackGxsId, game.blackPlayer);
		if (!game.whitePeerId.isEmpty()) {
			QPixmap avatar;
			AvatarDefs::getAvatarFromSslId(
			        RsPeerId(game.whitePeerId.toStdString()), avatar);
			if (!avatar.isNull()) item->setIcon(1, QIcon(avatar));
		}
		if (!game.blackPeerId.isEmpty()) {
			QPixmap avatar;
			AvatarDefs::getAvatarFromSslId(
			        RsPeerId(game.blackPeerId.toStdString()), avatar);
			if (!avatar.isNull()) item->setIcon(2, QIcon(avatar));
		}
		item->setText(3, game.result);
		item->setText(4, QString::number(game.moves.size()));
		item->setToolTip(3, game.reason);
		for (int column = 0; column < ui->gameHistory->columnCount(); ++column)
			item->setSizeHint(column, QSize(32, 40));
	}
	const bool hasGames = !games.isEmpty();
	ui->gameHistoryDescription->setText(hasGames
	        ? tr("Double-click a saved game to replay every position.")
	        : tr("No saved games yet. Finished and interrupted games will appear here."));
	ui->reviewGameButton->setEnabled(hasGames);
	ui->exportGameButton->setEnabled(hasGames);
	ui->deleteGameButton->setEnabled(hasGames);
	if (hasGames)
		ui->gameHistory->setCurrentItem(ui->gameHistory->topLevelItem(0));
}

bool NEMainpage::selectedHistoryGame(ChessGameRecord &selected) const
{
	QTreeWidgetItem *item = ui->gameHistory->currentItem();
	if (!item) return false;
	const QString id = item->data(0, Qt::UserRole).toString();
	for (const ChessGameRecord &game : ChessGameHistory::games())
		if (game.id == id) {
			selected = game;
			return true;
		}
	return false;
}

void NEMainpage::reviewSelectedGame()
{
	ChessGameRecord game;
	if (!selectedHistoryGame(game)) return;
	ChessGameReviewDialog dialog(game, this);
	dialog.exec();
}

void NEMainpage::exportSelectedGame()
{
	ChessGameRecord game;
	if (!selectedHistoryGame(game)) return;
	const QString path = QFileDialog::getSaveFileName(
	        this, tr("Export chess game"),
	        QString("%1-vs-%2.pgn").arg(game.whitePlayer, game.blackPlayer),
	        tr("Portable Game Notation (*.pgn)"));
	if (path.isEmpty()) return;
	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly)
	        || file.write(ChessGameHistory::toPgn(game).toUtf8()) < 0
	        || !file.commit())
		QMessageBox::warning(
		        this, tr("Export chess game"),
		        tr("The PGN file could not be saved."));
}

void NEMainpage::deleteSelectedGame()
{
	ChessGameRecord game;
	if (!selectedHistoryGame(game)) return;
	if (QMessageBox::question(
	        this, tr("Delete saved game"),
	        tr("Delete the saved game between %1 and %2?")
	                .arg(game.whitePlayer, game.blackPlayer)) != QMessageBox::Yes)
		return;
	ChessGameHistory::removeGame(game.id);
	refreshGameHistory();
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

		for (RetroChessWindow *window : mGameSessions->games())
			if (window)
				window->refreshBoardTheme();
	});

}

void NEMainpage::handleEvent_identity_main_thread(std::shared_ptr<const RsEvent> event)
{
	if (!event || event->mType != RsEventType::GXS_IDENTITY) return;
	const RsGxsIdentityEvent *idEvent = dynamic_cast<const RsGxsIdentityEvent*>(event.get());
	if (!idEvent) return;

	switch (idEvent->mIdentityEventCode) {
	case RsGxsIdentityEventCode::NEW_IDENTITY:
	case RsGxsIdentityEventCode::UPDATED_IDENTITY:
	case RsGxsIdentityEventCode::DELETED_IDENTITY:
		refreshContacts();
		refreshAvailablePlayers();
		break;
	default:
		break;
	}
}

void NEMainpage::handleEvent_chat_main_thread(std::shared_ptr<const RsEvent> event)
{
	if (!event || event->mType != RsEventType::CHAT_SERVICE) return;
	const RsChatLobbyEvent *lobbyEvent = dynamic_cast<const RsChatLobbyEvent*>(event.get());
	if (!lobbyEvent) return;

	switch (lobbyEvent->mEventCode) {
	case RsChatLobbyEventCode::CHAT_LOBBY_LIST_CHANGED:
	case RsChatLobbyEventCode::CHAT_LOBBY_INVITE_RECEIVED:
		autoJoinOfficialLobby();
		break;
	default:
		break;
	}
}

