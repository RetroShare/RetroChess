/*******************************************************************************
 * gui/ChessGameReviewDialog.cpp                                              *
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

#include "ChessGameReviewDialog.h"

#include "RetroChessSettings.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioOutput>
#endif
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QMediaPlayer>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

static constexpr int BORDER_SIZE = 20;
static constexpr int TILE_SIZE = 58;
static constexpr int BOARD_FULL_SIZE = BORDER_SIZE * 2 + TILE_SIZE * 8; // 504
static constexpr int BOARD_INNER_SIZE = BOARD_FULL_SIZE - BORDER_SIZE;  // 484

namespace
{
QTableWidgetItem *createMoveTableItem(const QString &notation, bool isWhite)
{
	if (notation.isEmpty()) {
		return new QTableWidgetItem();
	}

	if (notation.startsWith(QLatin1String("O-O"))) {
		return new QTableWidgetItem(notation);
	}

	const QChar firstChar = notation.at(0);
	const QChar upper = firstChar.toUpper();
	if (upper == 'K' || upper == 'Q' || upper == 'R' || upper == 'B' || upper == 'N' || upper == 'H') {
		const QChar pieceCode = (upper == 'H') ? 'N' : upper;
		const QString iconPath = QStringLiteral(":/piece/%1%2.svg")
		        .arg(isWhite ? 'w' : 'b')
		        .arg(pieceCode);
		const QString displayText = notation.mid(1);
		return new QTableWidgetItem(QIcon(iconPath), displayText);
	}

	return new QTableWidgetItem(notation);
}
} // namespace

ChessGameReviewDialog::ChessGameReviewDialog(
        const ChessGameRecord &game, QWidget *parent)
    : QDialog(parent), m_game(game), m_winnerBadge(nullptr), m_loserBadge(nullptr),
      m_resultBar(nullptr), m_resultTextLabel(nullptr), m_resultInfoIcon(nullptr),
      m_moves(nullptr), m_first(nullptr), m_previous(nullptr), m_playPause(nullptr),
      m_next(nullptr), m_last(nullptr), m_positionLabel(nullptr), m_playTimer(nullptr),
      m_moveSound(nullptr), m_captureSound(nullptr), m_ply(0)
{
	for (int i = 0; i < 4; ++i) m_borders[i] = nullptr;
	m_drawBadges[0] = nullptr;
	m_drawBadges[1] = nullptr;

	setModal(false);
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
	setWindowTitle(tr("Review: %1 vs %2").arg(game.whitePlayer, game.blackPlayer));
	setMinimumSize(800, 560);
	resize(800, 560);
	QHBoxLayout *root = new QHBoxLayout(this);

	QWidget *board = new QWidget(this);
	board->setFixedSize(BOARD_FULL_SIZE, BOARD_FULL_SIZE);

	const QColor borderColor = RetroChessSettings::boardTheme().dark.lighter(135);
	const QString borderStyle = QString(
	        "QLabel { background-color: %1; color: black; }")
	        .arg(borderColor.name());

	// Outer borders: top, bottom, left, right (matching chess window)
	m_borders[0] = new QLabel(board);
	m_borders[0]->setGeometry(0, 0, BOARD_FULL_SIZE, BORDER_SIZE);
	m_borders[1] = new QLabel(board);
	m_borders[1]->setGeometry(0, BOARD_INNER_SIZE, BOARD_FULL_SIZE, BORDER_SIZE);
	m_borders[2] = new QLabel(board);
	m_borders[2]->setGeometry(0, BORDER_SIZE, BORDER_SIZE, BOARD_FULL_SIZE - 2 * BORDER_SIZE);
	m_borders[3] = new QLabel(board);
	m_borders[3]->setGeometry(BOARD_INNER_SIZE, BORDER_SIZE, BORDER_SIZE, BOARD_FULL_SIZE - 2 * BORDER_SIZE);

	for (int i = 0; i < 4; ++i) {
		m_borders[i]->setStyleSheet(borderStyle);
	}

	// 64 board squares
	for (int index = 0; index < 64; ++index) {
		const int row = index / 8;
		const int col = index % 8;
		QLabel *square = new QLabel(board);
		square->setGeometry(BORDER_SIZE + col * TILE_SIZE,
		                    BORDER_SIZE + row * TILE_SIZE,
		                    TILE_SIZE, TILE_SIZE);
		square->setAlignment(Qt::AlignCenter);
		m_squares[index] = square;
	}

	// Board coordinates (ranks 8..1 on the left border, files a..h on the bottom border)
	for (int i = 0; i < 8; ++i) {
		QLabel *rankLabel = new QLabel(QString::number(8 - i), board);
		rankLabel->setGeometry(0, BORDER_SIZE + i * TILE_SIZE, BORDER_SIZE, TILE_SIZE);
		rankLabel->setAlignment(Qt::AlignCenter);
		rankLabel->setStyleSheet(
		        "QLabel { color: #353525; background: transparent; font-weight: bold; }");
		rankLabel->raise();

		QLabel *fileLabel = new QLabel(QString(QChar('a' + i)), board);
		fileLabel->setGeometry(BORDER_SIZE + i * TILE_SIZE, BOARD_INNER_SIZE, TILE_SIZE, BORDER_SIZE);
		fileLabel->setAlignment(Qt::AlignCenter);
		fileLabel->setStyleSheet(
		        "QLabel { color: #353525; background: transparent; font-weight: bold; }");
		fileLabel->raise();
	}

	m_winnerBadge = new QLabel(board);
	m_winnerBadge->setAlignment(Qt::AlignCenter);
	m_winnerBadge->setStyleSheet(
	        "QLabel {"
	        "  background-color: #ffffff;"
	        "  color: #2e7d32;"
	        "  font-weight: bold;"
	        "  font-size: 11px;"
	        "  border: 1px solid #a5d6a7;"
	        "  border-radius: 9px;"
	        "  padding: 1px 7px;"
	        "}");
	m_winnerBadge->hide();

	m_loserBadge = new QLabel(board);
	m_loserBadge->setAlignment(Qt::AlignCenter);
	m_loserBadge->setStyleSheet(
	        "QLabel {"
	        "  background-color: #ffffff;"
	        "  color: #c62828;"
	        "  font-weight: bold;"
	        "  font-size: 11px;"
	        "  border: 1px solid #ef9a9a;"
	        "  border-radius: 9px;"
	        "  padding: 1px 7px;"
	        "}");
	m_loserBadge->hide();

	for (int i = 0; i < 2; ++i) {
		m_drawBadges[i] = new QLabel(board);
		m_drawBadges[i]->setAlignment(Qt::AlignCenter);
		m_drawBadges[i]->setText(QString::fromUtf8("½"));
		m_drawBadges[i]->setStyleSheet(
		        "QLabel {"
		        "  background-color: #45433f;"
		        "  color: #ffffff;"
		        "  font-weight: bold;"
		        "  font-size: 13px;"
		        "  border: 1px solid rgba(255, 255, 255, 0.45);"
		        "  border-radius: 11px;"
		        "  min-width: 22px;"
		        "  max-width: 22px;"
		        "  min-height: 22px;"
		        "  max-height: 22px;"
		        "}");
		m_drawBadges[i]->hide();
	}

	root->addWidget(board, 0, Qt::AlignCenter);

	QVBoxLayout *side = new QVBoxLayout;
	QLabel *players = new QLabel(
	        tr("White: %1\nBlack: %2\nResult: %3\n%4")
	                .arg(game.whitePlayer, game.blackPlayer, game.result, game.reason), this);
	players->setWordWrap(true);
	side->addWidget(players);
	m_moves = new QTableWidget((game.moves.size() + 1) / 2, 3, this);
	m_moves->setHorizontalHeaderLabels({tr("#"), tr("White"), tr("Black")});
	m_moves->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_moves->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_moves->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
	m_moves->verticalHeader()->hide();
	m_moves->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_moves->setSelectionMode(QAbstractItemView::SingleSelection);
	m_moves->setSelectionBehavior(QAbstractItemView::SelectItems);
	m_moves->setShowGrid(false);
	m_moves->setAlternatingRowColors(true);
	m_moves->setIconSize(QSize(18, 18));
	for (int index = 0; index < game.moves.size(); ++index) {
		const int row = index / 2;
		if (!m_moves->item(row, 0)) {
			auto *numItem = new QTableWidgetItem(QString::number(row + 1));
			numItem->setTextAlignment(Qt::AlignCenter);
			m_moves->setItem(row, 0, numItem);
			m_moves->setRowHeight(row, 24);
		}
		const int col = index % 2 + 1;
		m_moves->setItem(row, col, createMoveTableItem(game.moves[index], col == 1));
	}
	side->addWidget(m_moves, 1);

	// Result summary row (e.g. "1/2-1/2 (i)")
	m_resultBar = new QWidget(this);
	QHBoxLayout *resultLayout = new QHBoxLayout(m_resultBar);
	resultLayout->setContentsMargins(4, 2, 4, 4);
	resultLayout->setSpacing(6);
	m_resultTextLabel = new QLabel(m_resultBar);
	m_resultTextLabel->setStyleSheet("QLabel { font-size: 13px; font-weight: bold; color: #444; }");
	m_resultInfoIcon = new QLabel(QStringLiteral("i"), m_resultBar);
	m_resultInfoIcon->setAlignment(Qt::AlignCenter);
	m_resultInfoIcon->setCursor(Qt::PointingHandCursor);
	m_resultInfoIcon->setStyleSheet(
	        "QLabel {"
	        "  background-color: #726f6a;"
	        "  color: #ffffff;"
	        "  font-weight: bold;"
	        "  font-size: 11px;"
	        "  font-family: sans-serif;"
	        "  border-radius: 8px;"
	        "  min-width: 16px;"
	        "  max-width: 16px;"
	        "  min-height: 16px;"
	        "  max-height: 16px;"
	        "}");
	resultLayout->addWidget(m_resultTextLabel);
	resultLayout->addWidget(m_resultInfoIcon);
	resultLayout->addStretch(1);
	m_resultBar->hide();
	side->addWidget(m_resultBar);

	// Audio setup
	m_moveSound = new QMediaPlayer(this);
	m_captureSound = new QMediaPlayer(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	m_moveSound->setAudioOutput(new QAudioOutput(m_moveSound));
	m_captureSound->setAudioOutput(new QAudioOutput(m_captureSound));
	m_moveSound->setSource(QUrl("qrc:/sound/Move.mp3"));
	m_captureSound->setSource(QUrl("qrc:/sound/Capture.mp3"));
	m_moveSound->audioOutput()->setVolume(0.7f);
	m_captureSound->audioOutput()->setVolume(0.7f);
#else
	m_moveSound->setMedia(QUrl("qrc:/sound/Move.mp3"));
	m_captureSound->setMedia(QUrl("qrc:/sound/Capture.mp3"));
	m_moveSound->setVolume(70);
	m_captureSound->setVolume(70);
#endif

	// Playback timer setup (1000ms per move)
	m_playTimer = new QTimer(this);
	connect(m_playTimer, &QTimer::timeout, this, &ChessGameReviewDialog::stepForward);

	QHBoxLayout *navigation = new QHBoxLayout;
	m_first = new QPushButton(QIcon(":/images/skip-prev-solid.png"), QString(), this);
	m_previous = new QPushButton(QIcon(":/images/chevron-left.png"), QString(), this);
	m_playPause = new QPushButton(QIcon(":/images/play-16.png"), QString(), this);
	m_next = new QPushButton(QIcon(":/images/chevron-right.png"), QString(), this);
	m_last = new QPushButton(QIcon(":/images/skip-next-solid.png"), QString(), this);

	m_first->setToolTip(tr("First position"));
	m_previous->setToolTip(tr("Previous move"));
	m_playPause->setToolTip(tr("Play"));
	m_next->setToolTip(tr("Next move"));
	m_last->setToolTip(tr("Last position"));

	for (QPushButton *button : {m_first, m_previous, m_playPause, m_next, m_last}) {
		button->setFixedSize(36, 28);
		button->setIconSize(QSize(20, 20));
		navigation->addWidget(button);
	}
	side->addLayout(navigation);

	m_positionLabel = new QLabel(this);
	side->addWidget(m_positionLabel);
	QDialogButtonBox *close = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
	side->addWidget(close);
	root->addLayout(side, 1);

	connect(m_first, &QPushButton::clicked, this, [this]() {
		pausePlayback();
		showPly(0, false);
	});
	connect(m_previous, &QPushButton::clicked, this, [this]() {
		pausePlayback();
		showPly(m_ply - 1, false);
	});
	connect(m_playPause, &QPushButton::clicked, this, &ChessGameReviewDialog::togglePlayPause);
	connect(m_next, &QPushButton::clicked, this, [this]() {
		pausePlayback();
		showPly(m_ply + 1, true);
	});
	connect(m_last, &QPushButton::clicked, this, [this]() {
		pausePlayback();
		showPly(m_game.positions.size() - 1, false);
	});
	connect(m_moves, &QTableWidget::cellClicked, this, [this](int row, int column) {
		if (column > 0) {
			pausePlayback();
			showPly(qMin(row * 2 + column, m_game.positions.size() - 1), false);
		}
	});
	showPly(0, false);
}

ChessGameReviewDialog::~ChessGameReviewDialog()
{
	if (m_playTimer) m_playTimer->stop();
	if (m_moveSound) m_moveSound->stop();
	if (m_captureSound) m_captureSound->stop();
}

void ChessGameReviewDialog::togglePlayPause()
{
	if (m_playTimer && m_playTimer->isActive()) {
		pausePlayback();
	} else {
		startPlayback();
	}
}

void ChessGameReviewDialog::startPlayback()
{
	if (m_game.positions.size() <= 1) return;
	if (m_ply >= m_game.positions.size() - 1) {
		showPly(0, false);
	}
	m_playTimer->start(1000);
	updatePlayPauseButton();
}

void ChessGameReviewDialog::pausePlayback()
{
	if (m_playTimer && m_playTimer->isActive()) {
		m_playTimer->stop();
	}
	updatePlayPauseButton();
}

void ChessGameReviewDialog::stepForward()
{
	if (m_ply + 1 < m_game.positions.size()) {
		showPly(m_ply + 1, true);
		if (m_ply >= m_game.positions.size() - 1) {
			pausePlayback();
		}
	} else {
		pausePlayback();
	}
}

void ChessGameReviewDialog::updatePlayPauseButton()
{
	const bool playing = m_playTimer && m_playTimer->isActive();
	m_playPause->setIcon(QIcon(playing ? ":/images/pause-24.png" : ":/images/play-16.png"));
	m_playPause->setToolTip(playing ? tr("Pause") : tr("Play"));
}

void ChessGameReviewDialog::showPly(int ply, bool playSound)
{
	if (m_game.positions.isEmpty()) return;
	const int previousPly = m_ply;
	m_ply = qBound(0, ply, m_game.positions.size() - 1);
	const QString position = m_game.positions[m_ply];
	const RetroChessBoardTheme theme = RetroChessSettings::boardTheme();

	const QColor borderColor = theme.dark.lighter(135);
	const QString borderStyle = QString(
	        "QLabel { background-color: %1; color: black; }")
	        .arg(borderColor.name());
	for (int i = 0; i < 4; ++i) {
		if (m_borders[i]) m_borders[i]->setStyleSheet(borderStyle);
	}

	int fromSquare = -1;
	int toSquare = -1;
	lastMoveSquares(m_ply, fromSquare, toSquare);

	const bool isFinalPly = (m_ply == m_game.positions.size() - 1 && m_ply > 0);
	int winnerSquare = -1;
	int loserSquare = -1;
	QString loserText;

	if (isFinalPly) {
		int winnerColor = -1;
		if (m_game.result.contains("1-0") || m_game.result.toLower().contains("white win")) {
			winnerColor = 1;
		} else if (m_game.result.contains("0-1") || m_game.result.toLower().contains("black win")) {
			winnerColor = 0;
		} else if (m_game.reason.toLower().contains("white") && m_game.reason.toLower().contains("win")) {
			winnerColor = 1;
		} else if (m_game.reason.toLower().contains("black") && m_game.reason.toLower().contains("win")) {
			winnerColor = 0;
		}

		if (winnerColor >= 0) {
			const int whiteKing = position.indexOf('K');
			const int blackKing = position.indexOf('k');
			winnerSquare = (winnerColor == 1) ? whiteKing : blackKing;
			loserSquare = (winnerColor == 1) ? blackKing : whiteKing;

			bool isCheckmate = false;
			if (!m_game.moves.isEmpty() && m_game.moves.last().contains('#')) {
				isCheckmate = true;
			} else if (m_game.reason.toLower().contains("checkmate") || m_game.reason.toLower().contains("mate")) {
				isCheckmate = true;
			}

			if (isCheckmate) {
				loserText = tr("Checkmate");
			} else if (m_game.reason.toLower().contains("resign")) {
				loserText = tr("Resigned");
			} else if (m_game.reason.toLower().contains("time")) {
				loserText = tr("Time out");
			} else if (m_game.reason.toLower().contains("left")) {
				loserText = tr("Abandoned");
			} else {
				loserText = tr("Defeat");
			}
		}
	}

	for (int index = 0; index < 64; ++index) {
		QLabel *square = m_squares[index];
		const int tileColor = (index / 8 + index % 8) % 2;
		const QColor baseColor = tileColor ? theme.dark : theme.light;
		QColor color = baseColor;

		if (isFinalPly && index == loserSquare) {
			const QColor loseRed(225, 75, 75);
			color = QColor(
			        qRound(baseColor.red() * 0.25 + loseRed.red() * 0.75),
			        qRound(baseColor.green() * 0.25 + loseRed.green() * 0.75),
			        qRound(baseColor.blue() * 0.25 + loseRed.blue() * 0.75));
		} else if (isFinalPly && index == winnerSquare) {
			const QColor winGreen(115, 185, 80);
			color = QColor(
			        qRound(baseColor.red() * 0.35 + winGreen.red() * 0.65),
			        qRound(baseColor.green() * 0.35 + winGreen.green() * 0.65),
			        qRound(baseColor.blue() * 0.35 + winGreen.blue() * 0.65));
		} else if (index == fromSquare || index == toSquare) {
			const qreal opacity = 0.90;
			color = QColor(
			        qRound(baseColor.red() * (1.0 - opacity) + theme.lastMove.red() * opacity),
			        qRound(baseColor.green() * (1.0 - opacity) + theme.lastMove.green() * opacity),
			        qRound(baseColor.blue() * (1.0 - opacity) + theme.lastMove.blue() * opacity));
		}

		square->setStyleSheet(QString("QLabel { background: %1; }").arg(color.name()));
		square->clear();
		if (index >= position.size() || position[index] == '.') continue;
		const QChar encoded = position[index];
		const QString colorPrefix = encoded.isUpper() ? "w" : "b";
		QChar piece = encoded.toUpper();
		if (piece == 'H') piece = 'N';
		const QIcon icon(QString(":/piece/%1%2.svg").arg(colorPrefix).arg(piece));
		square->setPixmap(icon.pixmap(50, 50));
	}

	const bool isDraw = m_game.result.contains("1/2") || m_game.result.toLower().contains("draw")
	        || m_game.reason.toLower().contains("stalemate") || m_game.reason.toLower().contains("draw");

	if (isFinalPly && isDraw) {
		if (m_winnerBadge) m_winnerBadge->hide();
		if (m_loserBadge) m_loserBadge->hide();

		const int whiteKing = position.indexOf('K');
		const int blackKing = position.indexOf('k');
		auto positionCornerBadge = [](QLabel *badge, int squareIndex) {
			if (!badge || squareIndex < 0 || squareIndex >= 64) return;
			const int col = squareIndex % 8;
			const int row = squareIndex / 8;
			const int squareX = BORDER_SIZE + col * TILE_SIZE;
			const int squareY = BORDER_SIZE + row * TILE_SIZE;
			const int bx = squareX + TILE_SIZE - 22 - 2;
			const int by = squareY + 2;
			badge->setGeometry(bx, by, 22, 22);
			badge->raise();
			badge->show();
		};

		if (whiteKing >= 0 && m_drawBadges[0]) positionCornerBadge(m_drawBadges[0], whiteKing);
		if (blackKing >= 0 && m_drawBadges[1]) positionCornerBadge(m_drawBadges[1], blackKing);

		QString explanation;
		if (m_game.reason.toLower().contains("stalemate")) {
			explanation = tr("A draw by stalemate occurs when the player whose turn it is has no legal moves, but their king is not in check.");
			for (int i = 0; i < 2; ++i) if (m_drawBadges[i]) m_drawBadges[i]->setToolTip(tr("Stalemate (½)"));
		} else if (m_game.reason.toLower().contains("repetition")) {
			explanation = tr("A draw by repetition occurs when the same position appears three times in the game with the same player to move and the same possible moves.");
			for (int i = 0; i < 2; ++i) if (m_drawBadges[i]) m_drawBadges[i]->setToolTip(tr("Draw by repetition (½)"));
		} else if (m_game.reason.toLower().contains("50-move") || m_game.reason.toLower().contains("75-move")) {
			explanation = tr("A draw by the 50-move rule occurs when no capture has been made and no pawn has been moved in the last 50 moves.");
			for (int i = 0; i < 2; ++i) if (m_drawBadges[i]) m_drawBadges[i]->setToolTip(tr("Draw by 50-move rule (½)"));
		} else if (m_game.reason.toLower().contains("dead") || m_game.reason.toLower().contains("insufficient")) {
			explanation = tr("A draw occurs when neither player has sufficient material to checkmate the opponent King.");
			for (int i = 0; i < 2; ++i) if (m_drawBadges[i]) m_drawBadges[i]->setToolTip(tr("Draw by insufficient material (½)"));
		} else if (m_game.reason.toLower().contains("agreement") || m_game.reason.toLower().contains("mutual")) {
			explanation = tr("Draw agreed by mutual agreement between both players.");
			for (int i = 0; i < 2; ++i) if (m_drawBadges[i]) m_drawBadges[i]->setToolTip(tr("Draw by agreement (½)"));
		} else {
			explanation = tr("The game ended in a draw.");
			for (int i = 0; i < 2; ++i) if (m_drawBadges[i]) m_drawBadges[i]->setToolTip(tr("Draw (½)"));
		}

		if (m_resultBar) {
			m_resultTextLabel->setText(m_game.result.isEmpty() ? "1/2-1/2" : m_game.result);
			m_resultTextLabel->setToolTip(explanation);
			m_resultInfoIcon->setToolTip(explanation);
			m_resultBar->show();
		}
	} else if (isFinalPly && winnerSquare >= 0 && loserSquare >= 0) {
		for (int i = 0; i < 2; ++i) if (m_drawBadges[i]) m_drawBadges[i]->hide();
		auto positionBadge = [](QLabel *badge, int squareIndex) {
			if (!badge || squareIndex < 0 || squareIndex >= 64) return;
			const int col = squareIndex % 8;
			const int row = squareIndex / 8;
			const int squareX = BORDER_SIZE + col * TILE_SIZE;
			const int squareY = BORDER_SIZE + row * TILE_SIZE;

			badge->adjustSize();
			const int bw = badge->width();
			const int bh = badge->height();

			int bx = squareX + (TILE_SIZE - bw) / 2;
			bx = qBound(BORDER_SIZE, bx, BOARD_INNER_SIZE - bw);

			int by = squareY - bh / 2;
			if (by < BORDER_SIZE) {
				by = squareY + 2;
			}
			badge->move(bx, by);
			badge->raise();
			badge->show();
		};

		m_winnerBadge->setText(tr("Winner"));
		m_loserBadge->setText(loserText);
		positionBadge(m_winnerBadge, winnerSquare);
		positionBadge(m_loserBadge, loserSquare);

		if (m_resultBar) {
			m_resultTextLabel->setText(m_game.result);
			const QString explanation = m_game.reason.isEmpty() ? tr("Game ended") : m_game.reason;
			m_resultTextLabel->setToolTip(explanation);
			m_resultInfoIcon->setToolTip(explanation);
			m_resultBar->show();
		}
	} else {
		for (int i = 0; i < 2; ++i) if (m_drawBadges[i]) m_drawBadges[i]->hide();
		if (m_winnerBadge) m_winnerBadge->hide();
		if (m_loserBadge) m_loserBadge->hide();
		if (m_resultBar) m_resultBar->hide();
	}

	if (m_ply == 0) m_moves->clearSelection();
	else if (QTableWidgetItem *item = m_moves->item((m_ply - 1) / 2, (m_ply - 1) % 2 + 1)) {
		m_moves->setCurrentItem(item);
		m_moves->scrollToItem(item);
	}
	m_positionLabel->setText(tr("Position %1 of %2").arg(m_ply).arg(m_game.positions.size() - 1));
	updateControls();

	if (playSound && m_ply > previousPly) {
		playMoveSound(isCapture(m_ply));
	}
}

void ChessGameReviewDialog::updateControls()
{
	m_first->setEnabled(m_ply > 0);
	m_previous->setEnabled(m_ply > 0);
	m_next->setEnabled(m_ply + 1 < m_game.positions.size());
	m_last->setEnabled(m_ply + 1 < m_game.positions.size());
	m_playPause->setEnabled(m_game.positions.size() > 1);
	updatePlayPauseButton();
}

void ChessGameReviewDialog::playMoveSound(bool capture)
{
	if (capture ? !RetroChessSettings::captureSoundEnabled()
	            : !RetroChessSettings::moveSoundEnabled()) return;
	QMediaPlayer *player = capture ? m_captureSound : m_moveSound;
	if (!player)
		return;
	player->stop();
	player->setPosition(0);
	player->play();
}

bool ChessGameReviewDialog::isCapture(int ply) const
{
	if (ply <= 0) return false;
	if (ply - 1 < m_game.moves.size()) {
		if (m_game.moves[ply - 1].contains('x')) return true;
	}
	if (ply < m_game.positions.size() && ply - 1 < m_game.positions.size()) {
		const QString &prev = m_game.positions[ply - 1];
		const QString &curr = m_game.positions[ply];
		int prevCount = 0;
		int currCount = 0;
		for (const QChar &c : prev) {
			if (c != '.') ++prevCount;
		}
		for (const QChar &c : curr) {
			if (c != '.') ++currCount;
		}
		if (currCount < prevCount) return true;
	}
	return false;
}

void ChessGameReviewDialog::lastMoveSquares(int ply, int &fromSquare, int &toSquare) const
{
	fromSquare = -1;
	toSquare = -1;
	if (ply <= 0 || ply >= m_game.positions.size() || m_game.positions.isEmpty()) {
		return;
	}

	const bool isWhiteMove = (ply % 2 == 1);

	// Try extracting from move notation first
	if (ply - 1 < m_game.moves.size()) {
		const QString move = m_game.moves[ply - 1].trimmed();
		if (move.startsWith("O-O-O")) {
			fromSquare = isWhiteMove ? 60 : 4; // e1 or e8
			toSquare = isWhiteMove ? 58 : 2;   // c1 or c8
			return;
		}
		if (move.startsWith("O-O")) {
			fromSquare = isWhiteMove ? 60 : 4; // e1 or e8
			toSquare = isWhiteMove ? 62 : 6;   // g1 or g8
			return;
		}

		auto parseSquare = [](const QString &str, int start) -> int {
			if (start < 0 || start + 1 >= str.size()) return -1;
			const char f = str[start].toLatin1();
			const char r = str[start + 1].toLatin1();
			if (f >= 'a' && f <= 'h' && r >= '1' && r <= '8') {
				return ('8' - r) * 8 + (f - 'a');
			}
			return -1;
		};

		int sep = move.indexOf('-');
		if (sep == -1) sep = move.indexOf('x');
		if (sep >= 2 && sep + 2 < move.size()) {
			const int from = parseSquare(move, sep - 2);
			const int to = parseSquare(move, sep + 1);
			if (from >= 0 && to >= 0) {
				fromSquare = from;
				toSquare = to;
				return;
			}
		}
	}

	// Compare board snapshots between ply - 1 and ply
	if (ply - 1 >= 0 && ply < m_game.positions.size()) {
		const QString &prev = m_game.positions[ply - 1];
		const QString &curr = m_game.positions[ply];
		if (prev.size() == 64 && curr.size() == 64) {
			QVector<int> emptied;
			QVector<int> filled;
			for (int i = 0; i < 64; ++i) {
				if (prev[i] == curr[i]) continue;
				if (curr[i] == '.') {
					emptied.append(i);
				} else if (curr[i].isUpper() == isWhiteMove) {
					filled.append(i);
				}
			}

			// Castling detection from board diff
			if (emptied.size() == 2 && filled.size() == 2) {
				const int kingStart = isWhiteMove ? 60 : 4;
				if (emptied.contains(kingStart)) {
					fromSquare = kingStart;
					for (int f : filled) {
						if (f == (isWhiteMove ? 62 : 6) || f == (isWhiteMove ? 58 : 2)) {
							toSquare = f;
							break;
						}
					}
					if (toSquare >= 0) return;
				}
			}

			if (!filled.isEmpty()) {
				toSquare = filled.first();
			}
			if (!emptied.isEmpty()) {
				for (int e : emptied) {
					if (prev[e] != '.' && (prev[e].isUpper() == isWhiteMove)) {
						fromSquare = e;
						break;
					}
				}
				if (fromSquare < 0) {
					fromSquare = emptied.first();
				}
			}
		}
	}
}
