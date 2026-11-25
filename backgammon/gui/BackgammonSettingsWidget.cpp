/*******************************************************************************
 * gui/BackgammonSettingsWidget.cpp                                            *
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

#include "BackgammonSettingsWidget.h"
#include "ui_BackgammonSettingsWidget.h"

#include "BackgammonSettings.h"
#include "BackgammonSounds.h"
#include "services/BackgammonService.h"
#include "BoardTheme.h"
#include "BackgammonConstants.h"
#include "BackgammonTrafficDialog.h"
#include "services/BackgammonDebug.h"

#include "gui/settings/rsharesettings.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QSignalBlocker>
#include <QUrl>

BackgammonSettingsWidget::BackgammonSettingsWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::BackgammonSettingsWidget)
{
    ui->setupUi(this);
    connect(ui->navigationList, &QListWidget::currentRowChanged, ui->pagesStack, &QStackedWidget::setCurrentIndex);
    ui->navigationList->setCurrentRow(General);

    for (const BoardTheme &theme : BoardTheme::all())
        ui->themeCombo->addItem(theme.name, theme.id);

    for (int i = 0; i < BackgammonSounds::EventCount; ++i) {
        auto *item = new QListWidgetItem(BackgammonSounds::label(BackgammonSounds::Event(i)),
                                         ui->eventsList);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        item->setData(Qt::UserRole, i);
        item->setCheckState(Qt::Checked);
    }

    connect(ui->themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BackgammonSettingsWidget::themeChanged);
    connect(ui->backgroundCheck, &QCheckBox::toggled, this, [this](bool on) {
        BoardTheme::setUseBackground(on);
        Settings->setValueToGroup(QStringLiteral("Backgammon"), QStringLiteral("ThemeBackground"), on);
        updatePreview();
    });
    connect(ui->openWindowCheck, &QCheckBox::toggled, this, [](bool on) { BackgammonSettings::setOpenInvitationWindow(on); });
    connect(ui->contactsOnlyCheck, &QCheckBox::toggled, this, [](bool on) {
        BackgammonService::instance().setContactsOnly(on);
        Settings->setValueToGroup(QStringLiteral("Backgammon"), QStringLiteral("ContactsOnly"), on);
    });
    connect(ui->toasterCheck, &QCheckBox::toggled, this, [](bool on) { BackgammonSettings::setInvitationToaster(on); });
    connect(ui->soundsGroup, &QGroupBox::toggled, this, &BackgammonSettingsWidget::soundsToggled);
    connect(ui->volumeSlider, &QSlider::valueChanged, this, &BackgammonSettingsWidget::volumeChanged);
    connect(ui->eventsList, &QListWidget::itemChanged, this, &BackgammonSettingsWidget::eventChanged);
    connect(ui->eventsList, &QListWidget::itemDoubleClicked, this, &BackgammonSettingsWidget::eventActivated);

    connect(ui->tunnelLogCheck, &QCheckBox::toggled, this, [](bool on) { BackgammonSettings::setTunnelLogging(on); });
    connect(ui->gameLogCheck, &QCheckBox::toggled, this, [](bool on) { BackgammonSettings::setGameLogging(on); });
    connect(ui->trafficButton, &QPushButton::clicked, this, [this] {
        // One viewer at a time; it closes itself (WA_DeleteOnClose).
        static QPointer<BackgammonTrafficDialog> dialog;
        if (!dialog)
            dialog = new BackgammonTrafficDialog(window());
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    });
    connect(ui->logFolderButton, &QPushButton::clicked, this, [] {
        const QString path = QString::fromStdString(BackgammonDebug::logFilePath(BackgammonDebug::Tunnel));
        if (!path.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    });
}

BackgammonSettingsWidget::~BackgammonSettingsWidget()
{
    delete ui;
}

void BackgammonSettingsWidget::load()
{
    const QSignalBlocker blockGroup(ui->soundsGroup);
    const QSignalBlocker blockSlider(ui->volumeSlider);
    const QSignalBlocker blockList(ui->eventsList);
    const QSignalBlocker blockTheme(ui->themeCombo);
    const QSignalBlocker blockBackground(ui->backgroundCheck);
    ui->backgroundCheck->setChecked(BoardTheme::useBackground());
    const QSignalBlocker blockWindow(ui->openWindowCheck);
    const QSignalBlocker blockToaster(ui->toasterCheck);
    const QSignalBlocker blockContacts(ui->contactsOnlyCheck);
    const QSignalBlocker blockTunnelLog(ui->tunnelLogCheck);
    const QSignalBlocker blockGameLog(ui->gameLogCheck);
    // Also on when switched on for this session with BACKGAMMON_DEBUG.
    ui->tunnelLogCheck->setChecked(BackgammonDebug::enabled(BackgammonDebug::Tunnel));
    ui->gameLogCheck->setChecked(BackgammonDebug::enabled(BackgammonDebug::Game));
    const QString logFile = QString::fromStdString(BackgammonDebug::logFilePath(BackgammonDebug::Tunnel));
    ui->logFolderLabel->setText(logFile.isEmpty()
        ? tr("Logs are written to the console only.")
        : tr("Log files are written to %1").arg(QDir::toNativeSeparators(QFileInfo(logFile).absolutePath())));
    ui->logFolderButton->setEnabled(!logFile.isEmpty());
    ui->contactsOnlyCheck->setChecked(BackgammonService::instance().contactsOnly());

    ui->openWindowCheck->setChecked(BackgammonSettings::openInvitationWindow());
    ui->toasterCheck->setChecked(BackgammonSettings::invitationToaster());

    ui->themeCombo->setCurrentIndex(qMax(0, ui->themeCombo->findData(BoardTheme::current().id)));
    updatePreview();

    ui->soundsGroup->setChecked(BackgammonSounds::soundsEnabled());
    ui->volumeSlider->setValue(BackgammonSounds::volume());
    ui->volumeValue->setText(tr("%1%").arg(BackgammonSounds::volume()));
    for (int row = 0; row < ui->eventsList->count(); ++row) {
        QListWidgetItem *item = ui->eventsList->item(row);
        const auto event = BackgammonSounds::Event(item->data(Qt::UserRole).toInt());
        item->setCheckState(BackgammonSounds::eventEnabled(event) ? Qt::Checked : Qt::Unchecked);
    }
}

void BackgammonSettingsWidget::setCurrentPage(Page page)
{
    ui->navigationList->setCurrentRow(page);
}

void BackgammonSettingsWidget::themeChanged(int index)
{
    const QString id = ui->themeCombo->itemData(index).toString();
    BoardTheme::setCurrent(id);
    Settings->setValueToGroup(QStringLiteral("Backgammon"), QStringLiteral("BoardTheme"), id);
    updatePreview();
}

void BackgammonSettingsWidget::updatePreview()
{
    const int index = qMax(0, ui->themeCombo->currentIndex());
    ui->themePreview->setPixmap(BoardTheme::all().at(index).preview(QSize(320, 150)));
}

void BackgammonSettingsWidget::soundsToggled(bool enabled)
{
    BackgammonSounds::setSoundsEnabled(enabled);
}

void BackgammonSettingsWidget::volumeChanged(int value)
{
    BackgammonSounds::setVolume(value);
    ui->volumeValue->setText(tr("%1%").arg(value));
}

void BackgammonSettingsWidget::eventChanged(QListWidgetItem *item)
{
    const auto event = BackgammonSounds::Event(item->data(Qt::UserRole).toInt());
    BackgammonSounds::setEventEnabled(event, item->checkState() == Qt::Checked);
}

void BackgammonSettingsWidget::eventActivated(QListWidgetItem *item)
{
    // Preview, also when that sound is switched off.
    BackgammonSounds::preview(BackgammonSounds::Event(item->data(Qt::UserRole).toInt()));
}
