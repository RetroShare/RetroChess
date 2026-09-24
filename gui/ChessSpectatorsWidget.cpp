/*******************************************************************************
 * gui/ChessSpectatorsWidget.cpp                                               *
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

#include "ChessSpectatorsWidget.h"
#include "interface/rsRetroChess.h"
#include "gui/gxs/GxsIdTreeWidgetItem.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QLabel>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <set>

ChessSpectatorsWidget::ChessSpectatorsWidget(const RsGxsId &opponent, QWidget *parent)
    : QToolButton(parent), m_opponent(opponent), m_dialog(new QDialog(this)),
      m_empty(new QLabel(tr("Nobody is watching through you."), m_dialog)),
      m_watchers(new QTreeWidget(m_dialog))
{
    setAutoRaise(true);
    setFocusPolicy(Qt::NoFocus);
    setToolTip(tr("Show spectators connected through you. Viewers connected through your opponent are not included."));
    m_dialog->setWindowTitle(tr("Watching your game"));
    m_dialog->setModal(false);
    m_dialog->resize(420, 300);
    auto *layout = new QVBoxLayout(m_dialog);
    auto *description = new QLabel(tr("Spectators connected through you"), m_dialog);
    description->setWordWrap(true);
    layout->addWidget(description);
    layout->addWidget(m_empty);
    m_watchers->setHeaderLabel(tr("Watchers"));
    m_watchers->setRootIsDecorated(false);
    // Keep enough room for the identity renderer to generate a smooth avatar.
    // The source image is rendered at this size instead of being scaled from
    // the default tiny tree-item icon.
    m_watchers->setIconSize(QSize(32, 32));
    m_watchers->setUniformRowHeights(true);
    layout->addWidget(m_watchers);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, m_dialog);
    connect(buttons, &QDialogButtonBox::rejected, m_dialog, &QDialog::hide);
    layout->addWidget(buttons);
    connect(this, &QToolButton::clicked, this, [this]() {
        refresh();
        m_dialog->show();
        m_dialog->raise();
        m_dialog->activateWindow();
    });
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        if (isVisible() || m_dialog->isVisible()) refresh();
    });
    timer->start(2000);
    refresh();
}

void ChessSpectatorsWidget::refresh()
{
    const auto ids = rsRetroChess ? rsRetroChess->gameSpectators(m_opponent) : std::vector<RsGxsId>();
    setText(tr("Watching: %1").arg(static_cast<qulonglong>(ids.size())));
    m_empty->setVisible(ids.empty());
    std::set<RsGxsId> remaining(ids.begin(), ids.end());
    // Preserve items so asynchronous identity lookups can complete.
    for (int row = m_watchers->topLevelItemCount() - 1; row >= 0; --row) {
        auto *item = static_cast<GxsIdRSTreeWidgetItem *>(m_watchers->topLevelItem(row));
        RsGxsId id;
        item->getId(id);
        if (!remaining.erase(id)) delete m_watchers->takeTopLevelItem(row);
    }
    for (const auto &id : remaining) {
        auto *item = new GxsIdRSTreeWidgetItem(nullptr, GxsIdDetails::ICON_TYPE_AVATAR, true, m_watchers);
        QFont font = item->font(0);
        font.setPointSize(15);
        item->setFont(0, font);
        item->setSizeHint(0, QSize(0, 36));
        item->setId(id, 0, true);
    }
}
