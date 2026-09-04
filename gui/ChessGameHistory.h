/*******************************************************************************
 * gui/ChessGameHistory.h                                                     *
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

#ifndef CHESSGAMEHISTORY_H
#define CHESSGAMEHISTORY_H

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

struct ChessGameRecord
{
	QString id;
	QDateTime startedAt;
	QDateTime endedAt;
	QString whitePlayer;
	QString blackPlayer;
	QString result;
	QString reason;
	QStringList moves;
	QStringList positions;
};

class ChessGameHistory
{
public:
	static QVector<ChessGameRecord> games();
	static bool addGame(const ChessGameRecord &game);
	static bool removeGame(const QString &id);
	static QString toPgn(const ChessGameRecord &game);
};

#endif // CHESSGAMEHISTORY_H
