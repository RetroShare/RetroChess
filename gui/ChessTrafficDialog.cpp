/*******************************************************************************
 * gui/ChessTrafficDialog.cpp                                                  *
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

#include "ChessTrafficDialog.h"
#include "interface/rsRetroChess.h"
#include "gui/gxs/GxsIdTreeWidgetItem.h"
#include "util/misc.h"

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

ChessTrafficDialog::ChessTrafficDialog(QWidget *parent)
    : QDialog(parent), m_status(new QLabel(this)), m_traffic(new QTreeWidget(this))
{
	setWindowTitle(tr("RetroChess — Tunnel traffic"));
	setAttribute(Qt::WA_DeleteOnClose);
	setModal(false);
	resize(1150, 480);
	auto *layout = new QVBoxLayout(this);
	auto *note = new QLabel(tr("Live GXS tunnels tracked by RetroChess. Byte totals include tunnel management "
	                          "and any other services sharing a tunnel. Sent/received packet counts are unavailable in this core. "
	                          "Closed tunnels disappear; counters start when a tunnel opens. Logging is not required."), this);
	note->setWordWrap(true);
	layout->addWidget(note);
	layout->addWidget(m_status);
	m_traffic->setColumnCount(10);
	m_traffic->setHeaderLabels({tr("Tunnel ID"), tr("Local identity"), tr("Remote identity"),
	        tr("Status"), tr("Sent"), tr("Received"), tr("Packets sent"),
	        tr("Packets received"), tr("Unacknowledged"), tr("Side")});
	m_traffic->setRootIsDecorated(false);
	m_traffic->setIconSize(QSize(24, 24));
	m_traffic->setAlternatingRowColors(true);
	m_traffic->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_traffic->header()->setStretchLastSection(false);
	for (int i = 0; i < 10; ++i) m_traffic->setColumnWidth(i, i < 3 ? 180 : 120);
	layout->addWidget(m_traffic, 1);
	auto *buttons = new QHBoxLayout;
	auto *copy = new QPushButton(tr("Copy table"), this);
	auto *close = new QPushButton(tr("Close"), this);
	buttons->addWidget(copy);
	buttons->addStretch();
	buttons->addWidget(close);
	layout->addLayout(buttons);
	auto *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [this]() {
		if (isVisible()) refreshTraffic();
	});
	connect(close, &QPushButton::clicked, this, &QDialog::close);
	connect(copy, &QPushButton::clicked, this, [this, note]() {
		QStringList lines, cells;
		lines << windowTitle() << note->text();
		if (!m_status->isHidden()) lines << m_status->text();
		for (int c = 0; c < m_traffic->columnCount(); ++c) cells << m_traffic->headerItem()->text(c);
		lines << cells.join('\t');
		for (int r = 0; r < m_traffic->topLevelItemCount(); ++r) {
			cells.clear();
			for (int c = 0; c < m_traffic->columnCount(); ++c) {
				const auto *item = m_traffic->topLevelItem(r);
				QString text = item->text(c);
				if (c == 1 || c == 2) {
					const QString id = item->data(c, Qt::UserRole).toString();
					if (text != id) text += " (" + id + ")";
				}
				cells << text;
			}
			lines << cells.join('\t');
		}
		QApplication::clipboard()->setText(lines.join('\n'));
	});
	refreshTraffic();
	timer->start(2000);
}

void ChessTrafficDialog::refreshTraffic()
{
	std::vector<RsGxsTunnelService::GxsTunnelInfo> infos;
	const bool available = rsRetroChess && rsRetroChess->tunnelTraffic(infos);
	QSet<QString> selected;
	for (auto *item : m_traffic->selectedItems()) selected.insert(item->text(0));
	const int scroll = m_traffic->verticalScrollBar()->value();
	m_traffic->setUpdatesEnabled(false);
	m_traffic->clear();
	for (const auto &info : infos) {
		QString status;
		switch (info.tunnel_status) {
		case RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_UNKNOWN: status = tr("Unknown"); break;
		case RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_TUNNEL_DN: status = tr("Down / connecting"); break;
		case RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_CAN_TALK: status = tr("Ready"); break;
		case RsGxsTunnelService::RS_GXS_TUNNEL_STATUS_REMOTELY_CLOSED: status = tr("Remotely closed"); break;
		default: status = tr("Unknown (%1)").arg(info.tunnel_status); break;
		}
		auto *item = new QTreeWidgetItem(m_traffic, {
		        QString::fromStdString(info.tunnel_id.toStdString()),
		        QString::fromStdString(info.source_gxs_id.toStdString()),
		        QString::fromStdString(info.destination_gxs_id.toStdString()), status,
		        misc::friendlyUnit(static_cast<float>(info.total_size_sent)),
		        misc::friendlyUnit(static_cast<float>(info.total_size_received)),
		        tr("N/A"), tr("N/A"),
		        QString::number(info.pending_data_packets), info.is_client_side ? tr("Initiator") : tr("Receiver")});
		for (int c = 0; c < 10; ++c) item->setToolTip(c, item->text(c));
		item->setData(1, Qt::UserRole, item->text(1));
		item->setData(2, Qt::UserRole, item->text(2));
		// Store the resolved display values in the model. The shared GXS
		// delegate's base painter reinitializes its option from model data,
		// losing a name supplied only during paint (and decoration flags).
		for (int column = 1; column <= 2; ++column) {
			const RsGxsId &id = column == 1 ? info.source_gxs_id : info.destination_gxs_id;
			QString name, comment;
			QIcon avatar;
			const bool resolved = GxsIdTreeItemDelegate::computeNameIconAndComment(id, name, avatar, comment);
			if (resolved && !name.isEmpty()) item->setText(column, name);
			if (avatar.isNull()) avatar = QIcon(GxsIdDetails::makeDefaultIcon(id, GxsIdDetails::SMALL));
			item->setIcon(column, avatar);
		}
		item->setToolTip(4, tr("%1 bytes").arg(info.total_size_sent));
		item->setToolTip(5, tr("%1 bytes").arg(info.total_size_received));
		for (int c = 4; c <= 8; ++c) item->setTextAlignment(c, Qt::AlignRight | Qt::AlignVCenter);
		item->setSelected(selected.contains(item->text(0)));
	}
	m_traffic->verticalScrollBar()->setValue(scroll);
	m_traffic->setUpdatesEnabled(true);
	if (!available) m_status->setText(tr("Tunnel statistics are unavailable."));
	else if (infos.empty()) m_status->setText(tr("No GXS tunnels currently tracked by RetroChess."));
	m_status->setVisible(!available || infos.empty());
}

