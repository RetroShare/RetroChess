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
#include <retroshare/rsidentity.h>
#include <retroshare/rsgxsforums.h>
#include "gui/settings/rsharesettings.h"

namespace {
constexpr double kScale = 173.7178;
constexpr double kTau = 0.5;
const char kLedgerName[] = "RetroChess Leaderboard";
const char kLedgerMarker[] = "Official distributed RetroChess rating ledger | retrochess:leaderboard:v1";

QString displayName(const RsGxsId &id)
{
	RsIdentityDetails details;
	if (rsIdentity && rsIdentity->getIdDetails(id, details))
		return QString::fromUtf8(details.mNickname.c_str());
	return QString::fromStdString(id.toStdString()).left(12);
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
		const double g = 1.0 / std::sqrt(1.0 + 3.0 * phiJ * phiJ / (M_PI * M_PI));
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
	connect(mSyncTimer, &QTimer::timeout, this, &RetroChessLeaderboard::synchronizeGxsLedger);
	mSyncTimer->setInterval(15000);
	mSyncTimer->start();
	QTimer::singleShot(0, this, &RetroChessLeaderboard::synchronizeGxsLedger);
}

RetroChessLeaderboard::~RetroChessLeaderboard() = default;

bool RetroChessLeaderboard::validResult(const QString &r)
{ return r == "1-0" || r == "0-1" || r == "1/2-1/2"; }

QString RetroChessLeaderboard::canonicalKey(const Receipt &r)
{ return r.gameId + '|' + r.white + '|' + r.black + '|' + r.result; }

void RetroChessLeaderboard::submitResult(const QString &gameId, const RsGxsId &white,
	                                      const RsGxsId &black, const QString &result)
{
	if (gameId.isEmpty() || white.isNull() || black.isNull() || !validResult(result)) return;
	Receipt r{gameId, QString::fromStdString(white.toStdString()),
	          QString::fromStdString(black.toStdString()), result,
	          QString::fromStdString((white == black ? RsGxsId() :
	              (rsIdentity->isOwnId(white) ? white : black)).toStdString()),
	          QDateTime::currentSecsSinceEpoch()};
	if (r.signer.isEmpty()) return;
	consumeReceipt(r);
	if (!mLedgerGroupId.isNull() && publishReceipt(r)) {
		mPublishedReceipts.insert(canonicalKey(r) + '|' + r.signer);
		save();
	}
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
	if (r.gameId.isEmpty() || r.white.isEmpty() || r.black.isEmpty() || !validResult(r.result)
	    || (r.signer != r.white && r.signer != r.black)) return;
	const QString key = canonicalKey(r) + '|' + r.signer;
	if (mReceipts.contains(key)) return;
	// A signer may confirm only one version of a game.
	for (auto it = mReceipts.cbegin(); it != mReceipts.cend(); ++it)
		if (it.value().gameId == r.gameId && it.value().signer == r.signer) return;
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
		if (list.size() < 2) continue;
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

void RetroChessLeaderboard::populate(QTableWidget *table) const
{
	QList<Player> players = mPlayers.values();
	std::sort(players.begin(), players.end(), [](const Player &a, const Player &b) { return a.rating > b.rating; });
	table->setSortingEnabled(false); table->setRowCount(players.size()); table->setColumnCount(10);
	table->setHorizontalHeaderLabels({tr("#"), tr("Player"), tr("Rating"), tr("RD"), tr("Games"),
	                                  tr("W"), tr("D"), tr("L"), tr("Status"), tr("Last played")});
	for (int row = 0; row < players.size(); ++row) {
		const Player &p = players.at(row);
		const QStringList values{QString::number(row + 1), p.name, QString::number(qRound(p.rating)),
		                         QString::number(qRound(p.rd)), QString::number(p.games()),
		                         QString::number(p.wins), QString::number(p.draws), QString::number(p.losses),
		                         p.provisional() ? tr("Provisional") : tr("Rated"),
		                         p.lastPlayed.toLocalTime().toString(Qt::DefaultLocaleShortDate)};
		for (int col = 0; col < values.size(); ++col) table->setItem(row, col, new QTableWidgetItem(values.at(col)));
	}
	table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	table->resizeColumnsToContents(); table->setSortingEnabled(true);
}

void RetroChessLeaderboard::load()
{
	mLedgerGroupId = RsGxsGroupId(Settings->valueFromGroup(
	        "RetroChess", "LeaderboardGxsGroup").toString().toStdString());
	const QStringList published = Settings->valueFromGroup(
	        "RetroChess", "LeaderboardPublishedReceipts").toStringList();
	mPublishedReceipts = QSet<QString>(published.cbegin(), published.cend());
	const QByteArray raw = Settings->valueFromGroup("RetroChess", "LeaderboardReceipts").toByteArray();
	const QJsonArray array = QJsonDocument::fromJson(raw).array();
	for (const QJsonValue &v : array) {
		const QJsonObject o = v.toObject();
		Receipt r{o["game_id"].toString(), o["white"].toString(), o["black"].toString(),
		          o["result"].toString(), o["signer"].toString(), static_cast<qint64>(o["finished_at"].toDouble())};
		if (!r.gameId.isEmpty()) mReceipts.insert(canonicalKey(r) + '|' + r.signer, r);
	}
	recompute();
}

void RetroChessLeaderboard::save() const
{
	QJsonArray array;
	for (const Receipt &r : mReceipts) array.append(QJsonObject{{"game_id",r.gameId},{"white",r.white},
		{"black",r.black},{"result",r.result},{"signer",r.signer},{"finished_at",static_cast<double>(r.finishedAt)}});
	Settings->setValueToGroup("RetroChess", "LeaderboardReceipts", QJsonDocument(array).toJson(QJsonDocument::Compact));
	Settings->setValueToGroup("RetroChess", "LeaderboardGxsGroup",
	        QString::fromStdString(mLedgerGroupId.toStdString()));
	Settings->setValueToGroup("RetroChess", "LeaderboardPublishedReceipts",
	        QStringList(mPublishedReceipts.values()));
	Settings->sync();
}

void RetroChessLeaderboard::synchronizeGxsLedger()
{
	if (!rsGxsForums || !rsIdentity) return;
	// Keep discovering even after creation so independently created bootstrap
	// groups converge on the lexicographically smallest valid protocol group.
	selectOrCreateLedgerGroup();
	if (mLedgerGroupId.isNull()) return;
	rsGxsForums->subscribeToForum(mLedgerGroupId, true);
	readLedgerPosts();
	publishPendingReceipts();
}

void RetroChessLeaderboard::selectOrCreateLedgerGroup()
{
	std::list<RsGroupMetaData> summaries;
	if (!rsGxsForums->getForumsSummaries(summaries)) return;
	QList<RsGxsGroupId> candidates;
	for (const RsGroupMetaData &meta : summaries)
		if (meta.mGroupName == kLedgerName) candidates.append(meta.mGroupId);
	if (!candidates.isEmpty()) {
		std::sort(candidates.begin(), candidates.end(), [](const RsGxsGroupId &a, const RsGxsGroupId &b) {
			return a.toStdString() < b.toStdString();
		});
		RsGxsGroupId selected;
		for (const RsGxsGroupId &id : candidates) {
			std::vector<RsGxsForumGroup> groups;
			if (rsGxsForums->getForumsInfo({id}, groups) && !groups.empty()
			    && groups.front().mDescription == kLedgerMarker) {
				selected = id; break;
			}
		}
		if (!selected.isNull()) {
			if (selected != mLedgerGroupId) {
				mLedgerGroupId = selected;
				mPublishedReceipts.clear();
				save();
				emit changed();
			}
			return;
		}
	}
	if (!mLedgerGroupId.isNull()) return;
	// Wait for normal GXS discovery before creating, which avoids most duplicate groups.
	if (++mDiscoveryAttempts < 3) return;
	std::list<RsGxsId> ownIds;
	rsIdentity->getOwnIds(ownIds);
	RsGxsId author;
	for (const RsGxsId &id : ownIds) {
		RsIdentityDetails details;
		if (rsIdentity->getIdDetails(id, details) && (details.mFlags & RS_IDENTITY_FLAGS_PGP_LINKED)) {
			author = id; break;
		}
	}
	if (author.isNull()) return;
	RsGxsGroupId created;
	std::string error;
	if (rsGxsForums->createForumV2(kLedgerName, kLedgerMarker, author, {},
	        RsGxsCircleType::PUBLIC, RsGxsCircleId(), created, error)) {
		mLedgerGroupId = created;
		rsGxsForums->subscribeToForum(created, true);
		save();
		emit changed();
	}
}

void RetroChessLeaderboard::readLedgerPosts()
{
	std::vector<RsMsgMetaData> metas;
	if (!rsGxsForums->getForumMsgMetaData(mLedgerGroupId, metas)) return;
	std::set<RsGxsMessageId> ids;
	for (const RsMsgMetaData &meta : metas) ids.insert(meta.mMsgId);
	if (ids.empty()) return;
	std::vector<RsGxsForumMsg> posts;
	if (!rsGxsForums->getForumContent(mLedgerGroupId, ids, posts)) return;
	for (const RsGxsForumMsg &post : posts) {
		const QJsonObject o = QJsonDocument::fromJson(QByteArray::fromStdString(post.mMsg)).object();
		if (o.value("type").toString() != "retrochess_result_v1") continue;
		receiveResult(post.mMeta.mAuthorId, o.value("game_id").toString(),
		        RsGxsId(o.value("white").toString().toStdString()),
		        RsGxsId(o.value("black").toString().toStdString()),
		        o.value("result").toString(), static_cast<qint64>(o.value("finished_at").toDouble()));
	}
	std::vector<RsGxsMessageId> readIds(ids.begin(), ids.end());
	rsGxsForums->markRead(mLedgerGroupId, readIds, true);
}

bool RetroChessLeaderboard::publishReceipt(const Receipt &r)
{
	if (mLedgerGroupId.isNull()) return false;
	QJsonObject o{{"type","retrochess_result_v1"},{"version",1},{"game_id",r.gameId},
	              {"white",r.white},{"black",r.black},{"result",r.result},
	              {"finished_at",static_cast<double>(r.finishedAt)}};
	RsGxsMessageId postId;
	std::string error;
	return rsGxsForums->createPost(mLedgerGroupId, "RetroChess result " + r.gameId.toStdString(),
	        QJsonDocument(o).toJson(QJsonDocument::Compact).toStdString(),
	        RsGxsId(r.signer.toStdString()), RsGxsMessageId(), RsGxsMessageId(), postId, error);
}

void RetroChessLeaderboard::publishPendingReceipts()
{
	for (const Receipt &r : mReceipts) {
		const QString key = canonicalKey(r) + '|' + r.signer;
		if (mPublishedReceipts.contains(key)) continue;
		const RsGxsId signer(r.signer.toStdString());
		if (!rsIdentity->isOwnId(signer)) continue;
		if (publishReceipt(r)) mPublishedReceipts.insert(key);
	}
	save();
}
