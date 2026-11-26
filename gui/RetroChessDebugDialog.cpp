/*******************************************************************************
 * gui/RetroChessDebugDialog.cpp                                                *
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

#include "RetroChessDebugDialog.h"
#include "ui_RetroChessDebugDialog.h"

#include "interface/rsRetroChess.h"
#include "../backgammon/RetroChessBackgammon.h"
#include "util/misc.h"

#include <retroshare/rsidentity.h>

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QHeaderView>
#include <QPointer>
#include <QRegularExpression>
#include <QScrollBar>

#include <algorithm>
#include <string>

namespace {
constexpr int kMaxRows = 5000;
enum Column { ColTime, ColWhat, ColArea, ColPeer, ColTunnel, ColMessage, ColBytes, ColNote };
const int kProblemRole = Qt::UserRole + 1;
const int kAreaRole = Qt::UserRole + 2;

enum Kind { In, Out, Drop, Status, Note };

// One log line, split into the columns of the Traffic tab. The lines are
// the ones of retrochess_tunnels.log / retrochess_activity.log:
// "SEND tunnel=... to=... type=... bytes=... ok=1 ...".
struct ParsedLine
{
	Kind kind = Note;
	QString peer;      // full gxs id
	QString tunnel;    // full tunnel id
	QString message;
	int bytes = 0;
	QString note;
	bool problem = false;
};

ParsedLine parseLine(const QString &text)
{
	static const QRegularExpression token(QStringLiteral("\\b(tunnel|to|from|peer|host|type|bytes)=(\\S+)"));
	static const QRegularExpression problem(QStringLiteral(
	        "\\bok=0\\b|\\berror=[1-9]|timeout|timed out|DROPPED|dropped|ignored|failed|FAILED|giving up|"
	        "REMOTELY_CLOSED|TUNNEL_DN|DESYNC|REJECT|ORPHAN|unknown to GxsTunnel|did not come"));
	ParsedLine p;
	const QString verb = text.section(QLatin1Char(' '), 0, 0);
	QString from, to, other;
	QString rest = text;
	QRegularExpressionMatchIterator it = token.globalMatch(text);
	while (it.hasNext()) {
		const QRegularExpressionMatch m = it.next();
		const QString key = m.captured(1), value = m.captured(2);
		if (key == QLatin1String("tunnel") && p.tunnel.isEmpty()) p.tunnel = value;
		else if (key == QLatin1String("from")) from = value;
		else if (key == QLatin1String("to")) to = value;
		else if ((key == QLatin1String("peer") || key == QLatin1String("host")) && other.isEmpty()) other = value;
		else if (key == QLatin1String("type")) p.message = value;
		else if (key == QLatin1String("bytes")) p.bytes = value.toInt();
		else continue;
		rest.replace(m.captured(0), QString());
	}
	if (verb == QLatin1String("SEND")) p.kind = Out;
	else if (verb == QLatin1String("RECV")) p.kind = In;
	else if (verb == QLatin1String("STATUS")) p.kind = Status;
	p.problem = problem.match(text).hasMatch();
	if (p.problem && (text.contains(QLatin1String("DROPPED")) || text.contains(QLatin1String("ignored"))))
		p.kind = Drop;
	// The remote side: the receiver of what we send, the sender of what we get.
	p.peer = p.kind == Out ? (to.isEmpty() ? other : to)
	                       : (!from.isEmpty() ? from : (!other.isEmpty() ? other : to));
	if (p.message.isEmpty()) p.message = verb;
	if (p.kind != Note || rest.startsWith(verb + QLatin1Char(' '))) rest.remove(0, verb.size());
	p.note = rest.simplified();
	return p;
}

QString kindText(Kind kind)
{
	switch (kind) {
	case In: return QObject::tr("In");
	case Out: return QObject::tr("Out");
	case Drop: return QObject::tr("Dropped");
	case Status: return QObject::tr("Tunnel");
	case Note: return QObject::tr("Note");
	}
	return QString();
}

// Works with libretroshare before and after #435 (uint32_t or enum class).
template<typename S> QString tunnelStatusText(S status)
{
	switch (static_cast<uint32_t>(status)) {
	case 0: return QObject::tr("Unknown");
	case 1: return QObject::tr("Down – looking for a route");
	case 2: return QObject::tr("Can talk");
	case 3: return QObject::tr("Remotely closed");
	}
	return QObject::tr("Unknown (%1)").arg(static_cast<uint32_t>(status));
}
template<typename S> bool canTalk(S status) { return static_cast<uint32_t>(status) == 2; }

QString shortId(const QString &id) { return id.left(8); }
QString shortId(const RsGxsTunnelId &id) { return id.isNull() ? QString() : shortId(QString::fromStdString(id.toStdString())); }

QString secondsText(int64_t secs)
{
	if (secs < 0) return QObject::tr("never");
	if (secs < 120) return QObject::tr("%1 s").arg(secs);
	if (secs < 7200) return QObject::tr("%1 min").arg(secs / 60);
	if (secs < 2 * 86400) return QObject::tr("%1 h").arg(secs / 3600);
	return QObject::tr("%1 days").arg(secs / 86400);
}
}

void RetroChessDebugDialog::showDialog()
{
	static QPointer<RetroChessDebugDialog> dialog;
	if (!dialog) {
		dialog = new RetroChessDebugDialog;
		dialog->setAttribute(Qt::WA_DeleteOnClose);
	}
	dialog->show();
	dialog->raise();
	dialog->activateWindow();
}

RetroChessDebugDialog::RetroChessDebugDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::RetroChessDebugDialog)
{
	ui->setupUi(this);
	setWindowFlags(windowFlags() | Qt::Window | Qt::WindowMinMaxButtonsHint);
	ui->treeTraffic->header()->setStretchLastSection(true);
	ui->treeTunnels->header()->setStretchLastSection(true);
	ui->treeContacts->header()->setStretchLastSection(true);

	ui->comboArea->addItem(tr("All areas"), -1);
	for (int c = 0; c < RetroChessTunnelDebug::CategoryCount; ++c)
		ui->comboArea->addItem(QString::fromLatin1(RetroChessTunnelDebug::categoryName(c)), c);
	for (int c : {RetroChessTunnelDebug::BackgammonTunnel, RetroChessTunnelDebug::BackgammonGame})
		ui->comboArea->addItem(QString::fromLatin1(RetroChessTunnelDebug::categoryName(c)), c);

	pollEvents();
	for (int c = 0; c < ColNote; ++c) ui->treeTraffic->resizeColumnToContents(c);
	ui->treeTraffic->scrollToBottom();

	// The service writes from its own thread: poll the shared buffer.
	mPoll.setInterval(300);
	connect(&mPoll, &QTimer::timeout, this, [this] {
		if (!ui->checkPause->isChecked()) pollEvents();
	});
	mPoll.start();
	connect(ui->btnClear, &QPushButton::clicked, this, [this] {
		RetroChessTunnelDebug::clearEvents();
		ui->treeTraffic->clear();
		mIn = mOut = mProblems = 0;
		refreshSummary();
	});
	connect(ui->btnCopy, &QPushButton::clicked, this, &RetroChessDebugDialog::copyAll);
	connect(ui->lineFilter, &QLineEdit::textChanged, this, &RetroChessDebugDialog::applyFilterAll);
	connect(ui->checkProblems, &QCheckBox::toggled, this, &RetroChessDebugDialog::applyFilterAll);
	connect(ui->comboArea, QOverload<int>::of(&QComboBox::currentIndexChanged),
	        this, &RetroChessDebugDialog::applyFilterAll);

	mRefresh.setInterval(1000);
	connect(&mRefresh, &QTimer::timeout, this, [this] {
		if (ui->tabs->currentWidget() == ui->tabTunnels) refreshTunnels();
		else if (ui->tabs->currentWidget() == ui->tabContacts) refreshContacts();
	});
	connect(ui->tabs, &QTabWidget::currentChanged, this, [this] {
		if (ui->tabs->currentWidget() == ui->tabTunnels) refreshTunnels();
		else if (ui->tabs->currentWidget() == ui->tabContacts) refreshContacts();
	});
	mRefresh.start();
	refreshSummary();
}

RetroChessDebugDialog::~RetroChessDebugDialog()
{
	delete ui;
}

QString RetroChessDebugDialog::peerText(const QString &hexId)
{
	if (hexId.isEmpty()) return QString();
	auto cached = mNames.constFind(hexId);
	if (cached != mNames.constEnd()) return cached.value();
	const RsGxsId id(hexId.toStdString());
	RsIdentityDetails details;
	if (id.isNull() || !rsIdentity || !rsIdentity->getIdDetails(id, details) || details.mNickname.empty())
		return shortId(hexId);   // not known (yet): asked again next time
	const QString text = QStringLiteral("%1 (%2)").arg(QString::fromUtf8(details.mNickname.c_str()), shortId(hexId));
	mNames.insert(hexId, text);
	return text;
}

QString RetroChessDebugDialog::peerText(const RsGxsId &id)
{
	return id.isNull() ? QString() : peerText(QString::fromStdString(id.toStdString()));
}

void RetroChessDebugDialog::pollEvents()
{
	const std::vector<RetroChessTunnelDebug::Event> events = RetroChessTunnelDebug::eventsSince(mLastEventId);
	if (events.empty()) return;
	QScrollBar *bar = ui->treeTraffic->verticalScrollBar();
	const bool atEnd = bar->value() == bar->maximum();
	ui->treeTraffic->setUpdatesEnabled(false);
	for (const RetroChessTunnelDebug::Event &e : events) addEvent(e);
	mLastEventId = events.back().id;
	ui->treeTraffic->setUpdatesEnabled(true);
	if (atEnd) ui->treeTraffic->scrollToBottom();
	refreshSummary();
}

void RetroChessDebugDialog::addEvent(const RetroChessTunnelDebug::Event &e)
{
	const QString text = QString::fromStdString(e.text);
	const ParsedLine p = parseLine(text);
	auto *item = new QTreeWidgetItem;
	item->setText(ColTime, QDateTime::fromMSecsSinceEpoch(e.msecs).toString(QStringLiteral("HH:mm:ss.zzz")));
	item->setText(ColWhat, kindText(p.kind));
	item->setText(ColArea, QString::fromLatin1(RetroChessTunnelDebug::categoryName(e.category)));
	item->setText(ColPeer, peerText(p.peer));
	item->setToolTip(ColPeer, p.peer);
	item->setText(ColTunnel, shortId(p.tunnel));
	item->setToolTip(ColTunnel, p.tunnel);
	item->setText(ColMessage, p.message);
	item->setText(ColBytes, p.bytes > 0 ? QString::number(p.bytes) : QString());
	item->setText(ColNote, p.note);
	item->setToolTip(ColNote, text);   // the whole line, as in the log file
	item->setData(0, kProblemRole, p.problem);
	item->setData(0, kAreaRole, e.category);
	QColor color;
	if (p.problem) color = QColor(200, 40, 40);
	else if (p.kind == In) color = QColor(20, 120, 40);
	else if (p.kind == Out) color = QColor(30, 80, 170);
	else if (p.kind == Status) color = QColor(150, 100, 0);
	if (color.isValid()) for (int c = 0; c <= ColNote; ++c) item->setForeground(c, color);
	switch (p.kind) {
	case In: ++mIn; break;
	case Out: ++mOut; break;
	default: break;
	}
	if (p.problem) ++mProblems;
	ui->treeTraffic->addTopLevelItem(item);
	applyFilter(item);
	while (ui->treeTraffic->topLevelItemCount() > kMaxRows) delete ui->treeTraffic->takeTopLevelItem(0);
}

void RetroChessDebugDialog::applyFilter(QTreeWidgetItem *item) const
{
	const QString filter = ui->lineFilter->text().trimmed();
	const int area = ui->comboArea->currentData().toInt();
	bool show = !ui->checkProblems->isChecked() || item->data(0, kProblemRole).toBool();
	if (show && area >= 0) show = item->data(0, kAreaRole).toInt() == area;
	if (show && !filter.isEmpty()) {
		show = false;
		for (int c = 0; c <= ColNote && !show; ++c)
			if (item->text(c).contains(filter, Qt::CaseInsensitive)
			        || item->toolTip(c).contains(filter, Qt::CaseInsensitive)) show = true;
	}
	item->setHidden(!show);
}

void RetroChessDebugDialog::applyFilterAll()
{
	for (int i = 0; i < ui->treeTraffic->topLevelItemCount(); ++i) applyFilter(ui->treeTraffic->topLevelItem(i));
}

void RetroChessDebugDialog::refreshSummary()
{
	ui->labelSummary->setText(tr("%1 packets in, %2 out, %3 problems").arg(mIn).arg(mOut).arg(mProblems));
}

void RetroChessDebugDialog::refreshTunnels()
{
	std::vector<RsGxsTunnelService::GxsTunnelInfo> tunnels;
	std::vector<RsRetroChessDebugPeer> peers;
	std::map<RsGxsTunnelId, std::string> roles;
	const bool available = rsRetroChess && rsRetroChess->tunnelTraffic(tunnels);
	if (rsRetroChess) rsRetroChess->debugPeers(peers, roles);
	// Backgammon matches. RetroShare shares one tunnel per pair of
	// identities between services, so a tunnel may be listed already.
	for (const RetroChessBackgammon::DebugMatch &m : RetroChessBackgammon::debugMatches()) {
		const std::string role = "backgammon: " + m.what + " (" + std::to_string(m.packetsSent)
		        + " sent, " + std::to_string(m.packetsReceived) + " received)";
		bool listed = false;
		for (const auto &t : tunnels) listed = listed || t.tunnel_id == m.info.tunnel_id;
		if (!listed) tunnels.push_back(m.info);
		std::string &text = roles[m.info.tunnel_id];
		text = text.empty() ? role : text + ", " + role;
	}
	const int scroll = ui->treeTunnels->verticalScrollBar()->value();
	ui->treeTunnels->setUpdatesEnabled(false);
	ui->treeTunnels->clear();
	for (const RsGxsTunnelService::GxsTunnelInfo &t : tunnels) {
		auto *item = new QTreeWidgetItem(ui->treeTunnels);
		const auto role = roles.find(t.tunnel_id);
		QString roleText = role != roles.end() ? QString::fromStdString(role->second) : QString();
		// "active: <gxs id>" -> "active: name (id)"
		const int colon = roleText.indexOf(QStringLiteral(": "));
		if (colon > 0 && roleText.size() - colon - 2 == 32)
			roleText = roleText.left(colon + 2) + peerText(roleText.mid(colon + 2));
		item->setText(0, shortId(t.tunnel_id));
		item->setToolTip(0, QString::fromStdString(t.tunnel_id.toStdString()));
		item->setText(1, peerText(t.destination_gxs_id));
		item->setToolTip(1, QString::fromStdString(t.destination_gxs_id.toStdString()));
		item->setText(2, peerText(t.source_gxs_id));
		item->setToolTip(2, QString::fromStdString(t.source_gxs_id.toStdString()));
		item->setText(3, t.is_client_side ? tr("we opened it") : tr("opened by the peer"));
		item->setText(4, tunnelStatusText(t.tunnel_status));
		item->setText(5, roleText);
		item->setText(6, QString::number(t.pending_data_packets));
		item->setText(7, misc::friendlyUnit(static_cast<float>(t.total_size_sent)));
		item->setToolTip(7, tr("%1 bytes").arg(t.total_size_sent));
		item->setText(8, misc::friendlyUnit(static_cast<float>(t.total_size_received)));
		item->setToolTip(8, tr("%1 bytes").arg(t.total_size_received));
		if (!canTalk(t.tunnel_status) || t.pending_data_packets > 0 || roleText.startsWith(QLatin1String("ORPHAN")))
			for (int c = 0; c < 9; ++c) item->setForeground(c, QColor(200, 40, 40));
	}
	if (tunnels.empty()) {
		auto *item = new QTreeWidgetItem(ui->treeTunnels);
		item->setText(0, available ? tr("no GXS tunnels used by RetroChess or Backgammon") : tr("tunnel statistics are unavailable"));
	}
	for (int c = 0; c < 8; ++c) ui->treeTunnels->resizeColumnToContents(c);
	ui->treeTunnels->verticalScrollBar()->setValue(scroll);
	ui->treeTunnels->setUpdatesEnabled(true);
}

void RetroChessDebugDialog::refreshContacts()
{
	std::vector<RsRetroChessDebugPeer> peers;
	std::map<RsGxsTunnelId, std::string> roles;
	std::vector<RsGxsTunnelService::GxsTunnelInfo> tunnels;
	if (rsRetroChess) {
		rsRetroChess->debugPeers(peers, roles);
		rsRetroChess->tunnelTraffic(tunnels);
	}
	// Backgammon matches: their activity on the identity's line, and their
	// tunnels for the status column.
	for (const RetroChessBackgammon::DebugMatch &m : RetroChessBackgammon::debugMatches()) {
		const RsGxsId &id = m.info.destination_gxs_id;
		const std::string activity = "backgammon: " + m.what;
		auto peer = std::find_if(peers.begin(), peers.end(),
		                         [&id](const RsRetroChessDebugPeer &p) { return p.id == id; });
		if (peer == peers.end()) {
			RsRetroChessDebugPeer added;
			added.id = id;
			added.ownId = m.info.source_gxs_id;
			added.tunnel = m.info.tunnel_id;
			added.tunnelRole = "backgammon";
			added.activity = activity;
			peers.push_back(added);
		} else {
			peer->activity = peer->activity.empty() ? activity : peer->activity + ", " + activity;
		}
		bool listed = false;
		for (const auto &t : tunnels) listed = listed || t.tunnel_id == m.info.tunnel_id;
		if (!listed) tunnels.push_back(m.info);
	}
	std::map<RsGxsTunnelId, const RsGxsTunnelService::GxsTunnelInfo *> byId;
	for (const auto &t : tunnels) byId[t.tunnel_id] = &t;

	const int scroll = ui->treeContacts->verticalScrollBar()->value();
	ui->treeContacts->setUpdatesEnabled(false);
	ui->treeContacts->clear();
	for (const RsRetroChessDebugPeer &p : peers) {
		auto *item = new QTreeWidgetItem(ui->treeContacts);
		item->setText(0, peerText(p.id));
		item->setToolTip(0, QString::fromStdString(p.id.toStdString()));
		item->setText(1, peerText(p.ownId));
		if (p.contact) {
			QString presence = QString::fromStdString(p.presence);
			if (p.dormant) presence += tr(", dormant");
			if (p.yielding) presence += tr(", waiting for the peer to dial");
			item->setText(2, presence);
			item->setText(3, QString::number(p.failures));
			item->setText(4, p.lastSeenAgo < 0 ? tr("never") : tr("%1 ago").arg(secondsText(p.lastSeenAgo)));
			item->setText(5, p.nextProbeIn > 0 ? tr("in %1").arg(secondsText(p.nextProbeIn)) : tr("now / running"));
			item->setText(6, p.replyDueIn > 0 ? tr("in %1").arg(secondsText(p.replyDueIn)) : QString());
		} else {
			item->setText(2, tr("not a contact"));
		}
		if (!p.tunnel.isNull()) {
			item->setText(7, QStringLiteral("%1 (%2)").arg(shortId(p.tunnel), QString::fromStdString(p.tunnelRole)));
			item->setToolTip(7, QString::fromStdString(p.tunnel.toStdString()));
			const auto t = byId.find(p.tunnel);
			if (t == byId.end()) {
				item->setText(8, tr("gone"));
			} else {
				QString status = tunnelStatusText(t->second->tunnel_status);
				if (t->second->pending_data_packets > 0)
					status += tr(", %n not acknowledged", "", int(t->second->pending_data_packets));
				item->setText(8, status);
			}
		}
		item->setText(9, QString::fromStdString(p.activity));
		QColor color;
		if (p.dormant || p.failures > 0) color = QColor(200, 40, 40);
		else if (p.presence == "available" || p.presence == "playing" || p.presence == "busy") color = QColor(20, 120, 40);
		if (color.isValid()) for (int c = 0; c < 10; ++c) item->setForeground(c, color);
	}
	if (peers.empty()) {
		auto *item = new QTreeWidgetItem(ui->treeContacts);
		item->setText(0, tr("no chess contacts, games or invitations, no Backgammon matches"));
	}
	for (int c = 0; c < 9; ++c) ui->treeContacts->resizeColumnToContents(c);
	ui->treeContacts->verticalScrollBar()->setValue(scroll);
	ui->treeContacts->setUpdatesEnabled(true);
}

void RetroChessDebugDialog::copyAll()
{
	QStringList lines;
	for (int i = 0; i < ui->treeTraffic->topLevelItemCount(); ++i) {
		const QTreeWidgetItem *item = ui->treeTraffic->topLevelItem(i);
		if (item->isHidden()) continue;
		QStringList cols;
		for (int c = 0; c <= ColNote; ++c) cols << item->text(c);
		lines << cols.join(QLatin1Char('\t'));
	}
	QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}
