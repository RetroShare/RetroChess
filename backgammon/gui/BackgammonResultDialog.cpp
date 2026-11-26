/*******************************************************************************
 * gui/BackgammonResultDialog.cpp                                              *
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

#include "BackgammonResultDialog.h"
#include "ui_BackgammonResultDialog.h"

#include "BackgammonStyle.h"

BackgammonResultDialog::BackgammonResultDialog(Outcome outcome, const QString &title, const QString &message,
                                               const QString &score, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::BackgammonResultDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);   // over the game window only
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    setProperty("outcome", outcome == Won ? "won" : outcome == Lost ? "lost" : "neutral");
    BackgammonStyle::apply(this);

    setWindowTitle(title);
    ui->titleLabel->setText(title);
    ui->messageLabel->setText(message);
    ui->messageLabel->setVisible(!message.isEmpty() && message != title);
    ui->scoreLabel->setText(score);
    ui->scoreLabel->setVisible(!score.isEmpty());

    connect(ui->closeGameButton, &QPushButton::clicked, this, [this] {
        emit closeGameRequested();
        close();
    });
}

BackgammonResultDialog::~BackgammonResultDialog()
{
    delete ui;
}
