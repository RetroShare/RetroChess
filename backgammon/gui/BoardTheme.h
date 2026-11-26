/*******************************************************************************
 * gui/BoardTheme.h                                                            *
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

#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QPixmap>
#include <QString>

#include "core/BgRules.h"

// Colours of the board. All board items read the current theme when they
// paint, so changing it repaints every open board at once.
struct BoardTheme
{
    QString id;
    QString name;
    QColor felt;        // playing surface
    QColor frame;       // middle bar and side bars
    QColor frameLine;   // edges of the frame
    QColor tray;        // bear-off trays
    QColor pointLight;  // odd points
    QColor pointDark;   // even points
    QColor whiteChecker;
    QColor blackChecker;
    QColor highlight;   // movable checkers and possible destinations
    QColor background;  // around the board in the game window (optional)

    QColor checker(bg::Side side) const
    {
        return side == bg::White ? whiteChecker : blackChecker;
    }

    static const QList<BoardTheme> &all();
    static const BoardTheme &current();
    // Unknown ids select the first theme.
    static void setCurrent(const QString &id);
    // Fill the game window around the board with the theme's background.
    static bool useBackground();
    static void setUseBackground(bool use);

    // A small picture of a board in this theme (settings page).
    QPixmap preview(const QSize &size) const;
};

// Emits changed() when the theme changes.
class BoardThemeNotifier : public QObject
{
    Q_OBJECT
public:
    static BoardThemeNotifier &instance();
signals:
    void changed();
};
