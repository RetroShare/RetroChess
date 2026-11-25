/*******************************************************************************
 * gui/RetroChessDebugDialog.h                                                 *
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

// The debug window of RetroChess (layout in RetroChessDebugDialog.ui), the
// same as the one of RetroGames:
//   Traffic   – every log line of RetroChess (packets in / out, tunnel
//               requests, status changes and closes, presence probes,
//               invitations, games, leaderboard...), kept in memory by
//               services/RetroChessTunnelDebug.h whether or not the log files
//               are enabled. Filter, only problems, pause, copy.
//   Tunnels   – the GXS tunnels of RetroChess and what each is used for;
//   Contacts  – chess contacts with their presence state (failures, dormant,
//               next probe) and games / invitations per identity.
// Backgammon (backgammon/) shows up in all three tabs: its log lines in the
// areas backgammon-tunnel / backgammon-game, its tunnels and matches next to
// the chess ones.
// Opened from RetroChess settings -> General -> Debug -> "Debug window…",
// and from Backgammon settings -> Debug -> "Debug window…".

#ifndef RETROCHESSDEBUGDIALOG_H
#define RETROCHESSDEBUGDIALOG_H

#include <QDialog>
#include <QHash>
#include <QTimer>

#include <retroshare/rsgxsifacetypes.h>
#include <retroshare/rsgxstunnel.h>

#include "services/RetroChessTunnelDebug.h"

class QTreeWidgetItem;
namespace Ui { class RetroChessDebugDialog; }

class RetroChessDebugDialog : public QDialog
{
	Q_OBJECT
public:
	// One window, raised if already open. Not parented to the settings
	// dialog, so it stays open when settings are closed.
	static void showDialog();
	~RetroChessDebugDialog() override;

private:
	explicit RetroChessDebugDialog(QWidget *parent = nullptr);
	void pollEvents();
	void addEvent(const RetroChessTunnelDebug::Event &event);
	void applyFilter(QTreeWidgetItem *item) const;
	void applyFilterAll();
	void refreshTunnels();
	void refreshContacts();
	void refreshSummary();
	void copyAll();
	QString peerText(const RsGxsId &id);
	QString peerText(const QString &hexId);

	Ui::RetroChessDebugDialog *ui;
	QTimer mPoll;
	QTimer mRefresh;
	uint64_t mLastEventId = 0;
	int mIn = 0, mOut = 0, mProblems = 0;
	QHash<QString, QString> mNames;   // gxs id -> display name, once known
};

#endif // RETROCHESSDEBUGDIALOG_H
