#pragma once

#include <QObject>
#include <QDateTime>
#include <QMap>
#include <QString>
#include <retroshare/rstypes.h>

class QTableWidget;
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

signals:
	void changed();

private:
	struct Receipt {
		QString gameId, white, black, result, signer;
		qint64 finishedAt = 0;
	};
	void consumeReceipt(const Receipt &receipt);
	void recompute();
	void load();
	void save() const;
	static QString canonicalKey(const Receipt &r);
	static bool validResult(const QString &result);

	QMap<QString, Receipt> mReceipts;
	QMap<QString, Player> mPlayers;
};
