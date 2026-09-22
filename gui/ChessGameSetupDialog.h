/*******************************************************************************
 * gui/ChessGameSetupDialog.h                                                  *
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

#include <QDialog>
#include "ChessTimeControl.h"

class QTabWidget;
class QSlider;
class QLabel;
class QPushButton;

/**
 * @brief Lichess-style "Game Setup" dialog shown before creating a lobby game.
 *
 * Presents two tabs:
 *  - Unlimited  — "Take all the time you need"
 *  - Real time  — minutes-per-side slider + increment slider + preset buttons
 *
 * After exec() == QDialog::Accepted, call selectedTimeControl() to obtain
 * the chosen ChessTimeControl.
 */
class ChessGameSetupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChessGameSetupDialog(QWidget *parent = nullptr);

    /// Returns the time control selected by the user.
    ChessTimeControl selectedTimeControl() const;

private slots:
    void onPresetClicked(int minutes, int increment);
    void onSlidersChanged();

private:
    void buildUnlimitedTab(QWidget *tab);
    void buildRealTimeTab(QWidget *tab);
    void updatePresetHighlight();
    void updateCategoryLabel();

    QTabWidget  *m_tabs          = nullptr;
    QSlider     *m_minutesSlider = nullptr;
    QSlider     *m_incrSlider    = nullptr;
    QLabel      *m_minutesBadge  = nullptr;
    QLabel      *m_incrBadge     = nullptr;
    QLabel      *m_categoryLabel = nullptr;

    // Preset buttons — kept to update their checked state
    struct Preset { int minutes; int increment; QPushButton *btn = nullptr; };
    QList<Preset> m_presets;
};
