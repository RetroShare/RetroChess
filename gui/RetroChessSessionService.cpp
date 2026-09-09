/*******************************************************************************
 * gui/RetroChessSessionService.cpp                                           *
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

#include "RetroChessSessionService.h"

#include "chess.h"

#include <QJsonDocument>
#include <QVariantMap>

RetroChessSessionService::RetroChessSessionService(QObject *parent)
    : QObject(parent), m_nextGeneration(0)
{
}

RetroChessSessionService::~RetroChessSessionService()
{
	closeAll();
}

bool RetroChessSessionService::registerGame(
        const QString &key, EndpointType endpointType, RetroChessWindow *window)
{
	if (key.isEmpty() || !window) return false;
	const quint64 generation = ++m_nextGeneration;
	Session session{
	        endpointType, key, QPointer<RetroChessWindow>(window), generation};
	m_sessions.insert(key, session);
	connect(window, &QObject::destroyed, this, [this, key, generation]() {
		// A finished window may be destroyed after a rematch has already
		// registered its replacement under the same endpoint key.
		const auto it = m_sessions.constFind(key);
		if (it == m_sessions.constEnd() || it->generation != generation) return;
		if (m_sessions.remove(key)) emit gameRemoved(key);
	});
	emit gameAdded(key);
	return true;
}

void RetroChessSessionService::unregisterGame(const QString &key)
{
	if (m_sessions.remove(key)) emit gameRemoved(key);
}

bool RetroChessSessionService::contains(const QString &key) const
{
	return game(key) != nullptr;
}

RetroChessWindow *RetroChessSessionService::game(const QString &key) const
{
	const auto it = m_sessions.constFind(key);
	return it == m_sessions.constEnd() ? nullptr : it->window.data();
}

QList<RetroChessWindow *> RetroChessSessionService::games() const
{
	QList<RetroChessWindow *> result;
	for (const Session &session : m_sessions)
		if (session.window) result.append(session.window.data());
	return result;
}

QStringList RetroChessSessionService::gameKeys() const
{
	return m_sessions.keys();
}

void RetroChessSessionService::closeAll()
{
	const auto sessions = m_sessions;
	m_sessions.clear();
	for (const Session &session : sessions)
		if (session.window) delete session.window;
}

bool RetroChessSessionService::routeMove(
        const QString &key, int col, int row, int count)
{
	RetroChessWindow *window = game(key);
	if (!window) {
		emit unroutableEvent(key, QStringLiteral("move"));
		return false;
	}
	// A remote packet must never drive the local player's pieces.
	if (window->m_flag_finished != 0 || window->turn == window->m_localplayer_turn)
		return false;
	window->validate_tile(row, col, count);
	return true;
}

bool RetroChessSessionService::routeAction(
        const QString &key, const QString &action)
{
	RetroChessWindow *window = game(key);
	if (!window) {
		emit unroutableEvent(key, QStringLiteral("game action"));
		return false;
	}
	window->applyGameAction(action, true);
	return true;
}

bool RetroChessSessionService::routePlayerLeft(const QString &key)
{
	RetroChessWindow *window = game(key);
	if (!window) {
		emit unroutableEvent(key, QStringLiteral("player left"));
		return false;
	}
	window->showPlayerLeaveMsg();
	unregisterGame(key);
	return true;
}

bool RetroChessSessionService::processPeerPacket(
        const RsPeerId &peerId, const QString &packet)
{
	const QJsonDocument document = QJsonDocument::fromJson(packet.toUtf8());
	if (!document.isObject()) {
		emit invalidPeerPacket(peerId, tr("Invalid JSON"));
		return false;
	}
	const QVariantMap data = document.toVariant().toMap();
	const QString type = data.value("type").toString();
	const QString key = QString::fromStdString(peerId.toStdString());
	if (type == "chessclick")
		return routeMove(key, data.value("col").toInt(),
		                 data.value("row").toInt(), data.value("count").toInt());
	if (type == "player_status_message") {
		if (data.value("player_status").toString() == "leave")
			return routePlayerLeft(key);
		return true;
	}
	if (type == "game_action")
		return routeAction(key, data.value("action").toString());
	if (type == "chess_invite") {
		emit peerInviteReceived(peerId); return true;
	}
	if (type == "chess_accept") {
		emit peerInviteAccepted(peerId); return true;
	}
	if (type == "chess_rematch") {
		emit peerRematchRequested(peerId, data.value("color").toInt()); return true;
	}
	emit invalidPeerPacket(peerId, tr("Unknown packet type: %1").arg(type));
	return false;
}
