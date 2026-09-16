#pragma once

#include <QObject>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QString>
#include <QByteArray>
#include <retroshare/rstypes.h>

class QTableWidget;
class QTimer;
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
	void consumeReceipt(const Receipt &receipt);
	void recompute();
	void load();
	void save() const;
	void broadcastReceipt(const Receipt &receipt, const RsGxsId &excludePeer = RsGxsId());
	void sendSyncToPeer(const RsGxsId &peerId);
	void sendSyncRequest(const RsGxsId &peerId);
	static QString canonicalKey(const Receipt &r);
	static bool validResult(const QString &result);

	QMap<QString, Receipt> mReceipts;
	QMap<QString, Player> mPlayers;
	QSet<QString> mGossipedReceipts;
	QTimer *mSyncTimer;
};
