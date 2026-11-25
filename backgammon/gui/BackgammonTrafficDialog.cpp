/*******************************************************************************
 * gui/BackgammonTrafficDialog.cpp                                             *
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

#include "BackgammonTrafficDialog.h"
#include "ui_BackgammonTrafficDialog.h"

#include "services/BackgammonService.h"

#include "gui/gxs/GxsIdDetails.h"
#include "gui/gxs/GxsIdTreeWidgetItem.h"
#include "util/misc.h"

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QScrollBar>
#include <QSet>
#include <QTimer>

namespace {
enum Column {
    ColumnTunnel, ColumnLocal, ColumnRemote, ColumnStatus, ColumnUse, ColumnSent, ColumnReceived,
    ColumnPacketsSent, ColumnPacketsReceived, ColumnPending, ColumnSide, ColumnCount
};
constexpr int RefreshMs = 2000;
}

BackgammonTrafficDialog::BackgammonTrafficDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::BackgammonTrafficDialog),
      m_timer(new QTimer(this))
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    ui->trafficTree->header()->setStretchLastSection(false);
    for (int column = 0; column < ColumnCount; ++column)
        ui->trafficTree->setColumnWidth(column, column <= ColumnRemote ? 170 : 105);
    ui->trafficTree->setColumnWidth(ColumnUse, 160);

    connect(m_timer, &QTimer::timeout, this, &BackgammonTrafficDialog::refresh);
    connect(ui->copyButton, &QPushButton::clicked, this, &BackgammonTrafficDialog::copyTable);
    connect(ui->closeButton, &QPushButton::clicked, this, &QDialog::close);
}

BackgammonTrafficDialog::~BackgammonTrafficDialog()
{
    delete ui;
}

// Refreshed only while shown: a hidden viewer costs nothing.
void BackgammonTrafficDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    refresh();
    m_timer->start(RefreshMs);
}

void BackgammonTrafficDialog::hideEvent(QHideEvent *event)
{
    m_timer->stop();
    QDialog::hideEvent(event);
}

void BackgammonTrafficDialog::refresh()
{
    std::vector<BackgammonService::TunnelTraffic> tunnels;
    const bool available = BackgammonService::instance().tunnelTraffic(tunnels);

    QTreeWidget *tree = ui->trafficTree;
    QSet<QString> selected;
    for (const QTreeWidgetItem *item : tree->selectedItems())
        selected.insert(item->text(ColumnTunnel));
    const int scroll = tree->verticalScrollBar()->value();
    tree->setUpdatesEnabled(false);
    tree->clear();

    for (const auto &tunnel : tunnels) {
        const auto &info = tunnel.info;
        QString status;
        switch (info.tunnel_status) {
        case RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_UNKNOWN:         status = tr("Unknown"); break;
        case RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_TUNNEL_DN:       status = tr("Down / connecting"); break;
        case RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_CAN_TALK:        status = tr("Ready"); break;
        case RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_REMOTELY_CLOSED: status = tr("Remotely closed"); break;
        default: status = tr("Unknown (%1)").arg(info.tunnel_status); break;
        }

        auto *item = new QTreeWidgetItem(tree);
        item->setText(ColumnTunnel, QString::fromStdString(info.tunnel_id.toStdString()));
        item->setText(ColumnStatus, status);
        item->setText(ColumnUse, tunnel.match);
        item->setText(ColumnSent, misc::friendlyUnit(float(info.total_size_sent)));
        item->setText(ColumnReceived, misc::friendlyUnit(float(info.total_size_received)));
        item->setText(ColumnPacketsSent, QString::number(tunnel.packetsSent));
        item->setText(ColumnPacketsReceived, QString::number(tunnel.packetsReceived));
        item->setText(ColumnPending, QString::number(info.pending_data_packets));
        item->setText(ColumnSide, info.is_client_side ? tr("Initiator") : tr("Receiver"));

        // Identities: name and avatar when known, the id otherwise.
        for (const int column : { int(ColumnLocal), int(ColumnRemote) }) {
            const RsGxsId &id = column == ColumnLocal ? info.source_gxs_id : info.destination_gxs_id;
            const QString idText = QString::fromStdString(id.toStdString());
            QString name, comment;
            QIcon avatar;
            const bool known = GxsIdTreeItemDelegate::computeNameIconAndComment(id, name, avatar, comment);
            item->setText(column, known && !name.isEmpty() ? name : idText);
            if (avatar.isNull())
                avatar = QIcon(GxsIdDetails::makeDefaultIcon(id, GxsIdDetails::SMALL));
            item->setIcon(column, avatar);
            item->setData(column, Qt::UserRole, idText);
            item->setToolTip(column, idText);
        }
        for (int column = 0; column < ColumnCount; ++column) {
            if (item->toolTip(column).isEmpty())
                item->setToolTip(column, item->text(column));
            if (column >= ColumnSent && column <= ColumnPending)
                item->setTextAlignment(column, Qt::AlignRight | Qt::AlignVCenter);
        }
        item->setToolTip(ColumnSent, tr("%1 bytes").arg(info.total_size_sent));
        item->setToolTip(ColumnReceived, tr("%1 bytes").arg(info.total_size_received));
        item->setSelected(selected.contains(item->text(ColumnTunnel)));
    }

    tree->verticalScrollBar()->setValue(scroll);
    tree->setUpdatesEnabled(true);

    if (!available)
        ui->statusLabel->setText(tr("The tunnel service is not available."));
    else if (tunnels.empty())
        ui->statusLabel->setText(tr("No Backgammon tunnels right now. Invite someone or accept an invitation."));
    ui->statusLabel->setVisible(!available || tunnels.empty());
}

void BackgammonTrafficDialog::copyTable()
{
    QTreeWidget *tree = ui->trafficTree;
    QStringList lines { windowTitle() };
    QStringList cells;
    for (int column = 0; column < tree->columnCount(); ++column)
        cells << tree->headerItem()->text(column);
    lines << cells.join('\t');
    for (int row = 0; row < tree->topLevelItemCount(); ++row) {
        const QTreeWidgetItem *item = tree->topLevelItem(row);
        cells.clear();
        for (int column = 0; column < tree->columnCount(); ++column) {
            QString text = item->text(column);
            const QString id = item->data(column, Qt::UserRole).toString();
            if (!id.isEmpty() && id != text)
                text += QStringLiteral(" (%1)").arg(id);
            cells << text;
        }
        lines << cells.join('\t');
    }
    QApplication::clipboard()->setText(lines.join('\n'));
}
