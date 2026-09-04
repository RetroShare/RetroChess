/*******************************************************************************
 * gui/ChessGameHistory.cpp                                                   *
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

#include "ChessGameHistory.h"

#include "gui/settings/rsharesettings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace
{
const char *HISTORY_KEY = "GameHistory";
const int MAX_SAVED_GAMES = 500;

QJsonArray stringsToJson(const QStringList &values)
{
	QJsonArray result;
	for (const QString &value : values) result.append(value);
	return result;
}

QStringList stringsFromJson(const QJsonArray &values)
{
	QStringList result;
	for (const QJsonValue &value : values) result.append(value.toString());
	return result;
}

QJsonObject toJson(const ChessGameRecord &game)
{
	QJsonObject object;
	object["id"] = game.id;
	object["started"] = game.startedAt.toString(Qt::ISODateWithMs);
	object["ended"] = game.endedAt.toString(Qt::ISODateWithMs);
	object["white"] = game.whitePlayer;
	object["black"] = game.blackPlayer;
	object["result"] = game.result;
	object["reason"] = game.reason;
	object["moves"] = stringsToJson(game.moves);
	object["positions"] = stringsToJson(game.positions);
	return object;
}

ChessGameRecord fromJson(const QJsonObject &object)
{
	ChessGameRecord game;
	game.id = object["id"].toString();
	game.startedAt = QDateTime::fromString(object["started"].toString(), Qt::ISODateWithMs);
	game.endedAt = QDateTime::fromString(object["ended"].toString(), Qt::ISODateWithMs);
	game.whitePlayer = object["white"].toString();
	game.blackPlayer = object["black"].toString();
	game.result = object["result"].toString();
	game.reason = object["reason"].toString();
	game.moves = stringsFromJson(object["moves"].toArray());
	game.positions = stringsFromJson(object["positions"].toArray());
	return game;
}

bool saveGames(const QVector<ChessGameRecord> &games)
{
	QJsonArray array;
	for (const ChessGameRecord &game : games) array.append(toJson(game));
	Settings->setValueToGroup(
	        "RetroChess", HISTORY_KEY,
	        QJsonDocument(array).toJson(QJsonDocument::Compact));
	Settings->sync();
	return true;
}
}

QVector<ChessGameRecord> ChessGameHistory::games()
{
	const QByteArray encoded = Settings->valueFromGroup(
	        "RetroChess", HISTORY_KEY, QByteArray()).toByteArray();
	const QJsonDocument document = QJsonDocument::fromJson(encoded);
	QVector<ChessGameRecord> result;
	if (!document.isArray()) return result;
	for (const QJsonValue &value : document.array()) {
		if (!value.isObject()) continue;
		ChessGameRecord game = fromJson(value.toObject());
		if (!game.id.isEmpty() && !game.positions.isEmpty()) result.append(game);
	}
	return result;
}

bool ChessGameHistory::addGame(const ChessGameRecord &value)
{
	ChessGameRecord game = value;
	if (game.id.isEmpty())
		game.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
	QVector<ChessGameRecord> all = games();
	all.prepend(game);
	while (all.size() > MAX_SAVED_GAMES) all.removeLast();
	return saveGames(all);
}

bool ChessGameHistory::removeGame(const QString &id)
{
	QVector<ChessGameRecord> all = games();
	for (int index = 0; index < all.size(); ++index)
		if (all[index].id == id) {
			all.remove(index);
			return saveGames(all);
		}
	return false;
}

QString ChessGameHistory::toPgn(const ChessGameRecord &game)
{
	QString pgn = QString("[Event \"RetroChess game\"]\n"
	                      "[Date \"%1\"]\n[White \"%2\"]\n"
	                      "[Black \"%3\"]\n[Result \"%4\"]\n\n")
	        .arg(game.startedAt.date().toString("yyyy.MM.dd"),
	             game.whitePlayer, game.blackPlayer, game.result);
	for (int index = 0; index < game.moves.size(); ++index) {
		if (!(index % 2)) pgn += QString::number(index / 2 + 1) + ". ";
		pgn += game.moves[index] + ' ';
	}
	return pgn + game.result + '\n';
}
