/*******************************************************************************
 * gui/BackgammonSettingsDialog.cpp                                            *
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

#include "BackgammonSettingsDialog.h"
#include "ui_BackgammonSettingsDialog.h"

#include <QPointer>

BackgammonSettingsDialog::BackgammonSettingsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::BackgammonSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(false);
    ui->settingsWidget->load();
}

BackgammonSettingsDialog::~BackgammonSettingsDialog()
{
    delete ui;
}

void BackgammonSettingsDialog::showSettings(QWidget *parent, BackgammonSettingsWidget::Page page)
{
    static QPointer<BackgammonSettingsDialog> dialog;
    if (!dialog)
        dialog = new BackgammonSettingsDialog(parent ? parent->window() : nullptr);
    dialog->ui->settingsWidget->setCurrentPage(page);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}
