/*******************************************************************************
 * gui/BackgammonSettingsDialog.h                                              *
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

#include "BackgammonSettingsWidget.h"

#include <QDialog>

namespace Ui { class BackgammonSettingsDialog; }

// Standalone "Backgammon Settings" window (like RetroChess's), opened from
// the settings buttons of the Backgammon page and of the game window.
// Not modal; changes are saved immediately.
class BackgammonSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // Shows the one settings window (creating it when needed) on that page.
    static void showSettings(QWidget *parent,
                             BackgammonSettingsWidget::Page page = BackgammonSettingsWidget::General);

    ~BackgammonSettingsDialog() override;

private:
    explicit BackgammonSettingsDialog(QWidget *parent);

    Ui::BackgammonSettingsDialog *ui;
};
