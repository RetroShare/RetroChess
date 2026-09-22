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

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <retroshare/rsinit.h>

namespace
{
const char *HISTORY_KEY = "GameHistory";
const int MAX_SAVED_GAMES = 500;

QString historyFilePath()
{
	const std::string accDir = RsAccounts::AccountDirectory();
	if (accDir.empty()) {
		return QString();
	}
	return QString::fromUtf8(accDir.c_str()) + "/retrochess_history.json";
}

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
	object["whiteGxsId"] = game.whiteGxsId;
	object["blackGxsId"] = game.blackGxsId;
	object["whitePeerId"] = game.whitePeerId;
	object["blackPeerId"] = game.blackPeerId;
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
	if (!game.startedAt.isValid())
		game.startedAt = QDateTime::fromString(object["started"].toString(), Qt::ISODate);
	if (!game.startedAt.isValid())
		game.startedAt = QDateTime::fromString(object["started"].toString());
	game.endedAt = QDateTime::fromString(object["ended"].toString(), Qt::ISODateWithMs);
	if (!game.endedAt.isValid())
		game.endedAt = QDateTime::fromString(object["ended"].toString(), Qt::ISODate);
	if (!game.endedAt.isValid())
		game.endedAt = QDateTime::fromString(object["ended"].toString());
	game.whitePlayer = object["white"].toString();
	game.blackPlayer = object["black"].toString();
	game.whiteGxsId = object["whiteGxsId"].toString();
	game.blackGxsId = object["blackGxsId"].toString();
	game.whitePeerId = object["whitePeerId"].toString();
	game.blackPeerId = object["blackPeerId"].toString();
	game.result = object["result"].toString();
	game.reason = object["reason"].toString();
	game.moves = stringsFromJson(object["moves"].toArray());
	game.positions = stringsFromJson(object["positions"].toArray());
	return game;
}

QVector<ChessGameRecord> parseGamesJson(const QJsonDocument &document)
{
	QVector<ChessGameRecord> result;
	QJsonArray array;
	if (document.isArray()) {
		array = document.array();
	} else if (document.isObject()) {
		array = document.object().value("games").toArray();
	} else {
		return result;
	}

	for (const QJsonValue &value : array) {
		if (!value.isObject()) continue;
		ChessGameRecord game = fromJson(value.toObject());
		if (!game.id.isEmpty() && !game.positions.isEmpty()) result.append(game);
	}
	return result;
}

bool saveGames(const QVector<ChessGameRecord> &games)
{
	QJsonArray array;
	for (const ChessGameRecord &game : games) array.append(toJson(game));

	const QString filePath = historyFilePath();
	if (!filePath.isEmpty()) {
		QJsonObject root;
		root["version"] = 1;
		root["games"] = array;

		QSaveFile saveFile(filePath);
		if (saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			saveFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
			if (saveFile.commit()) {
				return true;
			}
			saveFile.cancelWriting();
		}
	}

	// Fallback to Settings if account directory is unavailable
	Settings->setValueToGroup(
	        "RetroChess", HISTORY_KEY,
	        QJsonDocument(array).toJson(QJsonDocument::Compact));
	Settings->sync();
	return true;
}
}

QVector<ChessGameRecord> ChessGameHistory::games()
{
	const QString filePath = historyFilePath();
	if (!filePath.isEmpty() && QFile::exists(filePath)) {
		QFile file(filePath);
		if (file.open(QIODevice::ReadOnly)) {
			const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
			return parseGamesJson(document);
		}
	}

	// Check legacy data in Settings
	const QByteArray encoded = Settings->valueFromGroup(
	        "RetroChess", HISTORY_KEY, QByteArray()).toByteArray();
	if (!encoded.isEmpty()) {
		const QJsonDocument document = QJsonDocument::fromJson(encoded);
		const QVector<ChessGameRecord> migrated = parseGamesJson(document);
		if (!migrated.isEmpty()) {
			if (!filePath.isEmpty()) {
				saveGames(migrated);
				Settings->remove(QString("RetroChess/%1").arg(HISTORY_KEY));
				Settings->sync();
			}
			return migrated;
		}
	}

	return {};
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

bool ChessGameHistory::removeGames(const QStringList &ids)
{
	if (ids.isEmpty()) return false;
	const QSet<QString> idSet(ids.begin(), ids.end());
	QVector<ChessGameRecord> all = games();
	const int oldSize = all.size();
	all.erase(std::remove_if(all.begin(), all.end(),
	        [&idSet](const ChessGameRecord &game) { return idSet.contains(game.id); }),
	        all.end());
	if (all.size() != oldSize)
		return saveGames(all);
	return false;
}

bool ChessGameHistory::removeGame(const QString &id)
{
	return removeGames(QStringList{id});
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
