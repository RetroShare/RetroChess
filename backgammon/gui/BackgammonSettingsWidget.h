/*******************************************************************************
 * gui/BackgammonSettingsWidget.h                                              *
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

#include <QWidget>

namespace Ui { class BackgammonSettingsWidget; }

class QListWidgetItem;

// All Backgammon settings, one page per topic (General, Board, Sounds,
// Debug) with a list to switch pages. Shown in the standalone settings
// dialog and in Preferences -> Backgammon. Changes are saved immediately.
class BackgammonSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    enum Page { General = 0, Board, Sounds, Debug };

    explicit BackgammonSettingsWidget(QWidget *parent = nullptr);
    ~BackgammonSettingsWidget() override;

    void load();   // shows the current settings
    void setCurrentPage(Page page);

private slots:
    void themeChanged(int index);
    void soundsToggled(bool enabled);
    void volumeChanged(int value);
    void eventChanged(QListWidgetItem *item);
    void eventActivated(QListWidgetItem *item);

private:
    void updatePreview();

    Ui::BackgammonSettingsWidget *ui;
};
