/*******************************************************************************
 * gui/ChessSpectatorsWidget.h                                                 *
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

#ifndef CHESSSPECTATORSWIDGET_H
#define CHESSSPECTATORSWIDGET_H

#include <QToolButton>
#include <retroshare/rsids.h>

class QDialog;
class QLabel;
class QTreeWidget;

// Local-host spectator count and a modeless identity list.
class ChessSpectatorsWidget : public QToolButton
{
public:
    explicit ChessSpectatorsWidget(const RsGxsId &opponent, QWidget *parent = nullptr);

private:
    void refresh();
    RsGxsId m_opponent;
    QDialog *m_dialog;
    QLabel *m_empty;
    QTreeWidget *m_watchers;
};

#endif // CHESSSPECTATORSWIDGET_H
