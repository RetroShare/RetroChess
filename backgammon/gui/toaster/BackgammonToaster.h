/*******************************************************************************
 * gui/toaster/BackgammonToaster.h                                             *
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

#include "ui_BackgammonToaster.h"

#include <QPointer>
#include <QWidget>

class BackgammonGameWindow;

// Toaster for a new invitation: who invites, to what, and an Accept button.
// Without a window it is the preview shown in Preferences > Notify.
class BackgammonToaster : public QWidget
{
    Q_OBJECT

public:
    explicit BackgammonToaster(BackgammonGameWindow *invitation);

private slots:
    void accept();

private:
    Ui::BackgammonToaster ui;
    QPointer<BackgammonGameWindow> m_invitation;
};
