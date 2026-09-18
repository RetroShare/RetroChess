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
#include "RetroChessLeaderboard.h"
#include <QTableWidget>
#include <QLabel>
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
#include <QShortcut>
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
#include <QInputDialog>
#include <QLineEdit>
#include <QPainter>
#include <QCheckBox>
#include <QSplitter>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QDialogButtonBox>

#include "gui/common/FriendSelectionWidget.h"
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


namespace {
class ChessPlayerItem : public QTreeWidgetItem
{
public:
    explicit ChessPlayerItem(QTreeWidget *tree) : QTreeWidgetItem(tree) {}
    bool operator<(const QTreeWidgetItem &other) const override
    {
        const int column = treeWidget()->sortColumn();
        if (column == 0 || column == 1) {
            const int rank = data(1, Qt::UserRole).toInt();
            const int otherRank = other.data(1, Qt::UserRole).toInt();
            if (rank != otherRank) return rank < otherRank;
            return QString::localeAwareCompare(text(0), other.text(0)) < 0;
        }
        if (column == 3) {
            const int r1 = data(3, Qt::UserRole).toInt();
            const int r2 = other.data(3, Qt::UserRole).toInt();
            if (r1 != r2) return r1 < r2;
            const int rd1 = data(4, Qt::UserRole).toInt();
            const int rd2 = other.data(4, Qt::UserRole).toInt();
            if (rd1 != rd2) return rd1 < rd2;
            return QString::localeAwareCompare(text(0), other.text(0)) < 0;
        }
        if (column == 4 && treeWidget()->columnCount() >= 7) {
            const int rd1 = data(4, Qt::UserRole).toInt();
            const int rd2 = other.data(4, Qt::UserRole).toInt();
            if (rd1 != rd2) return rd1 < rd2;
            const int r1 = data(3, Qt::UserRole).toInt();
            const int r2 = other.data(3, Qt::UserRole).toInt();
            if (r1 != r2) return r1 < r2;
            return QString::localeAwareCompare(text(0), other.text(0)) < 0;
        }
        const bool isLastSeen = (treeWidget()->columnCount() == 3 && column == 2)
                             || (treeWidget()->columnCount() >= 7 && column == 5);
        if (isLastSeen) return data(column, Qt::UserRole).toLongLong() < other.data(column, Qt::UserRole).toLongLong();
        if (column == 7) {
            return data(7, Qt::UserRole).toInt() < other.data(7, Qt::UserRole).toInt();
        }
        return QTreeWidgetItem::operator<(other);
    }
};
}

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
	mLeaderboard = new RetroChessLeaderboard(this);
	mLeaderboardTable = ui->leaderboardTable;
	mLeaderboardInfo = ui->leaderboardInfo;
	connect(mLeaderboard, SIGNAL(changed()), this, SLOT(refreshLeaderboard()));
	if (mNotify) {
		connect(mNotify, &RetroChessNotify::gxsTunnelReady, mLeaderboard, &RetroChessLeaderboard::handleTunnelReady);
		connect(mNotify, &RetroChessNotify::leaderboardDataGxs, mLeaderboard, &RetroChessLeaderboard::handleTunnelData);
	}
	refreshLeaderboard();

    const QStringList savedContacts = Settings->valueFromGroup("RetroChess", "SavedChessContacts", QStringList()).toStringList();
    for (const QString &idStr : savedContacts) {
        rsRetroChess->addChessContact(RsGxsId(idStr.toStdString()));
    }
    // Import past opponents once; removed contacts must not return on restart.
    if (!Settings->valueFromGroup("RetroChess", "ChessContactsImported", false).toBool()) {
        for (const auto &game : ChessGameHistory::games()) {
            rsRetroChess->addChessContact(RsGxsId(game.whiteGxsId.toStdString()));
            rsRetroChess->addChessContact(RsGxsId(game.blackGxsId.toStdString()));
        }
        Settings->setValueToGroup("RetroChess", "ChessContactsImported", true);
        Settings->sync();
        saveContactsToSettings();
    }

	connect(ui->closeInfoFrameButton, &QToolButton::clicked,
	        ui->info_Frame, &QWidget::hide);
	ui->info_Frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	ui->availablePlayersTabLayout->setStretch(0, 1);
	ui->availablePlayersTabLayout->setStretch(1, 0);
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
		item->setData(0, Qt::UserRole + 1, "local");
		if (window) item->setToolTip(0, window->windowTitle());
	});
	connect(ui->active_games, &QTreeWidget::itemDoubleClicked,
	        this, [this](QTreeWidgetItem *item, int) {
		if (!item) return;
		if (item->data(0, Qt::UserRole + 1).toString() == "local") {
			const QString key = item->data(0, Qt::UserRole).toString();
			if (RetroChessWindow *window = mGameSessions->game(key)) {
				window->raise();
				window->activateWindow();
			}
		}
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
	connect(mNotify, &RetroChessNotify::chessInviteClearedGxs,
	        this, &NEMainpage::chessTunnelClosed, Qt::QueuedConnection);
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
    const int iconHeight = QFontMetricsF(ui->availablePlayers->font()).height() * 1.5;
    ui->savedContacts->setIconSize(QSize(iconHeight, iconHeight));
    ui->availablePlayers->setIconSize(QSize(iconHeight, iconHeight));

    loadLayoutSettings();

    setupPlayersTab();
	connect(ui->tabWidget, &QTabWidget::currentChanged,
	        this, [this](int index) {
		refreshAvailablePlayers();
		if (ui->tabWidget->widget(index) == ui->gameHistoryTab) {
			refreshGameHistory();
		} else if (ui->tabWidget->widget(index) == ui->leaderboardTab) {
			refreshLeaderboard();
		}
	});
	QHeaderView *historyHeader = ui->gameHistory->header();
	historyHeader->setSectionResizeMode(QHeaderView::Interactive);
	historyHeader->setSectionsMovable(true);
	historyHeader->setMinimumSectionSize(45);
	const QByteArray historyHeaderState = Settings->valueFromGroup(
	        "RetroChess", "GameHistoryHeaderState", QByteArray()).toByteArray();
	if (!historyHeaderState.isEmpty() && historyHeader->restoreState(historyHeaderState)) {
		if (historyHeader->sectionSize(4) < 50) {
			historyHeader->resizeSection(4, 90);
		}
	} else {
		historyHeader->resizeSection(0, 165);
		historyHeader->resizeSection(1, 210);
		historyHeader->resizeSection(2, 210);
		historyHeader->resizeSection(3, 85);
		historyHeader->resizeSection(4, 90);
		historyHeader->resizeSection(5, 75);
		historyHeader->setSortIndicator(0, Qt::DescendingOrder);
	}
	// Apply the date-last layout after restoring older saved column orders.
	historyHeader->moveSection(historyHeader->visualIndex(0), historyHeader->count() - 1);
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
	connect(historyHeader, &QHeaderView::sortIndicatorChanged, this,
	        [historyHeader](int, Qt::SortOrder) {
		Settings->setValueToGroup(
		        "RetroChess", "GameHistoryHeaderState", historyHeader->saveState());
	});
	ui->gameHistory->setIconSize(QSize(32, 32));
	if (ui->gameHistory->headerItem()) {
		ui->gameHistory->headerItem()->setTextAlignment(4, Qt::AlignCenter);
		ui->gameHistory->headerItem()->setTextAlignment(5, Qt::AlignCenter);
	}
	ui->gameHistory->setSelectionMode(QAbstractItemView::ExtendedSelection);
	ui->gameHistory->setContextMenuPolicy(Qt::CustomContextMenu);

	QShortcut *deleteHistoryShortcut = new QShortcut(QKeySequence::Delete, ui->gameHistory);
	connect(deleteHistoryShortcut, &QShortcut::activated,
	        this, &NEMainpage::deleteSelectedGame);

	connect(ui->gameHistory, &QTreeWidget::itemSelectionChanged, this, [this]() {
		const int selectedCount = ui->gameHistory->selectedItems().size();
		ui->reviewGameButton->setEnabled(selectedCount == 1);
		ui->exportGameButton->setEnabled(selectedCount >= 1);
		ui->deleteGameButton->setEnabled(selectedCount >= 1);
	});

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
		if (!item->isSelected()) {
			ui->gameHistory->clearSelection();
			item->setSelected(true);
			ui->gameHistory->setCurrentItem(item);
		}
		const auto selectedGames = selectedHistoryGames();
		if (selectedGames.isEmpty()) return;

		QMenu menu(ui->gameHistory);
		QAction *review = menu.addAction(tr("Review"));
		review->setEnabled(selectedGames.size() == 1);
		QAction *exportPgn = menu.addAction(tr("Export PGN"));
		menu.addSeparator();
		QAction *remove = menu.addAction(
		        selectedGames.size() > 1
		                ? tr("Delete (%1 games)").arg(selectedGames.size())
		                : tr("Delete"));
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

void NEMainpage::refreshAvailablePlayers()
{
    if (!ui->savedContacts || !ui->availablePlayers) return;
    const auto peers = rsRetroChess->availableChessPeers();
    for (QTreeWidget *tree : {ui->savedContacts, ui->availablePlayers}) {
        const bool isSavedContacts = (tree == ui->savedContacts);
        tree->setSortingEnabled(false);
        QMap<QString, QTreeWidgetItem *> rows;
        QSet<QString> retained;
        for (int row = 0; row < tree->topLevelItemCount(); ++row) {
            QTreeWidgetItem *item = tree->topLevelItem(row);
            rows.insert(item->data(0, Qt::UserRole).toString(), item);
        }
        for (const auto &peer : peers) {
            const RsGxsId id(peer.endpointId.toStdString());
            if (!peer.gxs || (rsIdentity && rsIdentity->isOwnId(id))) continue;
            const bool outgoing = rsRetroChess->hasInviteToGxs(id);
            const bool incoming = rsRetroChess->hasInviteFromGxs(id);
            QString status = peer.status;
            const RetroChessWindow *game = mGameSessions->game(peer.endpointId);
            if (game && game->m_flag_finished == 0
                    && (status == "available" || status == "busy" || status == "playing")) status = "playing";
            if (isSavedContacts ? !peer.savedContact : (status != "available" && !outgoing && !incoming)) continue;
            retained.insert(peer.endpointId);
            QTreeWidgetItem *item = rows.value(peer.endpointId);
            if (!item) item = new ChessPlayerItem(tree);
            RsIdentityDetails details;
            const bool known = rsIdentity && rsIdentity->getIdDetails(id, details);
            const QString name = known && !details.mNickname.empty()
                    ? QString::fromUtf8(details.mNickname.c_str()) : peer.endpointId;
            QPixmap avatar;
            if (!known || !details.mAvatar.mSize || !GxsIdDetails::loadPixmapFromData(
                    details.mAvatar.mData, details.mAvatar.mSize, avatar, GxsIdDetails::MEDIUM))
                avatar = GxsIdDetails::makeDefaultIcon(id, GxsIdDetails::MEDIUM);
            item->setText(0, name);
            item->setIcon(0, QIcon(avatar));
            item->setData(0, Qt::UserRole, peer.endpointId);
            item->setData(0, Qt::UserRole + 1, peer.savedContact);
            item->setToolTip(0, peer.endpointId);
            int rank = 5;
            QColor color("#808080");
            QString label = tr("Unknown");
            if (status == "available") { rank = 0; color = QColor("#278342"); label = tr("Available"); }
            else if (status == "playing") { rank = 1; color = QColor("#327fc1"); label = tr("Playing"); }
            else if (status == "busy") { rank = 2; color = QColor("#b47b16"); label = tr("Busy"); }
            else if (status == "checking") { rank = 3; label = tr("Checking..."); }
            else if (status == "offline") { rank = 4; label = tr("Offline"); }
            QPixmap dot(16, 16);
            dot.fill(Qt::transparent);
            QPainter painter(&dot);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawEllipse(3, 3, 10, 10);
            painter.end();
            item->setIcon(1, QIcon(dot));
            item->setText(1, label);
            item->setData(1, Qt::UserRole, rank);
            item->setData(1, Qt::UserRole + 1, status);
            const QString lastSeenText = peer.lastSeen
                    ? RetroChessSettings::formatDateTime(QDateTime::fromSecsSinceEpoch(peer.lastSeen))
                    : tr("Never");
            if (isSavedContacts) {
                item->setText(2, lastSeenText);
                item->setData(2, Qt::UserRole, static_cast<qlonglong>(peer.lastSeen));
            } else {
                int actionState = 0;
                if (incoming) {
                    actionState = 1; // Invited
                } else if (outgoing) {
                    actionState = 2; // Cancel
                } else if (status == "available" && (!game || game->m_flag_finished != 0)) {
                    actionState = 3; // Invite
                }

                if (actionState != 0) {
                    if (item->data(2, Qt::UserRole).toInt() != actionState || !tree->itemWidget(item, 2)) {
                        QWidget *actionWidget = new QWidget(tree);
                        QHBoxLayout *actionLayout = new QHBoxLayout(actionWidget);
                        actionLayout->setContentsMargins(2, 1, 2, 1);
                        actionLayout->setAlignment(Qt::AlignCenter);

                        QPushButton *actionBtn = new QPushButton(actionWidget);
                        actionBtn->setFont(tree->font());
                        actionBtn->setFixedHeight(22);

                        const QString endpoint = peer.endpointId;
                        if (actionState == 1) {
                            actionBtn->setText(tr("Accept"));
                            actionBtn->setToolTip(tr("Invited by player. Click to accept and start the game."));
                            actionBtn->setStyleSheet(
                                "QPushButton {"
                                "  border: 1px solid #199909; color: white; padding: 1px 8px; border-radius: 4px;"
                                "  background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 0.67, stop: 0 #22c70d, stop: 1 #116a06);"
                                "  font-weight: bold;"
                                "}"
                                "QPushButton:hover { border-color: #35d51f; }"
                                "QPushButton:pressed { background-color: #116a06; }"
                            );
                            connect(actionBtn, &QPushButton::clicked, this, [this, id]() {
                                if (rsRetroChess->hasInviteFromGxs(id)) {
                                    rsRetroChess->acceptedInviteGxs(id);
                                    mNotify->notifyChessStartGxs(id);
                                }
                                refreshAvailablePlayers();
                            });
                        } else if (actionState == 2) {
                            actionBtn->setText(tr("Pending..."));
                            actionBtn->setToolTip(tr("Click to cancel invitation"));
                            actionBtn->setStyleSheet(
                                "QPushButton {"
                                "  border: 1px solid #991919; color: white; padding: 1px 8px; border-radius: 4px;"
                                "  background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 0.67, stop: 0 #c72222, stop: 1 #6a1111);"
                                "}"
                                "QPushButton:hover { border-color: #d53535; }"
                                "QPushButton:pressed { background-color: #6a1111; }"
                            );
                            connect(actionBtn, &QPushButton::clicked, this, [this, id]() {
                                if (rsRetroChess->hasInviteToGxs(id)) {
                                    if (!rsRetroChess->cancelInviteToGxs(id)) {
                                        QMessageBox::warning(this, tr("Chess invitation"), tr("The invitation could not be updated."));
                                    }
                                }
                                refreshAvailablePlayers();
                            });
                        } else if (actionState == 3) {
                            actionBtn->setText(tr("Invite"));
                            actionBtn->setToolTip(tr("Invite to chess"));
                            actionBtn->setStyleSheet(
                                "QPushButton {"
                                "  border: 1px solid #2365a6; color: white; padding: 1px 20px; border-radius: 4px;"
                                "  background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 0.67, stop: 0 #3488db, stop: 1 #1f5f99);"
                                "}"
                                "QPushButton:hover { border-color: #5dade2; }"
                                "QPushButton:pressed { background-color: #1f5f99; }"
                            );
                            connect(actionBtn, &QPushButton::clicked, this, [this, id]() {
                                if (rsRetroChess->preferredChessIdentity().isNull()) {
                                    QMessageBox::information(this, tr("Chess invitation"),
                                        tr("Please select your chess identity in Chess profile before inviting players."));
                                    RetroChessSettingsDialog dialog(this, true);
                                    dialog.exec();
                                    refreshAvailablePlayers();
                                    return;
                                }
                                if (!rsRetroChess->sendInviteToGxs(id)) {
                                    QMessageBox::warning(this, tr("Chess invitation"), tr("The chess invitation could not be sent."));
                                }
                                refreshAvailablePlayers();
                            });
                        }
                        actionLayout->addWidget(actionBtn);
                        tree->setItemWidget(item, 2, actionWidget);
                        item->setData(2, Qt::UserRole, actionState);
                    }
                } else {
                    if (tree->itemWidget(item, 2)) {
                        tree->removeItemWidget(item, 2);
                    }
                    item->setData(2, Qt::UserRole, 0);
                    item->setText(2, QString());
                }

                RetroChessLeaderboard::Player p;
                if (mLeaderboard && mLeaderboard->getPlayer(id, p)) {
                    item->setText(3, QString::number(qRound(p.rating)));
                    item->setData(3, Qt::UserRole, qRound(p.rating));
                    item->setText(4, QString::number(qRound(p.rd)));
                    item->setData(4, Qt::UserRole, qRound(p.rd));
                    item->setToolTip(3, tr("Rating: %1 (%2, %3 games)")
                            .arg(qRound(p.rating))
                            .arg(p.provisional() ? tr("Provisional") : tr("Rated"))
                            .arg(p.games()));
                    item->setToolTip(4, tr("Rating Deviation: %1 (lower means more reliable)")
                            .arg(qRound(p.rd)));
                } else {
                    item->setText(3, tr("1500"));
                    item->setData(3, Qt::UserRole, 1500);
                    item->setText(4, tr("350"));
                    item->setData(4, Qt::UserRole, 350);
                    item->setToolTip(3, tr("Default rating: 1500 (Provisional, no games recorded yet)"));
                    item->setToolTip(4, tr("Default RD: 350 (Provisional)"));
                }
                item->setText(5, lastSeenText);
                item->setData(5, Qt::UserRole, static_cast<qlonglong>(peer.lastSeen));
                item->setText(6, outgoing && incoming ? tr("Sent / received") : outgoing ? tr("Sent")
                        : incoming ? tr("Received") : QString());

                if (incoming) {
                    if (!tree->itemWidget(item, 7)) {
                        QWidget *rejectWidget = new QWidget(tree);
                        QHBoxLayout *rejectLayout = new QHBoxLayout(rejectWidget);
                        rejectLayout->setContentsMargins(2, 1, 2, 1);
                        rejectLayout->setAlignment(Qt::AlignCenter);

                        QPushButton *rejectBtn = new QPushButton(rejectWidget);
                        rejectBtn->setFont(tree->font());
                        rejectBtn->setFixedHeight(22);
                        rejectBtn->setText(tr("Reject"));
                        rejectBtn->setToolTip(tr("Reject chess invitation"));
                        rejectBtn->setStyleSheet(
                            "QPushButton {"
                            "  border: 1px solid #991919; color: white; padding: 1px 8px; border-radius: 4px;"
                            "  background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 0.67, stop: 0 #c72222, stop: 1 #6a1111);"
                            "}"
                            "QPushButton:hover { border-color: #d53535; }"
                            "QPushButton:pressed { background-color: #6a1111; }"
                        );

                        connect(rejectBtn, &QPushButton::clicked, this, [this, id]() {
                            if (rsRetroChess->hasInviteFromGxs(id)) {
                                if (!rsRetroChess->rejectedInviteGxs(id)) {
                                    QMessageBox::warning(this, tr("Chess invitation"),
                                            tr("The rejection could not be sent. Please try again."));
                                }
                            }
                            refreshAvailablePlayers();
                        });
                        rejectLayout->addWidget(rejectBtn);
                        tree->setItemWidget(item, 7, rejectWidget);
                    }
                    item->setData(7, Qt::UserRole, 1);
                    item->setText(7, QString());
                } else {
                    if (tree->itemWidget(item, 7)) {
                        tree->removeItemWidget(item, 7);
                    }
                    item->setData(7, Qt::UserRole, 0);
                    item->setText(7, QString());
                }
            }
        }
        for (auto it = rows.constBegin(); it != rows.constEnd(); ++it)
            if (!retained.contains(it.key())) delete it.value();
        tree->setSortingEnabled(true);
    }
    filterSavedContacts();
    ui->availablePlayersDescription->setText(tr("Saved chess contacts keeps all saved players, including offline contacts. Available or invited players shows players ready for a game and incoming or outgoing invitations. Right-click or double-click a player for actions."));
    refreshActiveContactGames(peers);
    emit lobbyUnreadCountChanged();
}

void NEMainpage::refreshActiveContactGames(const std::vector<RsRetroChessAvailablePeer> &peers)
{
	if (!ui->active_games) return;

	struct MatchInfo {
		QString nameA;
		QString idA;
		QString nameB;
		QString idB;
	};
	QMap<QString, MatchInfo> activeMatches;

	for (const auto &peer : peers) {
		if (peer.status != "playing" || peer.opponentId.isEmpty()) continue;
		const QString idA = peer.endpointId;
		const QString idB = peer.opponentId;

		// Ignore if this is our own identity or a local game we participate in
		if (mGameSessions->contains(idA) || mGameSessions->contains(idB)) continue;
		if (rsIdentity && (rsIdentity->isOwnId(RsGxsId(idA.toStdString())) || rsIdentity->isOwnId(RsGxsId(idB.toStdString()))))
			continue;

		// Canonical match key so A vs B and B vs A are merged
		const QString canonicalKey = "contact_game:" + (idA < idB ? (idA + "_" + idB) : (idB + "_" + idA));

		RsIdentityDetails detailsA;
		const bool knownA = rsIdentity && rsIdentity->getIdDetails(RsGxsId(idA.toStdString()), detailsA);
		const QString nameA = knownA && !detailsA.mNickname.empty()
		        ? QString::fromUtf8(detailsA.mNickname.c_str()) : idA.left(12);

		QString nameB = peer.opponentName;
		if (nameB.isEmpty()) {
			RsIdentityDetails detailsB;
			if (rsIdentity && rsIdentity->getIdDetails(RsGxsId(idB.toStdString()), detailsB) && !detailsB.mNickname.empty())
				nameB = QString::fromUtf8(detailsB.mNickname.c_str());
			else
				nameB = idB.left(12);
		}

		if (!activeMatches.contains(canonicalKey)) {
			activeMatches[canonicalKey] = {nameA, idA, nameB, idB};
		} else {
			auto &existing = activeMatches[canonicalKey];
			if (existing.idA == idB && existing.nameA == idB.left(12) && !nameB.isEmpty() && nameB != idB.left(12))
				existing.nameA = nameB;
			if (existing.idB == idA && existing.nameB == idA.left(12) && !nameA.isEmpty() && nameA != idA.left(12))
				existing.nameB = nameA;
		}
	}

	QSet<QString> retainedKeys;
	for (int row = ui->active_games->topLevelItemCount() - 1; row >= 0; --row) {
		QTreeWidgetItem *item = ui->active_games->topLevelItem(row);
		if (!item) continue;
		if (item->data(0, Qt::UserRole + 1).toString() == "contact") {
			const QString key = item->data(0, Qt::UserRole).toString();
			if (activeMatches.contains(key)) {
				retainedKeys.insert(key);
				const auto &m = activeMatches.value(key);
				item->setText(0, tr("%1 — %2 (Contact Match)").arg(m.nameA, m.nameB));
				item->setToolTip(0, tr("Active match between %1 (%2) and %3 (%4)").arg(m.nameA, m.idA, m.nameB, m.idB));
			} else {
				delete ui->active_games->takeTopLevelItem(row);
			}
		}
	}

	for (auto it = activeMatches.constBegin(); it != activeMatches.constEnd(); ++it) {
		if (!retainedKeys.contains(it.key())) {
			QTreeWidgetItem *item = new QTreeWidgetItem(ui->active_games);
			const auto &m = it.value();
			item->setText(0, tr("%1 — %2 (Contact Match)").arg(m.nameA, m.nameB));
			item->setText(1, tr("Contact Match"));
			item->setData(0, Qt::UserRole, it.key());
			item->setData(0, Qt::UserRole + 1, "contact");
			item->setToolTip(0, tr("Active match between %1 (%2) and %3 (%4)").arg(m.nameA, m.idA, m.nameB, m.idB));
		}
	}
}


NEMainpage::~NEMainpage()
{
	saveLayoutSettings();
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
	if (!rsChats || !rsIdentity) return;

	std::list<ChatLobbyId> subscribedLobbies;
	rsChats->getChatLobbyList(subscribedLobbies);
	if (std::find(subscribedLobbies.begin(), subscribedLobbies.end(),
	              OFFICIAL_RETROCHESS_LOBBY_ID) != subscribedLobbies.end()) {
        // The setter emits CHAT_LOBBY_LIST_CHANGED even if already enabled.
        // Writing it unconditionally creates an endless queued GUI event loop.
        if (!rsChats->getLobbyAutoSubscribe(OFFICIAL_RETROCHESS_LOBBY_ID))
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
        // The setter emits CHAT_LOBBY_LIST_CHANGED even if already enabled.
        // Writing it unconditionally creates an endless queued GUI event loop.
        if (!rsChats->getLobbyAutoSubscribe(OFFICIAL_RETROCHESS_LOBBY_ID))
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

void NEMainpage::refreshLeaderboard()
{
	mLeaderboard->populate(mLeaderboardTable);
	mLeaderboardInfo->setText(
	        tr("Standard Glicko-2 ratings synchronized via GXS tunnels. Games count after both players exchange matching signed receipts."));
	refreshAvailablePlayers();
}

void NEMainpage::showEvent(QShowEvent *event)
{
	if (mLobbyUnreadCount) {
		mLobbyUnreadCount = 0;
		emit lobbyUnreadCountChanged();
	}
	MainPage::showEvent(event);
	refreshAvailablePlayers();
	refreshLeaderboard();
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
	// We accepted the remote participant's invitation: we are Black.
	create_chess_window_gxs(gxs_id, 1);
}

namespace
{
class ChessGameHistoryItem : public QTreeWidgetItem
{
public:
	explicit ChessGameHistoryItem()
	    : QTreeWidgetItem(), m_movesCount(0)
	{}

	QDateTime m_endedAt;
	int m_movesCount;

	bool operator<(const QTreeWidgetItem &other) const override
	{
		int column = 0;
		if (treeWidget()) {
			column = treeWidget()->sortColumn();
			if (column < 0 && treeWidget()->header())
				column = treeWidget()->header()->sortIndicatorSection();
		}
		if (column < 0) column = 0;

		const auto *otherItem = dynamic_cast<const ChessGameHistoryItem*>(&other);
		if (otherItem) {
			if (column == 0) {
				if (m_endedAt.isValid() && otherItem->m_endedAt.isValid()) {
					if (m_endedAt != otherItem->m_endedAt)
						return m_endedAt < otherItem->m_endedAt;
				} else if (m_endedAt.isValid() != otherItem->m_endedAt.isValid()) {
					return !m_endedAt.isValid();
				}
			} else if (column == 5) {
				if (m_movesCount != otherItem->m_movesCount)
					return m_movesCount < otherItem->m_movesCount;
				if (m_endedAt != otherItem->m_endedAt)
					return m_endedAt < otherItem->m_endedAt;
			} else if (column == 4) {
				if (m_endedAt != otherItem->m_endedAt)
					return m_endedAt < otherItem->m_endedAt;
			}
		}

		return text(column).localeAwareCompare(other.text(column)) < 0;
	}
};

}

void NEMainpage::chessInviteReceivedGxs(const RsGxsId &gxs_id)
{
	Q_UNUSED(gxs_id);
	refreshAvailablePlayers();
}

void NEMainpage::chessTunnelClosed(const RsGxsId &gxs_id)
{
	Q_UNUSED(gxs_id);
	refreshAvailablePlayers();
}

unsigned int NEMainpage::incomingInviteCount() const
{
	unsigned int count = 0;
	if (ui && ui->availablePlayers) {
		for (int i = 0; i < ui->availablePlayers->topLevelItemCount(); ++i) {
			QTreeWidgetItem *item = ui->availablePlayers->topLevelItem(i);
			if (item && item->data(7, Qt::UserRole).toInt() == 1) {
				++count;
			}
		}
	}
	return count;
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
	rsRetroChess->startNewGameIdForPeer(gxs_id);
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
	        this, tr("Rematch"), tr("Your opponent requests a rematch. Accept?"))
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
    connect(win, &RetroChessWindow::ratedResult, mLeaderboard, &RetroChessLeaderboard::submitResult);
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
    const QString endpoint = window->mIsGxs ? QString::fromStdString(window->mGxsId.toStdString())
            : QString::fromStdString(window->mPeerId);
    rsRetroChess->unregisterGameSession(endpoint);
	refreshGameHistory();
    refreshAvailablePlayers();
}

void NEMainpage::refreshGameHistory()
{
	ui->gameHistory->setSortingEnabled(false);
	ui->gameHistory->clear();
	const QVector<ChessGameRecord> games = ChessGameHistory::games();
	for (const ChessGameRecord &game : games) {
		ChessGameHistoryItem *item = new ChessGameHistoryItem();
		item->m_endedAt = game.endedAt;
		item->m_movesCount = game.moves.size();
		item->setData(0, Qt::UserRole, game.id);
		item->setText(0, RetroChessSettings::formatDateTime(
		        game.endedAt.toLocalTime()));
		item->setText(1, game.whitePlayer);
		item->setText(2, game.blackPlayer);
		auto setGxsIdentity = [item](
		        int column, const QString &idText, const QString &storedName) {
			if (idText.isEmpty()) return;
			const RsGxsId id(idText.toStdString());
			RsIdentityDetails details;
			QPixmap pixmap;
			QString name = storedName;
			const bool detailsAvailable = rsIdentity && rsIdentity->getIdDetails(id, details);
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
		if (game.whiteGxsId.isEmpty() && !game.whitePeerId.isEmpty()) {
			QPixmap avatar;
			if (AvatarDefs::getAvatarFromSslId(
			        RsPeerId(game.whitePeerId.toStdString()), avatar) && !avatar.isNull())
				item->setIcon(1, QIcon(avatar));
		}
		if (game.blackGxsId.isEmpty() && !game.blackPeerId.isEmpty()) {
			QPixmap avatar;
			if (AvatarDefs::getAvatarFromSslId(
			        RsPeerId(game.blackPeerId.toStdString()), avatar) && !avatar.isNull())
				item->setIcon(2, QIcon(avatar));
		}
		item->setText(3, game.result);
		item->setText(4, QString());
		item->setText(5, QString::number(game.moves.size()));
		item->setTextAlignment(5, Qt::AlignCenter);
		item->setToolTip(3, game.reason);
		for (int column = 0; column < ui->gameHistory->columnCount(); ++column)
			item->setSizeHint(column, QSize(32, 40));
		ui->gameHistory->addTopLevelItem(item);

		QWidget *reviewWidget = new QWidget(ui->gameHistory);
		QHBoxLayout *reviewLayout = new QHBoxLayout(reviewWidget);
		reviewLayout->setContentsMargins(4, 2, 4, 2);
		reviewLayout->setAlignment(Qt::AlignCenter);

		QPushButton *reviewBtn = new QPushButton(tr("Review"), reviewWidget);
		reviewBtn->setFont(ui->gameHistory->font());
		reviewBtn->setFixedHeight(24);
		reviewBtn->setCursor(Qt::PointingHandCursor);
		reviewBtn->setToolTip(tr("Review and replay this game"));

		connect(reviewBtn, &QPushButton::clicked, this, [this, item, game]() {
			ui->gameHistory->clearSelection();
			item->setSelected(true);
			ui->gameHistory->setCurrentItem(item);
			ChessGameReviewDialog *dialog = new ChessGameReviewDialog(game, this);
			dialog->show();
			dialog->raise();
			dialog->activateWindow();
		});
		reviewLayout->addWidget(reviewBtn);
		ui->gameHistory->setItemWidget(item, 4, reviewWidget);
	}
	ui->gameHistory->setSortingEnabled(true);
	int sortCol = ui->gameHistory->header()->sortIndicatorSection();
	Qt::SortOrder sortOrder = ui->gameHistory->header()->sortIndicatorOrder();
	if (sortCol < 0) {
		sortCol = 0;
		sortOrder = Qt::DescendingOrder;
	}
	ui->gameHistory->sortByColumn(sortCol, sortOrder);

	const bool hasGames = !games.isEmpty();
	ui->gameHistoryDescription->setText(hasGames
	        ? tr("Double-click a saved game to replay every position.")
	        : tr("No saved games yet. Finished and interrupted games will appear here."));
	if (hasGames) {
		ui->gameHistory->setCurrentItem(ui->gameHistory->topLevelItem(0));
	} else {
		ui->reviewGameButton->setEnabled(false);
		ui->exportGameButton->setEnabled(false);
		ui->deleteGameButton->setEnabled(false);
	}
}

bool NEMainpage::selectedHistoryGame(ChessGameRecord &selected) const
{
	QTreeWidgetItem *item = ui->gameHistory->currentItem();
	if (!item && !ui->gameHistory->selectedItems().isEmpty())
		item = ui->gameHistory->selectedItems().first();
	if (!item) return false;
	const QString id = item->data(0, Qt::UserRole).toString();
	for (const ChessGameRecord &game : ChessGameHistory::games())
		if (game.id == id) {
			selected = game;
			return true;
		}
	return false;
}

QVector<ChessGameRecord> NEMainpage::selectedHistoryGames() const
{
	QList<QTreeWidgetItem *> items = ui->gameHistory->selectedItems();
	if (items.isEmpty() && ui->gameHistory->currentItem())
		items.append(ui->gameHistory->currentItem());
	if (items.isEmpty()) return {};
	QSet<QString> selectedIds;
	for (QTreeWidgetItem *item : items)
		selectedIds.insert(item->data(0, Qt::UserRole).toString());
	QVector<ChessGameRecord> selected;
	for (const ChessGameRecord &game : ChessGameHistory::games())
		if (selectedIds.contains(game.id))
			selected.append(game);
	return selected;
}

void NEMainpage::reviewSelectedGame()
{
	ChessGameRecord game;
	if (!selectedHistoryGame(game)) return;
	ChessGameReviewDialog *dialog = new ChessGameReviewDialog(game, this);
	dialog->show();
	dialog->raise();
	dialog->activateWindow();
}

void NEMainpage::exportSelectedGame()
{
	const QVector<ChessGameRecord> games = selectedHistoryGames();
	if (games.isEmpty()) return;

	QString defaultFileName;
	if (games.size() == 1) {
		defaultFileName = QString("%1-vs-%2.pgn")
		        .arg(games.first().whitePlayer, games.first().blackPlayer);
	} else {
		defaultFileName = QString("chess-games-%1.pgn").arg(games.size());
	}

	const QString path = QFileDialog::getSaveFileName(
	        this, tr("Export chess game"),
	        defaultFileName,
	        tr("Portable Game Notation (*.pgn)"));
	if (path.isEmpty()) return;

	QString pgnContent;
	for (const ChessGameRecord &game : games) {
		if (!pgnContent.isEmpty()) pgnContent += "\n\n";
		pgnContent += ChessGameHistory::toPgn(game);
	}

	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly)
	        || file.write(pgnContent.toUtf8()) < 0
	        || !file.commit())
		QMessageBox::warning(
		        this, tr("Export chess game"),
		        tr("The PGN file could not be saved."));
}

void NEMainpage::deleteSelectedGame()
{
	const QVector<ChessGameRecord> games = selectedHistoryGames();
	if (games.isEmpty()) return;

	if (games.size() == 1) {
		const auto &game = games.first();
		if (QMessageBox::question(
		        this, tr("Delete saved game"),
		        tr("Delete the saved game between %1 and %2?")
		                .arg(game.whitePlayer, game.blackPlayer)) != QMessageBox::Yes)
			return;
	} else {
		if (QMessageBox::question(
		        this, tr("Delete saved games"),
		        tr("Delete the %1 selected saved games?")
		                .arg(games.size())) != QMessageBox::Yes)
			return;
	}

	QStringList ids;
	ids.reserve(games.size());
	for (const ChessGameRecord &game : games)
		ids.append(game.id);

	ChessGameHistory::removeGames(ids);
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

		refreshAvailablePlayers();
		refreshLeaderboard();
		refreshGameHistory();
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
		refreshAvailablePlayers();
		refreshLeaderboard();
		refreshGameHistory();
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

void NEMainpage::saveContactsToSettings()
{
	QStringList ids;
	const auto peers = rsRetroChess->availableChessPeers();
	for (const auto &peer : peers) {
		if (peer.savedContact) ids << peer.endpointId;
	}
	Settings->setValueToGroup("RetroChess", "SavedChessContacts", ids);
	Settings->sync();
}

void NEMainpage::filterSavedContacts()
{
	if (!ui || !ui->savedContacts) return;
	const QString contactQuery = ui->contactSearchEdit ? ui->contactSearchEdit->text().trimmed() : QString();
	const bool onlyOnline = ui->showOnlineplayersButton && ui->showOnlineplayersButton->isChecked();

	for (int i = 0; i < ui->savedContacts->topLevelItemCount(); ++i) {
		QTreeWidgetItem *item = ui->savedContacts->topLevelItem(i);
		if (!item) continue;
		const bool matchQuery = contactQuery.isEmpty()
		        || item->text(0).contains(contactQuery, Qt::CaseInsensitive)
		        || item->data(0, Qt::UserRole).toString().contains(contactQuery, Qt::CaseInsensitive);
		const QString status = item->data(1, Qt::UserRole + 1).toString();
		const bool isOnline = (status == "available" || status == "playing" || status == "busy");
		const bool match = matchQuery && (!onlyOnline || isOnline);
		item->setHidden(!match);
	}
}

void NEMainpage::loadLayoutSettings()
{
	ui->playersSplitter->setChildrenCollapsible(false);
	ui->playersSplitter->setStretchFactor(0, 1);
	ui->playersSplitter->setStretchFactor(1, 2);

	const QByteArray splitterState = Settings->valueFromGroup("RetroChess", "PlayersSplitterState", QByteArray()).toByteArray();
	if (!splitterState.isEmpty()) {
		ui->playersSplitter->restoreState(splitterState);
	} else {
		ui->playersSplitter->setSizes({320, 680});
	}

	ui->savedContacts->header()->setSectionResizeMode(QHeaderView::Interactive);
	ui->savedContacts->header()->setStretchLastSection(false);

	ui->availablePlayers->header()->setSectionResizeMode(QHeaderView::Interactive);
	ui->availablePlayers->header()->setStretchLastSection(false);

	const QByteArray savedHeader = Settings->valueFromGroup("RetroChess", "SavedContactsHeaderState", QByteArray()).toByteArray();
	bool restoredSaved = false;
	if (!savedHeader.isEmpty()) {
		restoredSaved = ui->savedContacts->header()->restoreState(savedHeader);
	}
	if (!restoredSaved) {
		const QVariantList savedWidths = Settings->valueFromGroup("RetroChess", "SavedContactsColumnWidths", QVariantList()).toList();
		if (savedWidths.size() == ui->savedContacts->columnCount()) {
			for (int col = 0; col < savedWidths.size(); ++col) {
				ui->savedContacts->setColumnWidth(col, savedWidths[col].toInt());
			}
		} else {
			ui->savedContacts->setColumnWidth(0, 200);
			ui->savedContacts->setColumnWidth(1, 90);
			ui->savedContacts->setColumnWidth(2, 130);
		}
	}

	const QByteArray availableHeader = Settings->valueFromGroup("RetroChess", "AvailablePlayersHeaderState", QByteArray()).toByteArray();
	bool restoredAvailable = false;
	const QVariantList availableWidths = Settings->valueFromGroup("RetroChess", "AvailablePlayersColumnWidths", QVariantList()).toList();
	if (!availableHeader.isEmpty() && availableWidths.size() == ui->availablePlayers->columnCount()) {
		restoredAvailable = ui->availablePlayers->header()->restoreState(availableHeader);
	}
	if (!restoredAvailable) {
		if (availableWidths.size() == ui->availablePlayers->columnCount()) {
			for (int col = 0; col < availableWidths.size(); ++col) {
				ui->availablePlayers->setColumnWidth(col, availableWidths[col].toInt());
			}
		} else {
			ui->availablePlayers->setColumnWidth(0, 180);
			ui->availablePlayers->setColumnWidth(1, 130);
			ui->availablePlayers->setColumnWidth(2, 130);
			ui->availablePlayers->setColumnWidth(3, 70);
			ui->availablePlayers->setColumnWidth(4, 60);
			ui->availablePlayers->setColumnWidth(5, 120);
			ui->availablePlayers->setColumnWidth(6, 100);
			ui->availablePlayers->setColumnWidth(7, 85);
		}
	}
	if (ui->availablePlayers->columnWidth(1) < 130) ui->availablePlayers->setColumnWidth(1, 130);
	if (ui->availablePlayers->columnWidth(2) < 130) ui->availablePlayers->setColumnWidth(2, 130);

	const bool onlyOnline = Settings->valueFromGroup("RetroChess", "ShowOnlyOnlineContacts", false).toBool();
	ui->showOnlineplayersButton->setChecked(onlyOnline);
	ui->showOnlineplayersButton->setToolTip(onlyOnline
	        ? tr("Show all chess players")
	        : tr("Show only online chess players"));

	connect(ui->savedContacts->header(), &QHeaderView::sectionResized,
	        this, &NEMainpage::saveLayoutSettings);
	connect(ui->availablePlayers->header(), &QHeaderView::sectionResized,
	        this, &NEMainpage::saveLayoutSettings);
	connect(ui->playersSplitter, &QSplitter::splitterMoved,
	        this, &NEMainpage::saveLayoutSettings);
}

void NEMainpage::saveLayoutSettings()
{
	Settings->setValueToGroup("RetroChess", "PlayersSplitterState", ui->playersSplitter->saveState());
	Settings->setValueToGroup("RetroChess", "SavedContactsHeaderState", ui->savedContacts->header()->saveState());
	Settings->setValueToGroup("RetroChess", "AvailablePlayersHeaderState", ui->availablePlayers->header()->saveState());
	Settings->setValueToGroup("RetroChess", "GameHistoryHeaderState", ui->gameHistory->header()->saveState());
	Settings->setValueToGroup("RetroChess", "ShowOnlyOnlineContacts", ui->showOnlineplayersButton->isChecked());

	QVariantList savedWidths;
	for (int col = 0; col < ui->savedContacts->columnCount(); ++col) {
		savedWidths.append(ui->savedContacts->columnWidth(col));
	}
	Settings->setValueToGroup("RetroChess", "SavedContactsColumnWidths", savedWidths);

	QVariantList availableWidths;
	for (int col = 0; col < ui->availablePlayers->columnCount(); ++col) {
		availableWidths.append(ui->availablePlayers->columnWidth(col));
	}
	Settings->setValueToGroup("RetroChess", "AvailablePlayersColumnWidths", availableWidths);

	Settings->sync();
}

void NEMainpage::setupPlayersTab()
{
	for (QTreeWidget *tree : {ui->savedContacts, ui->availablePlayers}) {
		tree->sortItems(0, Qt::AscendingOrder);
		tree->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(tree, &QTreeWidget::customContextMenuRequested,
		        this, [this, tree](const QPoint &position) {
			const QTreeWidgetItem *item = tree->itemAt(position);
			if (!item) return;
			const QString endpoint = item->data(0, Qt::UserRole).toString();
			const RsGxsId id(endpoint.toStdString());
			const bool saved = item->data(0, Qt::UserRole + 1).toBool();
			const bool outgoing = rsRetroChess->hasInviteToGxs(id);
			const bool incoming = rsRetroChess->hasInviteFromGxs(id);
			QMenu menu(this);
			QAction *invite = menu.addAction(outgoing ? tr("Cancel invitation") : tr("Invite to chess"));
			invite->setEnabled(outgoing || (!incoming && item->data(1, Qt::UserRole + 1).toString() == "available"
			        && (!mGameSessions->game(endpoint) || mGameSessions->game(endpoint)->m_flag_finished != 0)
			        && !rsRetroChess->preferredChessIdentity().isNull()));
			QAction *accept = incoming ? menu.addAction(tr("Accept invitation")) : nullptr;
			QAction *decline = incoming ? menu.addAction(tr("Decline invitation")) : nullptr;
			QAction *chatAction = nullptr;
			if (tree == ui->savedContacts) {
				chatAction = menu.addAction(QIcon(":/icons/png/chats.png"), tr("Start private chat"));
				chatAction->setEnabled(!id.isNull());
			}
			QAction *contact = nullptr;
			if (tree == ui->savedContacts) {
				menu.addSeparator();
				contact = menu.addAction(tr("Remove chess contact"));
			} else if (!saved) {
				menu.addSeparator();
				contact = menu.addAction(tr("Save chess contact"));
			}
			QAction *chosen = menu.exec(tree->viewport()->mapToGlobal(position));
			if (!chosen) return;
			if (contact && chosen == contact) {
				if (tree == ui->savedContacts) rsRetroChess->removeChessContact(id);
				else rsRetroChess->addChessContact(id);
				saveContactsToSettings();
			} else if (chosen == accept && rsRetroChess->hasInviteFromGxs(id)) {
				rsRetroChess->acceptedInviteGxs(id);
				mNotify->notifyChessStartGxs(id);
			} else if (chosen == decline && rsRetroChess->hasInviteFromGxs(id)) {
				rsRetroChess->rejectedInviteGxs(id);
			} else if (chosen == invite) {
				const bool ok = outgoing ? rsRetroChess->cancelInviteToGxs(id) : rsRetroChess->sendInviteToGxs(id);
				if (!ok) QMessageBox::warning(this, tr("Chess invitation"), tr("The invitation could not be updated."));
			} else if (chatAction && chosen == chatAction) {
				RsGxsId own_id = rsRetroChess->preferredChessIdentity();
				if (own_id.isNull() && rsIdentity) {
					std::list<RsGxsId> own_ids;
					if (rsIdentity->getOwnIds(own_ids) && !own_ids.empty())
						own_id = own_ids.front();
				}
				if (own_id.isNull()) {
					QMessageBox::information(this, tr("Private chat"),
						tr("Please select or create an identity before starting a private chat."));
					return;
				}
				DistantChatPeerId dpid;
				uint32_t error_code = 0;
				if (!rsChats || !rsChats->initiateDistantChatConnexion(id, own_id, dpid, error_code)) {
					QString error_str;
					switch (error_code) {
						case RS_DISTANT_CHAT_ERROR_DECRYPTION_FAILED:  error_str = tr("Decryption failed."); break;
						case RS_DISTANT_CHAT_ERROR_SIGNATURE_MISMATCH: error_str = tr("Signature mismatch."); break;
						case RS_DISTANT_CHAT_ERROR_UNKNOWN_KEY:        error_str = tr("Unknown key."); break;
						case RS_DISTANT_CHAT_ERROR_UNKNOWN_HASH:       error_str = tr("Unknown hash."); break;
						default:                                       error_str = tr("Unknown error."); break;
					}
					QMessageBox::warning(this, tr("Cannot start private chat"),
						tr("Private chat could not be initiated: %1 (code %2)").arg(error_str).arg(error_code));
				} else {
					ChatDialog::chatFriend(ChatId(dpid), true);
				}
			}
			refreshAvailablePlayers();
		});
		connect(tree, &QTreeWidget::itemDoubleClicked,
		        this, [this](QTreeWidgetItem *item, int) {
			if (!item) return;
			const QString endpoint = item->data(0, Qt::UserRole).toString();
			const RsGxsId id(endpoint.toStdString());
			const bool incoming = rsRetroChess->hasInviteFromGxs(id);
			if (incoming) {
				rsRetroChess->acceptedInviteGxs(id);
				mNotify->notifyChessStartGxs(id);
				refreshAvailablePlayers();
				return;
			}
			if (item->data(1, Qt::UserRole + 1).toString() == "available"
			        && !rsRetroChess->hasInviteToGxs(id)
			        && (!mGameSessions->game(endpoint) || mGameSessions->game(endpoint)->m_flag_finished != 0)
			        && !rsRetroChess->preferredChessIdentity().isNull()) {
				if (!rsRetroChess->sendInviteToGxs(id))
					QMessageBox::warning(this, tr("Chess invitation"), tr("The chess invitation could not be sent."));
				refreshAvailablePlayers();
			}
		});
	}

	ui->busyCheckBox->setChecked(rsRetroChess->chessBusy());
	connect(ui->busyCheckBox, &QCheckBox::toggled, this, [](bool value) { rsRetroChess->setChessBusy(value); });
	connect(ui->identitiesButton, &QPushButton::clicked, this, [this]() {
		RetroChessSettingsDialog dialog(this, true);
		if (dialog.exec() == QDialog::Accepted) {
			refreshAvailablePlayers();
			refreshLeaderboard();
			refreshGameHistory();
		}
	});
	ui->showOnlineplayersButton->setIcon(QIcon(":/images/chess-knight.svg"));
	ui->showOnlineplayersButton->setIconSize(QSize(24, 24));
	ui->showOnlineplayersButton->setCheckable(true);
	ui->showOnlineplayersButton->setAutoRaise(true);
	ui->showOnlineplayersButton->setToolTip(ui->showOnlineplayersButton->isChecked()
	        ? tr("Show all chess players")
	        : tr("Show only online chess players"));

	connect(ui->contactSearchEdit, &QLineEdit::textChanged, this, [this](const QString &) {
		filterSavedContacts();
	});
	connect(ui->showOnlineplayersButton, &QToolButton::toggled, this, [this](bool checked) {
		ui->showOnlineplayersButton->setToolTip(checked
		        ? tr("Show all chess players")
		        : tr("Show only online chess players"));
		filterSavedContacts();
		saveLayoutSettings();
	});
	connect(ui->addChessPlayerButton, &QToolButton::clicked, this, [this]() {
		QDialog dialog(this);
		dialog.setWindowTitle(tr("Add Chess Player"));
		dialog.resize(500, 540);
		QVBoxLayout *layout = new QVBoxLayout(&dialog);

		FriendSelectionWidget *friendsWidget = new FriendSelectionWidget(&dialog);
		friendsWidget->setHeaderText(tr("Select GXS identities to add as chess contacts:"));
		friendsWidget->setModus(FriendSelectionWidget::MODUS_MULTI);
		friendsWidget->setShowType(FriendSelectionWidget::SHOW_GXS);
		friendsWidget->start();
		layout->addWidget(friendsWidget);

		QDialogButtonBox *buttonBox = new QDialogButtonBox(
		        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
		layout->addWidget(buttonBox);

		connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
		connect(friendsWidget, &FriendSelectionWidget::doubleClicked,
		        &dialog, [&dialog, friendsWidget, this](int idType, const QString &id) {
			if (idType == FriendSelectionWidget::IDTYPE_GXS && !id.isEmpty()) {
				const RsGxsId gxsId(id.toStdString());
				if (!gxsId.isNull() && (!rsIdentity || !rsIdentity->isOwnId(gxsId))) {
					rsRetroChess->addChessContact(gxsId);
				}
			}
			dialog.accept();
		});

		if (dialog.exec() == QDialog::Accepted) {
			std::set<RsGxsId> selected;
			friendsWidget->selectedIds<RsGxsId, FriendSelectionWidget::IDTYPE_GXS>(selected, false);
			for (const auto &id : selected) {
				if (!id.isNull() && (!rsIdentity || !rsIdentity->isOwnId(id))) {
					rsRetroChess->addChessContact(id);
				}
			}
			saveContactsToSettings();
			refreshAvailablePlayers();
		}
	});
}




