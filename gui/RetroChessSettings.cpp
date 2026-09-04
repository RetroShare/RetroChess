/*******************************************************************************
 * gui/RetroChessSettings.cpp                                                  *
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

#include "RetroChessSettings.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "gui/settings/rsharesettings.h"

QVector<RetroChessBoardTheme> RetroChessSettings::boardThemes()
{
	return {
		{"original", QObject::tr("Original"), QColor(211, 211, 158), QColor(120, 120, 90), QColor("#d3d3d3")},
		{"green", QObject::tr("Green"), QColor("#eeeed2"), QColor("#769656"), QColor("#58a995")},
		{"brown", QObject::tr("Brown"), QColor("#f0d9b5"), QColor("#b58863"), QColor("#b3aa32")},
		{"blue", QObject::tr("Blue"), QColor("#dee3e6"), QColor("#8ca2ad"), QColor("#98b66f")},
		{"sky", QObject::tr("Sky"), QColor("#edf2f7"), QColor("#5b95e8"), QColor("#62b7b0")},
		{"checkers", QObject::tr("Checkers"), QColor("#d25058"), QColor("#292929"), QColor("#d3d3d3")}
	};
}

QString RetroChessSettings::boardThemeId()
{
	return Settings->valueFromGroup("RetroChess", "BoardTheme", "original").toString();
}

RetroChessBoardTheme RetroChessSettings::boardTheme()
{
	const QString selected = boardThemeId();
	for (const RetroChessBoardTheme &theme : boardThemes())
		if (theme.id == selected) return theme;
	return boardThemes().first();
}

void RetroChessSettings::setBoardThemeId(const QString &id)
{
	Settings->setValueToGroup("RetroChess", "BoardTheme", id);
	Settings->sync();
}

bool RetroChessSettings::moveSoundEnabled()
{
	return Settings->valueFromGroup("RetroChess", "SoundMove", true).toBool();
}

bool RetroChessSettings::captureSoundEnabled()
{
	return Settings->valueFromGroup("RetroChess", "SoundCapture", true).toBool();
}

bool RetroChessSettings::gameResultSoundEnabled()
{
	return Settings->valueFromGroup("RetroChess", "SoundGameResult", true).toBool();
}

bool RetroChessSettings::invitationSoundEnabled()
{
	return Settings->valueFromGroup("RetroChess", "SoundInvitation", true).toBool();
}

void RetroChessSettings::setSoundOptions(
        bool move, bool capture, bool gameResult, bool invitation)
{
	Settings->setValueToGroup("RetroChess", "SoundMove", move);
	Settings->setValueToGroup("RetroChess", "SoundCapture", capture);
	Settings->setValueToGroup("RetroChess", "SoundGameResult", gameResult);
	Settings->setValueToGroup("RetroChess", "SoundInvitation", invitation);
	Settings->sync();
}

RetroChessSettingsDialog::RetroChessSettingsDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(tr("RetroChess Settings"));
	setMinimumSize(650, 470);
	QVBoxLayout *root = new QVBoxLayout(this);
	QHBoxLayout *content = new QHBoxLayout;
	QListWidget *navigation = new QListWidget(this);
	navigation->setFixedWidth(145);
	navigation->addItem(tr("Board colours"));
	navigation->addItem(tr("Sounds"));
	QStackedWidget *pages = new QStackedWidget(this);
	content->addWidget(navigation);
	content->addWidget(pages, 1);
	root->addLayout(content, 1);

	QWidget *boardPage = new QWidget(pages);
	QVBoxLayout *boardRoot = new QVBoxLayout(boardPage);
	QLabel *title = new QLabel(tr("Board colours"), boardPage);
	QFont titleFont = title->font();
	titleFont.setPointSize(titleFont.pointSize() + 3);
	titleFont.setBold(true);
	title->setFont(titleFont);
	boardRoot->addWidget(title);
	boardRoot->addWidget(new QLabel(
	        tr("Choose the colour theme used for new and active chess boards."), boardPage));

	QGridLayout *themesLayout = new QGridLayout;
	QButtonGroup *group = new QButtonGroup(this);
	group->setExclusive(true);
	const QVector<RetroChessBoardTheme> themes = RetroChessSettings::boardThemes();
	for (int i = 0; i < themes.size(); ++i) {
		const RetroChessBoardTheme &theme = themes.at(i);
		QPixmap preview(92, 64);
		QPainter painter(&preview);
		for (int row = 0; row < 4; ++row)
			for (int col = 0; col < 4; ++col)
				painter.fillRect(col * 23, row * 16, 23, 16,
				                 ((row + col) % 2) ? theme.dark : theme.light);

		QToolButton *button = new QToolButton(boardPage);
		button->setText(theme.name);
		button->setIcon(QIcon(preview));
		button->setIconSize(preview.size());
		button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
		button->setCheckable(true);
		button->setMinimumSize(110, 94);
		button->setProperty("themeId", theme.id);
		button->setChecked(theme.id == RetroChessSettings::boardThemeId());
		group->addButton(button);
		themesLayout->addWidget(button, i / 3, i % 3);
	}
	boardRoot->addLayout(themesLayout);
	boardRoot->addStretch();
	pages->addWidget(boardPage);

	QWidget *soundsPage = new QWidget(pages);
	QVBoxLayout *soundsRoot = new QVBoxLayout(soundsPage);
	QLabel *soundsTitle = new QLabel(tr("Sounds"), soundsPage);
	soundsTitle->setFont(titleFont);
	soundsRoot->addWidget(soundsTitle);
	soundsRoot->addWidget(new QLabel(
	        tr("Choose which RetroChess events are allowed to play a sound."), soundsPage));
	QGridLayout *soundRows = new QGridLayout;
	soundRows->setColumnStretch(1, 1);
	auto addSound = [soundsPage, soundRows](
	        int row, const QString &name, const QString &purpose,
	        const QString &fileName, bool checked) {
		QCheckBox *enabled = new QCheckBox(name, soundsPage);
		enabled->setChecked(checked);
		QLabel *description = new QLabel(purpose, soundsPage);
		QLabel *file = new QLabel(fileName, soundsPage);
		file->setTextInteractionFlags(Qt::TextSelectableByMouse);
		soundRows->addWidget(enabled, row, 0);
		soundRows->addWidget(description, row, 1);
		soundRows->addWidget(file, row, 2);
		return enabled;
	};
	QCheckBox *moveSound = addSound(0, tr("Piece move"),
	        tr("After a normal chess move"), "Move.mp3",
	        RetroChessSettings::moveSoundEnabled());
	QCheckBox *captureSound = addSound(1, tr("Piece capture"),
	        tr("After a piece is captured"), "Capture.mp3",
	        RetroChessSettings::captureSoundEnabled());
	QCheckBox *resultSound = addSound(2, tr("Game result"),
	        tr("When a non-draw game finishes"), "victory.mp3",
	        RetroChessSettings::gameResultSoundEnabled());
	QCheckBox *inviteSound = addSound(3, tr("Chess invitation"),
	        tr("When an invitation toaster is received"), "ping.mp3",
	        RetroChessSettings::invitationSoundEnabled());
	soundsRoot->addLayout(soundRows);
	soundsRoot->addStretch();
	pages->addWidget(soundsPage);
	connect(navigation, &QListWidget::currentRowChanged,
	        pages, &QStackedWidget::setCurrentIndex);
	navigation->setCurrentRow(0);

	QDialogButtonBox *buttons = new QDialogButtonBox(
	        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
	root->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(buttons, &QDialogButtonBox::accepted, this,
	        [this, group, moveSound, captureSound, resultSound, inviteSound]() {
		if (group->checkedButton())
			RetroChessSettings::setBoardThemeId(
			        group->checkedButton()->property("themeId").toString());
		RetroChessSettings::setSoundOptions(
		        moveSound->isChecked(), captureSound->isChecked(),
		        resultSound->isChecked(), inviteSound->isChecked());
		accept();
	});
}
