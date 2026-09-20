/*******************************************************************************
 * gui/NEMainpage.h                                                            *
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

/* This is the main page displayed by the plugin
 *
 * For:
 * 	event handle
 * 	peer check
 *  game launching
*/

#ifndef NEMAINPAGE_H
#define NEMAINPAGE_H

#include <retroshare-gui/mainpage.h>
#include <retroshare/rsfiles.h>
#include <retroshare/rspeers.h>
#include <retroshare/rsevents.h>
#include "gui/RetroChessNotify.h"

#include "gui/chess.h"
#include "gui/ChessTimeControl.h"

#include <QWidget>
#include <QSet>
#include <memory>

class ChatDialog;
class ChatWidget;
class UserNotify;
class QShowEvent;
class RetroChessLeaderboard;
class QTableWidget;
class QLabel;
class QTreeWidgetItem;
class QTreeWidget;
class QMenu;
class RetroChessSessionService;
struct RsRetroChessAvailablePeer;

namespace Ui
{
class NEMainpage;
}

class NEMainpage : public MainPage
{
	Q_OBJECT

public:
	explicit NEMainpage(QWidget *parent, RetroChessNotify *notify);
	~NEMainpage();
	UserNotify *createUserNotify(QObject *parent) override;
	unsigned int lobbyUnreadCount() const { return mLobbyUnreadCount; }
	unsigned int incomingInviteCount() const;
	unsigned int notificationCount() const
	{
		return mLobbyUnreadCount + incomingInviteCount();
	}

signals:
	void lobbyUnreadCountChanged();

private slots:
	void refreshLeaderboard();
	void setupMenuActions();
	void NeMsgArrived(const RsPeerId &peer_id, QString str);
	void chessInvitePeer(const RsPeerId &peer_id);
	void chessAcceptedPeer(const RsPeerId &peer_id);
	void chessRematchPeer(const RsPeerId &peer_id, int remoteColor);
	void chessStart(const RsPeerId &peer_id);
	void chessStartGxs(const RsGxsId &gxs_id);
	void chessStartGxsAsBlack(const RsGxsId &gxs_id);
	void chessInviteReceivedGxs(const RsGxsId &gxs_id);
	void chessTunnelClosed(const RsGxsId &gxs_id);
	void chessMoveGxs(const RsGxsId &gxs_id, int col, int row, int count);
	void chessPlayerLeftGxs(const RsGxsId &gxs_id);
	void chessRematchGxs(const RsGxsId &gxs_id, int remoteColor);
	void chessGameActionGxs(const RsGxsId &gxs_id, QString action);
	void requestRematchGxs(const RsGxsId &gxs_id, int localColor);
	void requestRematchPeer(QString peerId, int localColor);
	void removeActiveGame(QString gameId);
	void removeActiveGameListing(QString gameId);
	void autoJoinOfficialLobby();
	void officialLobbyNewMessage(ChatWidget *chatWidget);
	void archiveFinishedGame();
	void refreshAvailablePlayers();
	void watchSelectedActiveGame();
	void chessWatchState(const RsGxsId &hostId, const QString &gameKey,
	                     const QString &whiteId, const QString &whiteName,
	                     const QString &blackId, const QString &blackName,
	                     const QString &fen, int sequence,
	                     int lastFrom = -1, int lastTo = -1,
	                     const QStringList &moves = QStringList());
	void chessWatchAction(const RsGxsId &hostId, const QString &gameKey, const QString &action);
	void chessWatchEnd(const RsGxsId &hostId, const QString &gameKey, const QString &reason);
	void onSpectatorClosed(const RsGxsId &hostId, const QString &gameKey);
private:
	RetroChessLeaderboard *mLeaderboard;
	QTableWidget *mLeaderboardTable;
	QLabel *mLeaderboardInfo;
	Ui::NEMainpage *ui;
	RetroChessNotify *mNotify;
	RetroChessSessionService *mGameSessions;
	QMap<QString, QPointer<RetroChessWindow>> mSpectatorWindows;
	ChatDialog *mOfficialLobbyDialog;
	unsigned int mLobbyUnreadCount;
	RsEventsHandlerId_t mEventHandlerId_identity = 0;
	RsEventsHandlerId_t mEventHandlerId_chat = 0;

	void handleEvent_identity_main_thread(std::shared_ptr<const RsEvent> event);
	void handleEvent_chat_main_thread(std::shared_ptr<const RsEvent> event);
	void create_chess_window(std::string peer_id, int player_id);
    void create_chess_window_gxs(const RsGxsId &gxs_id, int player_id);
	void showOfficialLobby();
	void setOfficialLobbyTabVisible(bool visible);
	void refreshGameHistory();
	void refreshActiveContactGames(const std::vector<RsRetroChessAvailablePeer> &peers);
	bool selectedHistoryGame(ChessGameRecord &game) const;
	QVector<ChessGameRecord> selectedHistoryGames() const;
	void reviewSelectedGame();
	void exportSelectedGame();
	void deleteSelectedGame();
	void saveContactsToSettings();
	void filterSavedContacts();
	void loadLayoutSettings();
	void saveLayoutSettings();
	void setupPlayersTab();
	void onCreateLobbyGame();
	void broadcastSeek(bool active, const ChessTimeControl &tc);
	QMenu *createSavedContactsContextMenu(QMenu *contextMenu = nullptr);
	void showSavedContactsHeaderContextMenu(const QPoint &globalPos);

	ChessTimeControl m_pendingSeek;
	bool m_seekActive = false;

protected:
	void showEvent(QShowEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // NEMAINPAGE_H
