/*******************************************************************************
 * gui/RetroChessLeaderboard.cpp                                               *
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

#include "RetroChessLeaderboard.h"

#include <algorithm>
#include <cmath>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTableWidget>
#include <QHeaderView>
#include <QTimer>
#include <QBuffer>
#include <QPixmap>
#include <QSet>
#include <QLocale>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QUuid>
#include <QHash>
#include <functional>
#include <retroshare/rsinit.h>
#include <retroshare/rsidentity.h>
#include "interface/rsRetroChess.h"
#include "gui/settings/rsharesettings.h"
#include "gui/gxs/GxsIdTreeWidgetItem.h"
#include "gui/ChessGameHistory.h"
#include "gui/RetroChessSettings.h"
#include "gui/common/AvatarDefs.h"
#include "util/HandleRichText.h"
#include <retroshare/rspeers.h>
#include "services/RetroChessTunnelDebug.h"

// Short description of a receipt for the debug log.
#define LB_RECEIPT(r) "game=" << (r).gameId.toStdString() << " result=" << (r).result.toStdString() \
	<< " signer=" << (r).signer.toStdString()

namespace {
constexpr double kScale = 173.7178;
constexpr double kTau = 0.5;

// Receipts carry no cryptographic signature (the public RetroShare plugin API
// exposes no GXS signing call), so authenticity comes from the tunnel:
//  - a receipt received from the tunnel peer that is its signer is first-hand;
//  - a receipt relayed by anybody else is hearsay and only counts once
//    kMinWitnesses distinct peers reported it.
// The caps bound memory growth from peers that flood receipts.
// Leaderboard sync. Peers ask every 5 minutes for the receipts they have not
// seen yet ("since" = last sequence number received from that peer). The full
// history is only sent on first contact, after a restart, or to old versions
// that do not send "since". Full history at most every 5 minutes per peer for
// current versions (they switch to incremental right after the first one) and
// every 30 minutes for old versions (they never switch).
constexpr qint64 kSyncRequestIntervalMs = 5 * 60 * 1000;
constexpr qint64 kIncrementalSyncMinIntervalMs = 60 * 1000;
// Full history, current versions: 5 minutes. They send epoch+since, get a
// cursor from the first full answer and then only ask incrementally, so a
// repeat is only needed after our restart (new epoch) or a lost answer.
constexpr qint64 kFullSyncMinIntervalMs = 5 * 60 * 1000;
// Full history, old versions: 30 minutes. They send no "since" and can never
// sync incrementally, so every request would resend the whole history -
// the 30 minute limit keeps that traffic down.
constexpr qint64 kLegacyFullSyncMinIntervalMs = 30 * 60 * 1000;
constexpr int kSyncBatchSize = 10;
constexpr int kMinWitnesses = 2;
constexpr int kMaxWitnesses = 8;
constexpr int kMaxReceipts = 50000;
constexpr int kMaxPending = 2000;

QString leaderboardFilePath()
{
	const std::string accDir = RsAccounts::AccountDirectory();
	if (accDir.empty()) {
		return QString();
	}
	return QString::fromUtf8(accDir.c_str()) + "/retrochess_leaderboard.json";
}

struct HistoryName { QString name; QString peerId; };

// Table item whose tooltip is only built when it is first shown. The player
// tooltip decodes the LARGE avatar, rescales it and embeds it as base64 PNG;
// doing that for every row on every leaderboard refresh was most of the cost
// of populate().
class LazyTooltipItem : public QTableWidgetItem
{
public:
	LazyTooltipItem(const QString &text, std::function<QString()> makeTooltip)
	    : QTableWidgetItem(text), mMakeTooltip(std::move(makeTooltip)) {}
	QVariant data(int role) const override
	{
		if (role == Qt::ToolTipRole && mMakeTooltip) {
			mTooltip = mMakeTooltip();
			mMakeTooltip = nullptr;
		}
		if (role == Qt::ToolTipRole && !mTooltip.isNull()) return mTooltip;
		return QTableWidgetItem::data(role);
	}
private:
	mutable std::function<QString()> mMakeTooltip;
	mutable QString mTooltip;
};

// Identity -> name/peer seen in the game history (newest game first). Rebuilt
// only when the history changes, instead of scanning every game for every
// name lookup (twice per receipt in recompute(), once per table row).
const QHash<QString, HistoryName> &historyNames()
{
	static QHash<QString, HistoryName> names;
	static quint64 builtFor = ~quint64(0);
	const QVector<ChessGameRecord> games = ChessGameHistory::games(); // refreshes revision()
	if (builtFor == ChessGameHistory::revision()) return names;
	names.clear();
	auto add = [](const QString &id, const QString &name, const QString &peerId) {
		if (id.isEmpty()) return;
		HistoryName &entry = names[id.toLower()];
		if (entry.name.isEmpty()) entry.name = name;
		if (entry.peerId.isEmpty()) entry.peerId = peerId;
	};
	for (const ChessGameRecord &game : games) {
		add(game.whiteGxsId, game.whitePlayer, game.whitePeerId);
		add(game.blackGxsId, game.blackPlayer, game.blackPeerId);
	}
	builtFor = ChessGameHistory::revision();
	return names;
}

QString lookupGameHistoryName(const QString &idStr, QString *outPeerId = nullptr)
{
	if (idStr.isEmpty()) return QString();
	const auto &names = historyNames();
	const auto it = names.constFind(idStr.toLower());
	if (it == names.constEnd()) return QString();
	if (outPeerId && !it->peerId.isEmpty()) *outPeerId = it->peerId;
	return it->name;
}

QString displayName(const RsGxsId &id)
{
	RsIdentityDetails details;
	if (rsIdentity && rsIdentity->getIdDetails(id, details) && !details.mNickname.empty())
		return QString::fromUtf8(details.mNickname.c_str());

	const QString idStr = QString::fromStdString(id.toStdString());
	QString peerId;
	const QString histName = lookupGameHistoryName(idStr, &peerId);
	if (!histName.isEmpty())
		return histName;

	if (!peerId.isEmpty() && rsPeers) {
		const std::string peerName = rsPeers->getPeerName(RsPeerId(peerId.toStdString()));
		if (!peerName.empty())
			return QString::fromStdString(peerName);
	}

	return idStr.left(12);
}

double newVolatility(double phi, double delta, double variance, double sigma)
{
	const double a = std::log(sigma * sigma);
	auto f = [=](double x) {
		const double ex = std::exp(x);
		return ex * (delta * delta - phi * phi - variance - ex)
		       / (2.0 * std::pow(phi * phi + variance + ex, 2.0)) - (x - a) / (kTau * kTau);
	};
	double A = a, B;
	if (delta * delta > phi * phi + variance)
		B = std::log(delta * delta - phi * phi - variance);
	else {
		int k = 1;
		while (f(a - k * kTau) < 0.0) ++k;
		B = a - k * kTau;
	}
	double fA = f(A), fB = f(B);
	while (std::abs(B - A) > 0.000001) {
		const double C = A + (A - B) * fA / (fB - fA);
		const double fC = f(C);
		if (fC * fB <= 0.0) { A = B; fA = fB; }
		else fA /= 2.0;
		B = C; fB = fC;
	}
	return std::exp(A / 2.0);
}

void updatePair(RetroChessLeaderboard::Player &a,
	             RetroChessLeaderboard::Player &b, double scoreA)
{
	auto next = [](const RetroChessLeaderboard::Player &p,
	               const RetroChessLeaderboard::Player &op, double score) {
		RetroChessLeaderboard::Player n = p;
		const double mu = (p.rating - 1500.0) / kScale;
		const double phi = p.rd / kScale;
		const double muJ = (op.rating - 1500.0) / kScale;
		const double phiJ = op.rd / kScale;
		const double g = 1.0 / std::sqrt(1.0 + 3.0 * phiJ * phiJ / (3.14159265358979323846 * 3.14159265358979323846));
		const double e = 1.0 / (1.0 + std::exp(-g * (mu - muJ)));
		const double variance = 1.0 / (g * g * e * (1.0 - e));
		const double delta = variance * g * (score - e);
		n.volatility = newVolatility(phi, delta, variance, p.volatility);
		const double phiStar = std::sqrt(phi * phi + n.volatility * n.volatility);
		const double phiPrime = 1.0 / std::sqrt(1.0 / (phiStar * phiStar) + 1.0 / variance);
		const double muPrime = mu + phiPrime * phiPrime * g * (score - e);
		n.rating = 1500.0 + kScale * muPrime;
		n.rd = kScale * phiPrime;
		return n;
	};
	const RetroChessLeaderboard::Player oldA = a, oldB = b;
	a = next(oldA, oldB, scoreA);
	b = next(oldB, oldA, 1.0 - scoreA);
}
}

RetroChessLeaderboard::RetroChessLeaderboard(QObject *parent)
    : QObject(parent), mSyncTimer(new QTimer(this)), mCommitTimer(new QTimer(this))
{
	mCommitTimer->setSingleShot(true);
	mCommitTimer->setInterval(500);
	connect(mCommitTimer, &QTimer::timeout, this, &RetroChessLeaderboard::commitChanges);
	mSyncEpoch = QUuid::createUuid().toString(QUuid::WithoutBraces);
	load();
	connect(mSyncTimer, &QTimer::timeout, this, &RetroChessLeaderboard::synchronizeTunnels);
	mSyncClock.start();
	mSyncTimer->setInterval(kSyncRequestIntervalMs);
	mSyncTimer->start();
	QTimer::singleShot(0, this, &RetroChessLeaderboard::synchronizeTunnels);
}

RetroChessLeaderboard::~RetroChessLeaderboard()
{
	// Do not lose receipts that arrived during the last commit delay.
	if (mSaveNeeded) save();
}

void RetroChessLeaderboard::scheduleCommit(bool ratingsChanged)
{
	mSaveNeeded = true;
	if (ratingsChanged) mRecomputeNeeded = true;
	if (!mCommitTimer->isActive()) mCommitTimer->start();
}

void RetroChessLeaderboard::commitChanges()
{
	mCommitTimer->stop();
	const bool saveNeeded = mSaveNeeded;
	const bool recomputeNeeded = mRecomputeNeeded;
	mSaveNeeded = mRecomputeNeeded = false;
	if (saveNeeded) save();
	if (recomputeNeeded) {
		recompute();
		emit changed();
	}
}

bool RetroChessLeaderboard::validResult(const QString &r)
{ return r == "1-0" || r == "0-1" || r == "1/2-1/2"; }

QString RetroChessLeaderboard::canonicalKey(const Receipt &r)
{ return r.gameId + '|' + r.white + '|' + r.black + '|' + r.result; }

void RetroChessLeaderboard::submitResult(const QString &gameId, const RsGxsId &white,
	                                      const RsGxsId &black, const QString &result)
{
	if (!rsIdentity || gameId.isEmpty() || white.isNull() || black.isNull() || white == black
        || (!rsIdentity->isOwnId(white) && !rsIdentity->isOwnId(black)) || !validResult(result)) return;
	Receipt r{gameId, QString::fromStdString(white.toStdString()),
	          QString::fromStdString(black.toStdString()), result,
	          QString::fromStdString((white == black ? RsGxsId() :
	              (rsIdentity->isOwnId(white) ? white : black)).toStdString()),
	          QDateTime::currentSecsSinceEpoch()};
	if (r.signer.isEmpty()) return;
	CHESS_LBLOG("OWN result " << LB_RECEIPT(r) << " white=" << white << " black=" << black
	            << " -> storing and sending to peers");
	// Our own receipt: the signer is a local identity, so it is first-hand.
	consumeReceipt(r, RsGxsId(r.signer.toStdString()));
	broadcastReceipt(r);
	// Our own game: show the new rating right away.
	commitChanges();
}

void RetroChessLeaderboard::receiveResult(const RsGxsId &signer, const QString &gameId,
	                                       const RsGxsId &white, const RsGxsId &black,
	                                       const QString &result, qint64 finishedAt)
{
	// The caller must already have authenticated `signer` (for example from
	// the tunnel the result arrived on).
	consumeReceipt(Receipt{gameId, QString::fromStdString(white.toStdString()),
	        QString::fromStdString(black.toStdString()), result,
	        QString::fromStdString(signer.toStdString()), finishedAt}, signer);
}

bool RetroChessLeaderboard::consumeReceipt(const Receipt &r, const RsGxsId &sender)
{
	if (r.gameId.isEmpty() || r.gameId.size() > 128 || r.white == r.black
        || RsGxsId(r.white.toStdString()).isNull() || RsGxsId(r.black.toStdString()).isNull()
        || r.finishedAt <= 0 || !validResult(r.result)
	    || (r.signer != r.white && r.signer != r.black)) {
		CHESS_LBLOG("REJECT invalid receipt from=" << sender << " " << LB_RECEIPT(r)
		            << " white=" << r.white.toStdString() << " black=" << r.black.toStdString());
		return false;
	}
	const QString key = canonicalKey(r) + '|' + r.signer;
	// Duplicate posts cannot count twice. Pick a stable timestamp regardless of
    // arrival order; retain conflicting claims so every node excludes them.
    if (mReceipts.contains(key) && mReceipts.value(key).finishedAt <= r.finishedAt) return false;

	// Only the signer itself can vouch for its own receipt. Anything relayed
	// by another peer is hearsay: without this, any peer could post receipts
	// naming other identities as signer and move their ratings.
	const bool firstHand = !sender.isNull() && RsGxsId(r.signer.toStdString()) == sender;
	if (!firstHand) {
		if (sender.isNull()) return false;
		if (!mPending.contains(key)) {
			if (mPending.size() >= kMaxPending) {
				CHESS_LBLOG("REJECT relayed receipt from=" << sender << " " << LB_RECEIPT(r)
				            << ": pending list full (" << kMaxPending << ")");
				return false;
			}
			mPending.insert(key, r);
		}
		QSet<QString> &witnesses = mWitnesses[key];
		const int before = witnesses.size();
		if (witnesses.size() < kMaxWitnesses)
			witnesses.insert(QString::fromStdString(sender.toStdString()));
		if (witnesses.size() < kMinWitnesses) {
			if (witnesses.size() != before)
				CHESS_LBLOG("WAIT relayed receipt from=" << sender << " " << LB_RECEIPT(r)
				            << ": witnesses " << witnesses.size() << "/" << kMinWitnesses);
			return false;
		}
		// Corroborated by enough distinct peers: accept the first version seen.
		const Receipt accepted = mPending.value(key);
		mPending.remove(key);
		mWitnesses.remove(key);
		if (mReceipts.size() >= kMaxReceipts && !mReceipts.contains(key)) {
			CHESS_LBLOG("REJECT receipt " << LB_RECEIPT(accepted) << ": leaderboard full (" << kMaxReceipts << ")");
			return false;
		}
		storeReceipt(key, accepted, sender);
		CHESS_LBLOG("ACCEPT relayed receipt " << LB_RECEIPT(accepted) << " confirmed by "
		            << kMinWitnesses << " witnesses (last=" << sender << ") seq=" << mNextSeq);
	} else {
		mPending.remove(key);
		mWitnesses.remove(key);
		if (mReceipts.size() >= kMaxReceipts && !mReceipts.contains(key)) {
			CHESS_LBLOG("REJECT receipt " << LB_RECEIPT(r) << ": leaderboard full (" << kMaxReceipts << ")");
			return false;
		}
		storeReceipt(key, r, sender);
		CHESS_LBLOG("ACCEPT first-hand receipt from=" << sender << " " << LB_RECEIPT(r) << " seq=" << mNextSeq);
	}
	scheduleCommit();
	return true;
}

void RetroChessLeaderboard::storeReceipt(const QString &key, Receipt receipt, const RsGxsId &from)
{
	receipt.learnedFrom = QString::fromStdString(from.toStdString());
	// A replaced receipt (earlier timestamp for the same key) gets a new number
	// too, so that peers pick up the correction in their next incremental sync.
	receipt.seq = ++mNextSeq;
	mReceipts.insert(key, receipt);
}

void RetroChessLeaderboard::recompute()
{
	mPlayers.clear();
	QMap<QString, QList<Receipt>> byGame;
	for (const Receipt &r : mReceipts) byGame[r.gameId].append(r);
	QList<Receipt> confirmed;
	for (const QList<Receipt> &list : byGame) {
		if (list.size() != 2) continue;
		for (const Receipt &a : list) for (const Receipt &b : list)
			if (a.signer == a.white && b.signer == a.black && canonicalKey(a) == canonicalKey(b)) {
				confirmed.append(a); goto nextGame;
			}
		nextGame:;
	}
	std::sort(confirmed.begin(), confirmed.end(), [](const Receipt &a, const Receipt &b) {
		return a.finishedAt == b.finishedAt ? a.gameId < b.gameId : a.finishedAt < b.finishedAt;
	});
	for (const Receipt &r : confirmed) {
		Player &w = mPlayers[r.white], &b = mPlayers[r.black];
		w.id = RsGxsId(r.white.toStdString()); b.id = RsGxsId(r.black.toStdString());
		const double score = r.result == "1-0" ? 1.0 : (r.result == "0-1" ? 0.0 : 0.5);
		updatePair(w, b, score);
		if (score == 1.0) { ++w.wins; ++b.losses; }
		else if (score == 0.0) { ++w.losses; ++b.wins; }
		else { ++w.draws; ++b.draws; }
		w.lastPlayed = b.lastPlayed = QDateTime::fromSecsSinceEpoch(r.finishedAt);
	}
	// Resolve each name once per player, not twice per receipt.
	for (Player &player : mPlayers) player.name = displayName(player.id);
}

bool RetroChessLeaderboard::getPlayer(const RsGxsId &id, Player &player) const
{
	const QString key = QString::fromStdString(id.toStdString());
	auto it = mPlayers.find(key);
	if (it != mPlayers.end()) {
		player = it.value();
		return true;
	}
	return false;
}

QString RetroChessLeaderboard::playerTooltipHtml(
        const QString &name, const QString &endpointId, const QPixmap &avatar,
        const Player &player)
{
	const auto tr = [](const char *text) { return RetroChessLeaderboard::tr(text).toHtmlEscaped(); };
	QByteArray imageBytes;
	QBuffer buffer(&imageBytes);
	buffer.open(QIODevice::WriteOnly);
	const bool saved = avatar.scaled(70, 70, Qt::KeepAspectRatio,
	        Qt::SmoothTransformation).save(&buffer, "PNG");
	const QString embeddedAvatar = saved
	        ? QStringLiteral("<img src=\"data:image/png;base64,%1\">")
	                .arg(QString::fromLatin1(imageBytes.toBase64())) : QString();
	return QStringLiteral(
	        "<table cellspacing='4'><tr><td rowspan='4' valign='top'>%1</td>"
	        "<td colspan='2'><span style='font-size:large; font-weight:600;'>%2</span></td></tr>"
	        "<tr><td>%3</td><td><b>%4</b> &nbsp; %5</td></tr>"
	        "<tr><td>%6</td><td>%7</td></tr>"
	        "<tr><td>%8</td><td>%9</td></tr>"
	        "<tr><td colspan='3'><hr/></td></tr>"
	        "<tr><td colspan='3'><small>%10 %11</small></td></tr></table>")
	        .arg(embeddedAvatar, name.toHtmlEscaped())
	        .arg(tr("Rating")).arg(qRound(player.rating))
	        .arg(player.provisional() ? tr("Provisional") : tr("Rated"))
	        .arg(tr("RD")).arg(qRound(player.rd))
	        .arg(tr("Games")).arg(player.games())
	        .arg(tr("ID:"), endpointId.toHtmlEscaped());
}

void RetroChessLeaderboard::populate(QTableWidget *table) const
{
	QList<Player> players = mPlayers.values();
	std::sort(players.begin(), players.end(), [](const Player &a, const Player &b) { return a.rating == b.rating ? a.id < b.id : a.rating > b.rating; });
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	table->setSelectionMode(QAbstractItemView::SingleSelection);
	table->setSortingEnabled(false); table->setRowCount(players.size()); table->setColumnCount(10);
	table->setIconSize(QSize(32, 32));
	table->verticalHeader()->setDefaultSectionSize(36);
	table->setHorizontalHeaderLabels({tr("#"), tr("Player"), tr("Rating"), tr("RD"), tr("Games"),
	                                  tr("W"), tr("D"), tr("L"), tr("Status"), tr("Last played")});
	if (table->horizontalHeaderItem(0)) {
		table->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignCenter);
	}
	for (int c = 2; c <= 7; ++c) {
		if (table->horizontalHeaderItem(c)) {
			table->horizontalHeaderItem(c)->setTextAlignment(Qt::AlignCenter);
		}
	}
	static QMap<QString, qint64> sRequestedIdentities;
	const qint64 now = QDateTime::currentSecsSinceEpoch();

	for (int row = 0; row < players.size(); ++row) {
		const Player &p = players.at(row);
		const QString idStr = QString::fromStdString(p.id.toStdString());
		RsIdentityDetails details;
		const bool known = rsIdentity && rsIdentity->getIdDetails(p.id, details);

		QString peerId;
		QString playerName = known && !details.mNickname.empty()
		        ? QString::fromUtf8(details.mNickname.c_str())
		        : QString();
		if (playerName.isEmpty()) {
			playerName = lookupGameHistoryName(idStr, &peerId);
		}
		if (playerName.isEmpty() && !peerId.isEmpty() && rsPeers) {
			const std::string peerName = rsPeers->getPeerName(RsPeerId(peerId.toStdString()));
			if (!peerName.empty()) {
				playerName = QString::fromStdString(peerName);
			}
		}
		if (playerName.isEmpty()) {
			playerName = p.name.isEmpty() ? idStr.left(12) : p.name;
		}

		// Resolve table avatar (MEDIUM)
		QPixmap avatar;
		if (known && details.mAvatar.mSize > 0) {
			GxsIdDetails::loadPixmapFromData(
			        details.mAvatar.mData, details.mAvatar.mSize, avatar, GxsIdDetails::MEDIUM);
		}
		if (avatar.isNull()) {
			avatar = GxsIdDetails::makeDefaultIcon(p.id, GxsIdDetails::MEDIUM);
		}

		// The tooltip avatar (LARGE) is only decoded when the tooltip is shown.
		const QByteArray avatarBytes = known && details.mAvatar.mSize > 0
		        ? QByteArray(reinterpret_cast<const char *>(details.mAvatar.mData),
		                     static_cast<int>(details.mAvatar.mSize))
		        : QByteArray();

		// Actively request unknown identity details from peers (throttled to once every 30 seconds per ID)
		if (rsIdentity && !p.id.isNull() && !rsIdentity->isOwnId(p.id)) {
			if (!known || details.mNickname.empty()) {
				if (!sRequestedIdentities.contains(idStr) || (now - sRequestedIdentities.value(idStr) > 30)) {
					sRequestedIdentities[idStr] = now;
					rsIdentity->requestIdentity(p.id);
				}
			}
		}

		QString playerTooltip = known ? GxsIdDetails::getComment(details) : QString();
		if (playerTooltip.isEmpty())
			playerTooltip = tr("Identity name: %1<br/>Identity Id: %2")
			        .arg(playerName.toHtmlEscaped(), idStr.toHtmlEscaped());
		const RsGxsId playerId = p.id;
		auto makePlayerTooltip = [avatarBytes, playerId, playerTooltip]() {
			QPixmap tooltipPixmap;
			if (!avatarBytes.isEmpty())
				GxsIdDetails::loadPixmapFromData(
				        reinterpret_cast<const unsigned char *>(avatarBytes.constData()),
				        avatarBytes.size(), tooltipPixmap, GxsIdDetails::LARGE);
			if (tooltipPixmap.isNull())
				tooltipPixmap = GxsIdDetails::makeDefaultIcon(playerId, GxsIdDetails::LARGE);
			QString embeddedImage;
			if (RsHtml::makeEmbeddedImage(
			        tooltipPixmap.scaled(QSize(96, 96), Qt::KeepAspectRatio,
			                             Qt::SmoothTransformation).toImage(),
			        embeddedImage, -1))
				return QString("<table><tr><td>%1</td><td>%2</td></tr></table>")
				        .arg(embeddedImage, playerTooltip);
			return playerTooltip;
		};

		const QStringList values{QString::number(row + 1), playerName, QString::number(qRound(p.rating)),
		                         QString::number(qRound(p.rd)), QString::number(p.games()),
		                         QString::number(p.wins), QString::number(p.draws), QString::number(p.losses),
		                         p.provisional() ? tr("Provisional") : tr("Rated"),
		                         RetroChessSettings::formatDateTime(p.lastPlayed.toLocalTime())};
		for (int col = 0; col < values.size(); ++col) {
            auto *item = col == 1 ? new LazyTooltipItem(values.at(col), makePlayerTooltip)
                                  : new QTableWidgetItem(values.at(col));
            if (col == 0 || (col >= 2 && col <= 7)) {
                item->setData(Qt::DisplayRole, values.at(col).toInt());
                item->setTextAlignment(Qt::AlignCenter);
            }
            if (col == 1) {
                item->setIcon(QIcon(avatar));
            } else if (col == 2) {
                item->setToolTip(tr("Rating: %1 (%2, %3 games)")
                        .arg(qRound(p.rating))
                        .arg(p.provisional() ? tr("Provisional") : tr("Rated"))
                        .arg(p.games()));
            } else if (col == 3) {
                item->setToolTip(tr("Rating Deviation: %1 (lower means more reliable)")
                        .arg(qRound(p.rd)));
            } else {
                item->setToolTip(QString::fromStdString(p.id.toStdString()));
            }
            table->setItem(row, col, item);
        }
	}
	table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
	table->resizeColumnsToContents();
	table->setColumnWidth(0, std::max(45, table->columnWidth(0)));
	table->setColumnWidth(1, std::max(200, table->columnWidth(1)));
	for (int c = 2; c <= 7; ++c) {
		table->setColumnWidth(c, std::max(50, table->columnWidth(c)));
	}
	table->horizontalHeader()->setStretchLastSection(true);
	table->setSortingEnabled(true);
}

void RetroChessLeaderboard::load()
{
	mReceipts.clear();
	mGossipedReceipts.clear();

	const QString filePath = leaderboardFilePath();
	bool loadedFromFile = false;

	if (!filePath.isEmpty() && QFile::exists(filePath)) {
		QFile file(filePath);
		if (!file.open(QIODevice::ReadOnly))
			CHESS_STORELOG("LOAD leaderboard file " << filePath.toStdString() << " cannot be opened: "
			               << file.errorString().toStdString());
		if (file.isOpen()) {
			QJsonParseError parseError;
			const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
			if (!doc.isObject())
				CHESS_STORELOG("LOAD leaderboard file " << filePath.toStdString() << " is not valid JSON: "
				               << parseError.errorString().toStdString());
			if (doc.isObject()) {
				const QJsonObject root = doc.object();
				const QJsonArray array = root.value("receipts").toArray();
				for (const QJsonValue &v : array) {
					const QJsonObject o = v.toObject();
					Receipt r{o["game_id"].toString(), o["white"].toString(), o["black"].toString(),
					          o["result"].toString(), o["signer"].toString(), static_cast<qint64>(o["finished_at"].toDouble())};
					if (!r.gameId.isEmpty() && r.gameId.size() <= 128 && r.white != r.black
					    && !RsGxsId(r.white.toStdString()).isNull() && !RsGxsId(r.black.toStdString()).isNull()
					    && validResult(r.result) && r.finishedAt > 0 && (r.signer == r.white || r.signer == r.black))
						storeReceipt(canonicalKey(r) + '|' + r.signer, r);
				}
				const QJsonArray gossipedArray = root.value("gossiped").toArray();
				for (const QJsonValue &gv : gossipedArray) {
					const QString key = gv.toString();
					if (!key.isEmpty()) {
						mGossipedReceipts.insert(key);
					}
				}
				loadedFromFile = true;
				CHESS_STORELOG("LOAD leaderboard file " << filePath.toStdString() << ": " << mReceipts.size()
				               << " of " << array.size() << " results valid, " << mGossipedReceipts.size()
				               << " already sent");
			}
		}
	} else {
		CHESS_STORELOG("LOAD no leaderboard file " << (filePath.isEmpty() ? std::string("(no account directory)")
		                                                                   : filePath.toStdString()));
	}

	// If no file existed or failed to load, check and migrate legacy data from Settings
	if (!loadedFromFile) {
		const QStringList gossiped = Settings->valueFromGroup(
		        "RetroChess", "LeaderboardGossipedReceipts").toStringList();
		mGossipedReceipts = QSet<QString>(gossiped.cbegin(), gossiped.cend());
		const QByteArray raw = Settings->valueFromGroup("RetroChess", "LeaderboardReceipts").toByteArray();
		if (!raw.isEmpty()) {
			const QJsonArray array = QJsonDocument::fromJson(raw).array();
			for (const QJsonValue &v : array) {
				const QJsonObject o = v.toObject();
				Receipt r{o["game_id"].toString(), o["white"].toString(), o["black"].toString(),
				          o["result"].toString(), o["signer"].toString(), static_cast<qint64>(o["finished_at"].toDouble())};
				if (!r.gameId.isEmpty() && r.gameId.size() <= 128 && r.white != r.black
				    && !RsGxsId(r.white.toStdString()).isNull() && !RsGxsId(r.black.toStdString()).isNull()
				    && validResult(r.result) && r.finishedAt > 0 && (r.signer == r.white || r.signer == r.black))
					storeReceipt(canonicalKey(r) + '|' + r.signer, r);
			}
			CHESS_STORELOG("LOAD old leaderboard data from settings: " << mReceipts.size() << " results"
			               << (mReceipts.isEmpty() ? "" : ", moving them to the leaderboard file"));
			// Migrate legacy data immediately into the dedicated file
			if (!mReceipts.isEmpty()) {
				save();
				Settings->remove("RetroChess/LeaderboardReceipts");
				Settings->remove("RetroChess/LeaderboardGossipedReceipts");
				Settings->sync();
			}
		}
	}

	recompute();
}

void RetroChessLeaderboard::save() const
{
	const QString filePath = leaderboardFilePath();
	if (filePath.isEmpty()) {
		QJsonArray array;
		for (const Receipt &r : mReceipts) array.append(QJsonObject{{"game_id",r.gameId},{"white",r.white},
			{"black",r.black},{"result",r.result},{"signer",r.signer},{"finished_at",static_cast<double>(r.finishedAt)}});
		Settings->setValueToGroup("RetroChess", "LeaderboardReceipts", QJsonDocument(array).toJson(QJsonDocument::Compact));
		Settings->setValueToGroup("RetroChess", "LeaderboardGossipedReceipts",
		        QStringList(mGossipedReceipts.values()));
		Settings->sync();
		CHESS_STORELOG("SAVE " << mReceipts.size() << " results to settings (no account directory)");
		return;
	}

	QJsonArray receiptsArray;
	for (const Receipt &r : mReceipts) {
		receiptsArray.append(QJsonObject{
			{"game_id", r.gameId},
			{"white", r.white},
			{"black", r.black},
			{"result", r.result},
			{"signer", r.signer},
			{"finished_at", static_cast<double>(r.finishedAt)}
		});
	}

	QJsonArray gossipedArray;
	for (const QString &key : mGossipedReceipts) {
		gossipedArray.append(key);
	}

	QJsonObject root;
	root["version"] = 1;
	root["receipts"] = receiptsArray;
	root["gossiped"] = gossipedArray;

	QSaveFile saveFile(filePath);
	if (saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		saveFile.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
		if (!saveFile.commit()) {
			CHESS_STORELOG("SAVE leaderboard file " << filePath.toStdString() << " FAILED: "
			               << saveFile.errorString().toStdString());
			saveFile.cancelWriting();
		} else {
			CHESS_STORELOG("SAVE leaderboard file: " << mReceipts.size() << " results, "
			               << mGossipedReceipts.size() << " already sent");
		}
	} else {
		CHESS_STORELOG("SAVE leaderboard file " << filePath.toStdString() << " cannot be opened: "
		               << saveFile.errorString().toStdString());
	}
}

void RetroChessLeaderboard::broadcastReceipt(const Receipt &r, const RsGxsId &excludePeer)
{
	if (!rsRetroChess) return;
	const QString key = canonicalKey(r) + '|' + r.signer;
	mGossipedReceipts.insert(key);
	if (mReceipts.contains(key)) scheduleCommit(false);
	QJsonObject obj{
		{"type", "leaderboard_receipt"},
		{"version", 1},
		{"game_id", r.gameId},
		{"white", r.white},
		{"black", r.black},
		{"result", r.result},
		{"signer", r.signer},
		{"finished_at", static_cast<double>(r.finishedAt)}
	};
	const QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
	if (excludePeer.isNull()) {
		CHESS_LBLOG("SEND receipt " << LB_RECEIPT(r) << " to confirmed peers");
		rsRetroChess->broadcastLeaderboardDataGxs(data);
	} else {
		const auto active = rsRetroChess->activeGxsTunnels();
		int relayed = 0;
		for (const RsGxsId &peer : active) {
			if (peer != excludePeer) {
				if (rsRetroChess->sendLeaderboardDataGxs(peer, data)) ++relayed;
			}
		}
		CHESS_LBLOG("RELAY receipt " << LB_RECEIPT(r) << " learned from=" << excludePeer
		            << " to " << relayed << " peer(s)");
	}
}

void RetroChessLeaderboard::sendSyncToPeer(const RsGxsId &peerId, const QJsonObject &request)
{
	if (!rsRetroChess || peerId.isNull() || mReceipts.isEmpty()) return;

	// Incremental only when the peer quotes our current epoch and a sequence
	// number we could have given it. Current versions without a valid cursor
	// get the full history at most every kFullSyncMinIntervalMs; old versions
	// (no "since") at most every kLegacyFullSyncMinIntervalMs.
	const bool hasCursor = request.contains("since") && request.contains("epoch");
	const quint64 since = static_cast<quint64>(request.value("since").toDouble());
	const bool incremental = hasCursor && request.value("epoch").toString() == mSyncEpoch
	        && since <= mNextSeq;
	const qint64 now = mSyncClock.elapsed();
	const char *kind = incremental ? "incremental" : (!hasCursor ? "full (old version, no cursor)"
	        : (request.value("epoch").toString().isEmpty() ? "full (no cursor yet)"
	        : (request.value("epoch").toString() != mSyncEpoch ? "full (cursor from before our restart)"
	        : "full (cursor ahead of our sequence)")));
	if (mLastSyncResponse.contains(peerId)
	        && now - mLastSyncResponse.value(peerId) < kIncrementalSyncMinIntervalMs) {
		CHESS_LBLOG("SYNC answer to=" << peerId << " " << kind << " skipped: answered "
		            << (now - mLastSyncResponse.value(peerId)) / 1000 << "s ago (limit "
		            << kIncrementalSyncMinIntervalMs / 1000 << "s)");
		return;
	}
	const qint64 fullInterval = hasCursor ? kFullSyncMinIntervalMs : kLegacyFullSyncMinIntervalMs;
	if (!incremental && mLastFullSyncResponse.contains(peerId)
	        && now - mLastFullSyncResponse.value(peerId) < fullInterval) {
		CHESS_LBLOG("SYNC answer to=" << peerId << " " << kind << " skipped: full history sent "
		            << (now - mLastFullSyncResponse.value(peerId)) / 1000 << "s ago (limit "
		            << fullInterval / 1000 << "s)");
		return;
	}
	mLastSyncResponse.insert(peerId, now);
	if (!incremental) mLastFullSyncResponse.insert(peerId, now);

	QList<Receipt> toSend;
	const QString peer = QString::fromStdString(peerId.toStdString());
	for (const Receipt &r : mReceipts)
		if ((!incremental || r.seq > since) && r.learnedFrom != peer) toSend.append(r);
	// Nothing new: the peer is already up to date, send nothing at all.
	if (toSend.isEmpty()) {
		CHESS_LBLOG("SYNC answer to=" << peerId << " " << kind << " since=" << since
		            << ": nothing new, peer is up to date");
		return;
	}
	CHESS_LBLOG("SYNC answer to=" << peerId << " " << kind << " since=" << since << ": sending "
	            << toSend.size() << " of " << mReceipts.size() << " results in "
	            << (toSend.size() + kSyncBatchSize - 1) / kSyncBatchSize << " batch(es), our seq=" << mNextSeq);
	std::sort(toSend.begin(), toSend.end(),
	          [](const Receipt &a, const Receipt &b) { return a.seq < b.seq; });

	for (int start = 0; start < toSend.size(); start += kSyncBatchSize) {
		QJsonArray batch;
		const int end = std::min<int>(start + kSyncBatchSize, toSend.size());
		for (int i = start; i < end; ++i) {
			const Receipt &r = toSend.at(i);
			batch.append(QJsonObject{
				{"game_id", r.gameId},
				{"white", r.white},
				{"black", r.black},
				{"result", r.result},
				{"signer", r.signer},
				{"finished_at", static_cast<double>(r.finishedAt)}
			});
		}
		QJsonObject syncMsg{
			{"type", "leaderboard_sync"},
			{"version", 1},
			{"receipts", batch}
		};
		// The last batch carries the cursor the peer quotes next time.
		// Older versions ignore these fields.
		if (end == toSend.size()) {
			syncMsg["epoch"] = mSyncEpoch;
			syncMsg["seq"] = static_cast<double>(mNextSeq);
			syncMsg["final"] = true;
		}
		rsRetroChess->sendLeaderboardDataGxs(peerId, QJsonDocument(syncMsg).toJson(QJsonDocument::Compact));
	}
}

void RetroChessLeaderboard::sendSyncRequest(const RsGxsId &peerId)
{
	if (!rsRetroChess || peerId.isNull()) return;
	bool online = false;
	for (const auto &peer : rsRetroChess->availableChessPeers()) {
		if (peer.gxs && peer.endpointId == QString::fromStdString(peerId.toStdString())
		        && (peer.status == "available" || peer.status == "busy" || peer.status == "playing")) {
			online = true;
			break;
		}
	}
	if (!online) return;
	const qint64 now = mSyncClock.elapsed();
	if (mLastSyncRequest.contains(peerId)
	        && now - mLastSyncRequest.value(peerId) < kSyncRequestIntervalMs) return;
	QJsonObject req{
		{"type", "leaderboard_sync_req"},
		{"version", 1}
	};
	// Ask only for what this peer has not sent us yet. Without a cursor (first
	// contact) since=0 with an empty epoch requests the full history.
	const SyncCursor cursor = mSyncCursors.value(peerId);
	req["epoch"] = cursor.epoch;
	req["since"] = static_cast<double>(cursor.seq);
	const bool sent = rsRetroChess->sendLeaderboardDataGxs(peerId, QJsonDocument(req).toJson(QJsonDocument::Compact));
	CHESS_LBLOG("SYNC request to=" << peerId << (cursor.epoch.isEmpty() ? " full history (no cursor yet)"
	            : " incremental") << " since=" << cursor.seq << " ok=" << sent);
	if (sent)
		mLastSyncRequest.insert(peerId, now);
}

void RetroChessLeaderboard::handleTunnelReady(const RsGxsId &peerId)
{
	if (peerId.isNull()) return;
	sendSyncRequest(peerId);
}

void RetroChessLeaderboard::handleTunnelData(const RsGxsId &sender, const QByteArray &data)
{
	const QJsonObject obj = QJsonDocument::fromJson(data).object();
	const QString type = obj.value("type").toString();
	if (type == "leaderboard_sync_req") {
		CHESS_LBLOG("SYNC request from=" << sender
		            << (obj.contains("since") ? " since=" + std::to_string(static_cast<quint64>(obj.value("since").toDouble()))
		                                      : std::string(" (old version, no cursor)")));
		sendSyncToPeer(sender, obj);
	} else if (type == "leaderboard_receipt") {
		Receipt r{
			obj.value("game_id").toString(),
			obj.value("white").toString(),
			obj.value("black").toString(),
			obj.value("result").toString(),
			obj.value("signer").toString(),
			static_cast<qint64>(obj.value("finished_at").toDouble())
		};
		const QString key = canonicalKey(r) + '|' + r.signer;
		const bool isNew = !mReceipts.contains(key) && !mPending.contains(key);
		CHESS_LBLOG("RECV receipt from=" << sender << " " << LB_RECEIPT(r)
		            << (isNew ? "" : " (already known)"));
		consumeReceipt(r, sender);
		// Relay newly seen receipts (also unconfirmed ones, so that peers
		// further away can still collect enough witnesses).
		if (isNew && (mReceipts.contains(key) || mPending.contains(key))
		        && !mGossipedReceipts.contains(key)) {
			broadcastReceipt(r, sender);
		}
	} else if (type == "leaderboard_sync") {
		// Remember how far we got with this peer (sent on its last batch only).
		if (obj.value("final").toBool() && !obj.value("epoch").toString().isEmpty()) {
			SyncCursor &cursor = mSyncCursors[sender];
			cursor.epoch = obj.value("epoch").toString();
			cursor.seq = static_cast<quint64>(obj.value("seq").toDouble());
		}
		const QJsonArray array = obj.value("receipts").toArray();
		int stored = 0, waiting = 0, known = 0;
		for (const QJsonValue &val : array) {
			const QJsonObject o = val.toObject();
			Receipt r{
				o.value("game_id").toString(),
				o.value("white").toString(),
				o.value("black").toString(),
				o.value("result").toString(),
				o.value("signer").toString(),
				static_cast<qint64>(o.value("finished_at").toDouble())
			};
			const QString key = canonicalKey(r) + '|' + r.signer;
			const bool isNew = !mReceipts.contains(key) && !mPending.contains(key);
			if (consumeReceipt(r, sender)) ++stored;
			else if (mPending.contains(key)) ++waiting;
			else ++known;
			if (isNew && (mReceipts.contains(key) || mPending.contains(key))
			        && !mGossipedReceipts.contains(key)) {
				broadcastReceipt(r, sender);
			}
		}
		CHESS_LBLOG("SYNC batch from=" << sender << ": " << array.size() << " results, "
		            << stored << " stored, " << waiting << " waiting for witnesses, "
		            << known << " already known or invalid"
		            << (obj.value("final").toBool() ? ", last batch, cursor seq="
		                + std::to_string(static_cast<quint64>(obj.value("seq").toDouble())) : std::string()));
	}
}

void RetroChessLeaderboard::synchronizeTunnels()
{
	if (!rsRetroChess) return;
	const auto active = rsRetroChess->activeGxsTunnels();
	for (auto it = mLastSyncRequest.begin(); it != mLastSyncRequest.end();) {
		if (std::find(active.begin(), active.end(), it.key()) == active.end())
			it = mLastSyncRequest.erase(it);
		else ++it;
	}
	for (auto it = mLastSyncResponse.begin(); it != mLastSyncResponse.end();) {
		if (mSyncClock.elapsed() - it.value() >= kIncrementalSyncMinIntervalMs)
			it = mLastSyncResponse.erase(it);
		else ++it;
	}
	// Keep entries for the longest (old version, 30 min) limit; the 5 min limit
	// for current versions is checked in sendSyncToPeer().
	for (auto it = mLastFullSyncResponse.begin(); it != mLastFullSyncResponse.end();) {
		if (mSyncClock.elapsed() - it.value() >= kLegacyFullSyncMinIntervalMs)
			it = mLastFullSyncResponse.erase(it);
		else ++it;
	}
	for (const RsGxsId &peer : active) {
		sendSyncRequest(peer);
	}
}
