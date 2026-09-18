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
#include <QSet>
#include <QLocale>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <retroshare/rsinit.h>
#include <retroshare/rsidentity.h>
#include "interface/rsRetroChess.h"
#include "gui/settings/rsharesettings.h"
#include "gui/gxs/GxsIdTreeWidgetItem.h"
#include "gui/ChessGameHistory.h"
#include "gui/common/AvatarDefs.h"
#include "util/HandleRichText.h"
#include <retroshare/rspeers.h>

namespace {
constexpr double kScale = 173.7178;
constexpr double kTau = 0.5;

QString leaderboardFilePath()
{
	const std::string accDir = RsAccounts::AccountDirectory();
	if (accDir.empty()) {
		return QString();
	}
	return QString::fromUtf8(accDir.c_str()) + "/retrochess_leaderboard.json";
}

QString lookupGameHistoryName(const QString &idStr, QString *outPeerId = nullptr)
{
	if (idStr.isEmpty()) return QString();
	for (const ChessGameRecord &game : ChessGameHistory::games()) {
		if (game.whiteGxsId.compare(idStr, Qt::CaseInsensitive) == 0) {
			if (outPeerId && !game.whitePeerId.isEmpty()) {
				*outPeerId = game.whitePeerId;
			}
			if (!game.whitePlayer.isEmpty()) {
				return game.whitePlayer;
			}
		}
		if (game.blackGxsId.compare(idStr, Qt::CaseInsensitive) == 0) {
			if (outPeerId && !game.blackPeerId.isEmpty()) {
				*outPeerId = game.blackPeerId;
			}
			if (!game.blackPlayer.isEmpty()) {
				return game.blackPlayer;
			}
		}
	}
	return QString();
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

RetroChessLeaderboard::RetroChessLeaderboard(QObject *parent) : QObject(parent), mSyncTimer(new QTimer(this))
{
	load();
	connect(mSyncTimer, &QTimer::timeout, this, &RetroChessLeaderboard::synchronizeTunnels);
	mSyncTimer->setInterval(15000);
	mSyncTimer->start();
	QTimer::singleShot(0, this, &RetroChessLeaderboard::synchronizeTunnels);
}

RetroChessLeaderboard::~RetroChessLeaderboard() = default;

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
	consumeReceipt(r);
	broadcastReceipt(r);
}

void RetroChessLeaderboard::receiveResult(const RsGxsId &signer, const QString &gameId,
	                                       const RsGxsId &white, const RsGxsId &black,
	                                       const QString &result, qint64 finishedAt)
{
	consumeReceipt(Receipt{gameId, QString::fromStdString(white.toStdString()),
	        QString::fromStdString(black.toStdString()), result,
	        QString::fromStdString(signer.toStdString()), finishedAt});
}

void RetroChessLeaderboard::consumeReceipt(const Receipt &r)
{
	if (r.gameId.isEmpty() || r.gameId.size() > 128 || r.white == r.black
        || RsGxsId(r.white.toStdString()).isNull() || RsGxsId(r.black.toStdString()).isNull()
        || r.finishedAt <= 0 || !validResult(r.result)
	    || (r.signer != r.white && r.signer != r.black)) return;
	const QString key = canonicalKey(r) + '|' + r.signer;
	// Duplicate posts cannot count twice. Pick a stable timestamp regardless of
    // arrival order; retain conflicting claims so every node excludes them.
    if (mReceipts.contains(key) && mReceipts.value(key).finishedAt <= r.finishedAt) return;
	mReceipts.insert(key, r);
	save(); recompute(); emit changed();
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
		w.name = displayName(w.id); b.name = displayName(b.id);
		const double score = r.result == "1-0" ? 1.0 : (r.result == "0-1" ? 0.0 : 0.5);
		updatePair(w, b, score);
		if (score == 1.0) { ++w.wins; ++b.losses; }
		else if (score == 0.0) { ++w.losses; ++b.wins; }
		else { ++w.draws; ++b.draws; }
		w.lastPlayed = b.lastPlayed = QDateTime::fromSecsSinceEpoch(r.finishedAt);
	}
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
		bool avatarLoaded = false;
		if (known && details.mAvatar.mSize > 0) {
			avatarLoaded = GxsIdDetails::loadPixmapFromData(
			        details.mAvatar.mData, details.mAvatar.mSize, avatar, GxsIdDetails::MEDIUM);
		}
		if (!avatarLoaded && rsIdentity && rsIdentity->isOwnId(p.id)) {
			AvatarDefs::getOwnAvatar(avatar);
			avatarLoaded = !avatar.isNull();
		}
		if (!avatarLoaded) {
			if (peerId.isEmpty()) {
				lookupGameHistoryName(idStr, &peerId);
			}
			if (!peerId.isEmpty()) {
				AvatarDefs::getAvatarFromSslId(RsPeerId(peerId.toStdString()), avatar);
				avatarLoaded = !avatar.isNull();
			}
		}
		if (!avatarLoaded || avatar.isNull()) {
			avatar = GxsIdDetails::makeDefaultIcon(p.id, GxsIdDetails::MEDIUM);
		}

		// Resolve tooltip avatar (LARGE)
		QPixmap tooltipPixmap;
		bool tooltipLoaded = false;
		if (known && details.mAvatar.mSize > 0) {
			tooltipLoaded = GxsIdDetails::loadPixmapFromData(
			        details.mAvatar.mData, details.mAvatar.mSize,
			        tooltipPixmap, GxsIdDetails::LARGE);
		}
		if (!tooltipLoaded && rsIdentity && rsIdentity->isOwnId(p.id)) {
			AvatarDefs::getOwnAvatar(tooltipPixmap);
			tooltipLoaded = !tooltipPixmap.isNull();
		}
		if (!tooltipLoaded && !peerId.isEmpty()) {
			AvatarDefs::getAvatarFromSslId(RsPeerId(peerId.toStdString()), tooltipPixmap);
			tooltipLoaded = !tooltipPixmap.isNull();
		}
		if (!tooltipLoaded || tooltipPixmap.isNull()) {
			tooltipPixmap = GxsIdDetails::makeDefaultIcon(p.id, GxsIdDetails::LARGE);
		}

		// Actively request unknown identity details from peers (throttled to once every 30 seconds per ID)
		if (rsIdentity && !p.id.isNull() && !rsIdentity->isOwnId(p.id)) {
			if (!known || details.mNickname.empty() || details.mAvatar.mSize == 0) {
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
		QString embeddedImage;
		if (RsHtml::makeEmbeddedImage(
		        tooltipPixmap.scaled(
		                QSize(96, 96), Qt::KeepAspectRatio,
		                Qt::SmoothTransformation).toImage(),
		        embeddedImage, -1))
			playerTooltip = QString("<table><tr><td>%1</td><td>%2</td></tr></table>")
			        .arg(embeddedImage, playerTooltip);

		const QStringList values{QString::number(row + 1), playerName, QString::number(qRound(p.rating)),
		                         QString::number(qRound(p.rd)), QString::number(p.games()),
		                         QString::number(p.wins), QString::number(p.draws), QString::number(p.losses),
		                         p.provisional() ? tr("Provisional") : tr("Rated"),
		                         QLocale().toString(p.lastPlayed.toLocalTime(), QLocale::ShortFormat)};
		for (int col = 0; col < values.size(); ++col) {
            auto *item = new QTableWidgetItem(values.at(col));
            if (col == 0 || (col >= 2 && col <= 7)) {
                item->setData(Qt::DisplayRole, values.at(col).toInt());
                item->setTextAlignment(Qt::AlignCenter);
            }
            if (col == 1) {
                item->setIcon(QIcon(avatar));
                item->setToolTip(playerTooltip);
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
		if (file.open(QIODevice::ReadOnly)) {
			const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
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
						mReceipts.insert(canonicalKey(r) + '|' + r.signer, r);
				}
				const QJsonArray gossipedArray = root.value("gossiped").toArray();
				for (const QJsonValue &gv : gossipedArray) {
					const QString key = gv.toString();
					if (!key.isEmpty()) {
						mGossipedReceipts.insert(key);
					}
				}
				loadedFromFile = true;
			}
		}
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
					mReceipts.insert(canonicalKey(r) + '|' + r.signer, r);
			}
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
		saveFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
		if (!saveFile.commit()) {
			saveFile.cancelWriting();
		}
	}
}

void RetroChessLeaderboard::broadcastReceipt(const Receipt &r, const RsGxsId &excludePeer)
{
	if (!rsRetroChess) return;
	const QString key = canonicalKey(r) + '|' + r.signer;
	mGossipedReceipts.insert(key);
	save();
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
		rsRetroChess->broadcastLeaderboardDataGxs(data);
	} else {
		const auto active = rsRetroChess->activeGxsTunnels();
		for (const RsGxsId &peer : active) {
			if (peer != excludePeer) {
				rsRetroChess->sendLeaderboardDataGxs(peer, data);
			}
		}
	}
}

void RetroChessLeaderboard::sendSyncToPeer(const RsGxsId &peerId)
{
	if (!rsRetroChess || peerId.isNull() || mReceipts.isEmpty()) return;
	QJsonArray currentBatch;
	for (const Receipt &r : mReceipts) {
		currentBatch.append(QJsonObject{
			{"game_id", r.gameId},
			{"white", r.white},
			{"black", r.black},
			{"result", r.result},
			{"signer", r.signer},
			{"finished_at", static_cast<double>(r.finishedAt)}
		});
		if (currentBatch.size() >= 10) {
			QJsonObject syncMsg{
				{"type", "leaderboard_sync"},
				{"version", 1},
				{"receipts", currentBatch}
			};
			rsRetroChess->sendLeaderboardDataGxs(peerId, QJsonDocument(syncMsg).toJson(QJsonDocument::Compact));
			currentBatch = QJsonArray();
		}
	}
	if (!currentBatch.isEmpty()) {
		QJsonObject syncMsg{
			{"type", "leaderboard_sync"},
			{"version", 1},
			{"receipts", currentBatch}
		};
		rsRetroChess->sendLeaderboardDataGxs(peerId, QJsonDocument(syncMsg).toJson(QJsonDocument::Compact));
	}
}

void RetroChessLeaderboard::sendSyncRequest(const RsGxsId &peerId)
{
	if (!rsRetroChess || peerId.isNull()) return;
	QJsonObject req{
		{"type", "leaderboard_sync_req"},
		{"version", 1}
	};
	rsRetroChess->sendLeaderboardDataGxs(peerId, QJsonDocument(req).toJson(QJsonDocument::Compact));
}

void RetroChessLeaderboard::handleTunnelReady(const RsGxsId &peerId)
{
	if (peerId.isNull()) return;
	sendSyncToPeer(peerId);
	sendSyncRequest(peerId);
}

void RetroChessLeaderboard::handleTunnelData(const RsGxsId &sender, const QByteArray &data)
{
	const QJsonObject obj = QJsonDocument::fromJson(data).object();
	const QString type = obj.value("type").toString();
	if (type == "leaderboard_sync_req") {
		sendSyncToPeer(sender);
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
		const bool isNew = !mReceipts.contains(key);
		consumeReceipt(r);
		if (isNew && !mGossipedReceipts.contains(key)) {
			broadcastReceipt(r, sender);
		}
	} else if (type == "leaderboard_sync") {
		const QJsonArray array = obj.value("receipts").toArray();
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
			const bool isNew = !mReceipts.contains(key);
			consumeReceipt(r);
			if (isNew && !mGossipedReceipts.contains(key)) {
				broadcastReceipt(r, sender);
			}
		}
	}
}

void RetroChessLeaderboard::synchronizeTunnels()
{
	if (!rsRetroChess) return;
	const auto active = rsRetroChess->activeGxsTunnels();
	for (const RsGxsId &peer : active) {
		sendSyncRequest(peer);
	}
}
