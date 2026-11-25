/*******************************************************************************
 * gui/BackgammonResultDialog.h                                                *
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

namespace Ui { class BackgammonResultDialog; }

// Small popup over the game window when a match ends: won, lost, or ended
// for another reason (opponent left, connection lost, invitation declined).
// Its look is in gui/qss/Backgammon.qss (property "outcome").
class BackgammonResultDialog : public QDialog
{
    Q_OBJECT

public:
    enum Outcome { Neutral, Won, Lost };

    BackgammonResultDialog(Outcome outcome, const QString &title, const QString &message,
                           const QString &score, QWidget *parent);
    ~BackgammonResultDialog() override;

signals:
    void closeGameRequested();

private:
    Ui::BackgammonResultDialog *ui;
};
