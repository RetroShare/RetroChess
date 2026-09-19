/*******************************************************************************
 * gui/RetroChessSettings.h                                                    *
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

#ifndef RETROCHESSSETTINGS_H
#define RETROCHESSSETTINGS_H

#include <QColor>
#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QDateTime>

struct RetroChessBoardTheme
{
	QString id;
	QString name;
	QColor light;
	QColor dark;
	QColor lastMove;
};

class RetroChessSettings
{
public:
	static QStringList pieceThemes();
	static QString pieceThemeId();
	static void setPieceThemeId(const QString &id);
	static QString pieceResource(QChar color, QChar piece, const QString &theme = QString());
	static QVector<RetroChessBoardTheme> boardThemes();
	static RetroChessBoardTheme boardTheme();
	static QString boardThemeId();
	static void setBoardThemeId(const QString &id);
	static bool moveSoundEnabled();
	static bool captureSoundEnabled();
	static bool gameResultSoundEnabled();
	static bool invitationSoundEnabled();
	static void setSoundOptions(bool move, bool capture, bool gameResult, bool invitation);
	static int dateFormat();
	static void setDateFormat(int format);
	static QString formatDateTime(const QDateTime &dt);
};

class RetroChessSettingsDialog : public QDialog
{
	Q_OBJECT
public:
	explicit RetroChessSettingsDialog(QWidget *parent = nullptr, bool identitiesPage = false);
};

#endif
