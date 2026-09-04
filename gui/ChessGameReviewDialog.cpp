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

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

ChessGameReviewDialog::ChessGameReviewDialog(
        const ChessGameRecord &game, QWidget *parent)
    : QDialog(parent), m_game(game), m_moves(nullptr), m_first(nullptr),
      m_previous(nullptr), m_next(nullptr), m_last(nullptr),
      m_positionLabel(nullptr), m_ply(0)
{
	setWindowTitle(tr("Review: %1 vs %2").arg(game.whitePlayer, game.blackPlayer));
	setMinimumSize(760, 540);
	QHBoxLayout *root = new QHBoxLayout(this);

	QWidget *board = new QWidget(this);
	QGridLayout *boardLayout = new QGridLayout(board);
	boardLayout->setContentsMargins(0, 0, 0, 0);
	boardLayout->setSpacing(0);
	for (int index = 0; index < 64; ++index) {
		QLabel *square = new QLabel(board);
		square->setFixedSize(58, 58);
		square->setAlignment(Qt::AlignCenter);
		m_squares[index] = square;
		boardLayout->addWidget(square, index / 8, index % 8);
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
	for (int index = 0; index < game.moves.size(); ++index) {
		const int row = index / 2;
		if (!m_moves->item(row, 0))
			m_moves->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
		m_moves->setItem(row, index % 2 + 1, new QTableWidgetItem(game.moves[index]));
	}
	side->addWidget(m_moves, 1);

	QHBoxLayout *navigation = new QHBoxLayout;
	m_first = new QPushButton(QIcon(":/images/skip-prev-solid.png"), QString(), this);
	m_previous = new QPushButton(QIcon(":/images/nav-arrow-left-solid.png"), QString(), this);
	m_next = new QPushButton(QIcon(":/images/nav-arrow-right-solid.png"), QString(), this);
	m_last = new QPushButton(QIcon(":/images/skip-next-solid.png"), QString(), this);
	for (QPushButton *button : {m_first, m_previous, m_next, m_last}) {
		button->setFixedSize(36, 28);
		navigation->addWidget(button);
	}
	side->addLayout(navigation);
	m_positionLabel = new QLabel(this);
	side->addWidget(m_positionLabel);
	QDialogButtonBox *close = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
	side->addWidget(close);
	root->addLayout(side, 1);

	connect(m_first, &QPushButton::clicked, this, [this]() { showPly(0); });
	connect(m_previous, &QPushButton::clicked, this, [this]() { showPly(m_ply - 1); });
	connect(m_next, &QPushButton::clicked, this, [this]() { showPly(m_ply + 1); });
	connect(m_last, &QPushButton::clicked, this, [this]() { showPly(m_game.positions.size() - 1); });
	connect(m_moves, &QTableWidget::cellClicked, this, [this](int row, int column) {
		if (column > 0) showPly(qMin(row * 2 + column, m_game.positions.size() - 1));
	});
	showPly(0);
}

void ChessGameReviewDialog::showPly(int ply)
{
	if (m_game.positions.isEmpty()) return;
	m_ply = qBound(0, ply, m_game.positions.size() - 1);
	const QString position = m_game.positions[m_ply];
	const RetroChessBoardTheme theme = RetroChessSettings::boardTheme();
	for (int index = 0; index < 64; ++index) {
		QLabel *square = m_squares[index];
		const QColor color = ((index / 8 + index % 8) % 2) ? theme.dark : theme.light;
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
	if (m_ply == 0) m_moves->clearSelection();
	else if (QTableWidgetItem *item = m_moves->item((m_ply - 1) / 2, (m_ply - 1) % 2 + 1)) {
		m_moves->setCurrentItem(item);
		m_moves->scrollToItem(item);
	}
	m_positionLabel->setText(tr("Position %1 of %2").arg(m_ply).arg(m_game.positions.size() - 1));
	updateControls();
}

void ChessGameReviewDialog::updateControls()
{
	m_first->setEnabled(m_ply > 0);
	m_previous->setEnabled(m_ply > 0);
	m_next->setEnabled(m_ply + 1 < m_game.positions.size());
	m_last->setEnabled(m_ply + 1 < m_game.positions.size());
}
