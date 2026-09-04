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
#include <QVector>

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
	static QVector<RetroChessBoardTheme> boardThemes();
	static RetroChessBoardTheme boardTheme();
	static QString boardThemeId();
	static void setBoardThemeId(const QString &id);
	static bool moveSoundEnabled();
	static bool captureSoundEnabled();
	static bool gameResultSoundEnabled();
	static bool invitationSoundEnabled();
	static void setSoundOptions(bool move, bool capture, bool gameResult, bool invitation);
};

class RetroChessSettingsDialog : public QDialog
{
	Q_OBJECT
public:
	explicit RetroChessSettingsDialog(QWidget *parent = nullptr);
};

#endif
