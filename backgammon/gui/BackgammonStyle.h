/*******************************************************************************
 * gui/BackgammonStyle.h                                                       *
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

#include <QFile>
#include <QStyle>
#include <QWidget>

// The plugin's style sheet lives in gui/qss/Backgammon.qss (resource
// :/qss/Backgammon.qss); no styles are hard-coded in C++ or .ui files.
namespace BackgammonStyle {

inline const QString &styleSheet()
{
    static const QString sheet = [] {
        QFile file(QStringLiteral(":/qss/Backgammon.qss"));
        return file.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(file.readAll()) : QString();
    }();
    return sheet;
}

// Applies the plugin's style sheet to a top-level plugin widget.
inline void apply(QWidget *widget)
{
    widget->setStyleSheet(styleSheet());
}

// Sets the "active" property used by [active="true"] rules and restyles.
inline void setActive(QWidget *widget, bool active)
{
    if (widget->property("active").isValid() && widget->property("active").toBool() == active)
        return;
    widget->setProperty("active", active);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace BackgammonStyle
