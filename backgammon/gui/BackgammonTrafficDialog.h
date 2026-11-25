/*******************************************************************************
 * gui/BackgammonTrafficDialog.h                                               *
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

namespace Ui { class BackgammonTrafficDialog; }

class QTimer;

// Settings → Backgammon → Debug → "View traffic…": the GXS tunnels of the
// Backgammon matches with their traffic, refreshed every 2 seconds while the
// dialog is shown. Works without logging.
class BackgammonTrafficDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BackgammonTrafficDialog(QWidget *parent = nullptr);
    ~BackgammonTrafficDialog() override;

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void refresh();
    void copyTable();

    Ui::BackgammonTrafficDialog *ui;
    QTimer *m_timer;
};
