/*******************************************************************************
 * gui/RetroChessLeaderboard.h                                                 *
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

#pragma once

#include <QObject>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QString>
#include <QByteArray>
#include <QElapsedTimer>
#include <retroshare/rstypes.h>

class QTableWidget;
class QTimer;
class QPixmap;
struct ChatMessage;

class RetroChessLeaderboard : public QObject
{
	Q_OBJECT
public:
	struct Player {
		RsGxsId id;
		QString name;
		double rating = 1500.0;
		double rd = 350.0;
		double volatility = 0.06;
		int wins = 0, draws = 0, losses = 0;
		QDateTime lastPlayed;
		int games() const { return wins + draws + losses; }
		bool provisional() const { return games() < 10 || rd > 110.0; }
	};

	explicit RetroChessLeaderboard(QObject *parent = nullptr);
	~RetroChessLeaderboard() override;
	void submitResult(const QString &gameId, const RsGxsId &white,
	                  const RsGxsId &black, const QString &result);
	void receiveResult(const RsGxsId &signer, const QString &gameId,
	                   const RsGxsId &white, const RsGxsId &black,
	                   const QString &result, qint64 finishedAt);
	void populate(QTableWidget *table) const;
	bool getPlayer(const RsGxsId &id, Player &player) const;
	// Rich HTML hover tooltip (avatar + Rating/RD/Games + identity id), shared between
	// the available-players list and the in-game player name labels.
	static QString playerTooltipHtml(
	        const QString &name, const QString &endpointId, const QPixmap &avatar,
	        const Player &player);

public slots:
	void handleTunnelData(const RsGxsId &sender, const QByteArray &data);
	void handleTunnelReady(const RsGxsId &peerId);
	void synchronizeTunnels();

signals:
	void changed();

private:
	struct Receipt {
		QString gameId, white, black, result, signer;
		qint64 finishedAt = 0;
		Receipt() = default;
		Receipt(const QString &g, const QString &w, const QString &b,
		        const QString &r, const QString &s, qint64 f)
		    : gameId(g), white(w), black(b), result(r), signer(s), finishedAt(f) {}
	};
	// sender is the authenticated GXS identity of the tunnel peer the receipt
	// arrived from (null for none). Returns true when the receipt was accepted
	// into mReceipts; see the trust rules in RetroChessLeaderboard.cpp.
	bool consumeReceipt(const Receipt &receipt, const RsGxsId &sender);
	void recompute();
	void load();
	void save() const;
	void broadcastReceipt(const Receipt &receipt, const RsGxsId &excludePeer = RsGxsId());
	void sendSyncToPeer(const RsGxsId &peerId);
	void sendSyncRequest(const RsGxsId &peerId);
	static QString canonicalKey(const Receipt &r);
	static bool validResult(const QString &result);

	QMap<QString, Receipt> mReceipts;
	// Receipts relayed by peers other than their signer. They only become part
	// of mReceipts once enough distinct peers reported them.
	QMap<QString, Receipt> mPending;
	QMap<QString, QSet<QString>> mWitnesses;
	QMap<QString, Player> mPlayers;
	QSet<QString> mGossipedReceipts;
	QTimer *mSyncTimer;
	QElapsedTimer mSyncClock;
	QMap<RsGxsId, qint64> mLastSyncRequest;
	QMap<RsGxsId, qint64> mLastSyncResponse;
};
