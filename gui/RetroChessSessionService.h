/*******************************************************************************
 * gui/RetroChessSessionService.h                                             *
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

#ifndef RETROCHESSSESSIONSERVICE_H
#define RETROCHESSSESSIONSERVICE_H

#include <QMap>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <retroshare/rstypes.h>

class RetroChessWindow;

/**
 * Owns the runtime association between a remote endpoint and its game window.
 *
 * Network services deliver endpoint-keyed events to this object. The main page
 * is therefore only responsible for creating/presenting windows and no longer
 * routes individual moves into them.
 */
class RetroChessSessionService : public QObject
{
	Q_OBJECT

public:
	enum EndpointType { PeerEndpoint, GxsEndpoint };

	struct Session
	{
		EndpointType endpointType;
		QString endpointId;
		QPointer<RetroChessWindow> window;
		quint64 generation;
	};

	explicit RetroChessSessionService(QObject *parent = nullptr);
	~RetroChessSessionService() override;

	bool registerGame(const QString &key, EndpointType endpointType,
	                  RetroChessWindow *window);
	void unregisterGame(const QString &key);
	bool contains(const QString &key) const;
	RetroChessWindow *game(const QString &key) const;
	QList<RetroChessWindow *> games() const;
	QStringList gameKeys() const;
	void closeAll();

	bool routeMove(const QString &key, int col, int row, int count);
	bool routeAction(const QString &key, const QString &action);
	bool routePlayerLeft(const QString &key);
	bool processPeerPacket(const RsPeerId &peerId, const QString &packet);

signals:
	void gameAdded(const QString &key);
	void gameRemoved(const QString &key);
	void unroutableEvent(const QString &key, const QString &eventType);
	void peerInviteReceived(const RsPeerId &peerId);
	void peerInviteAccepted(const RsPeerId &peerId);
	void peerRematchRequested(const RsPeerId &peerId, int remoteColor);
	void invalidPeerPacket(const RsPeerId &peerId, const QString &reason);

private:
	QMap<QString, Session> m_sessions;
	quint64 m_nextGeneration;
};

#endif // RETROCHESSSESSIONSERVICE_H
