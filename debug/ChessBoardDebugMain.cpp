/*******************************************************************************
 * debug/ChessBoardDebugMain.cpp                                              *
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

#include "../gui/ChessBoard.h"
#include "../gui/ChessPosition.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDateTime>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

class ChessBoardDebugWindow : public QWidget
{
public:
	ChessBoardDebugWindow()
	    : m_board(new ChessBoard(this)), m_fen(new QLineEdit(this)),
	      m_log(new QTextEdit(this)), m_selected(-1)
	{
		setWindowTitle(tr("RetroChess Board Debug"));
		QHBoxLayout *root = new QHBoxLayout(this);
		m_board->setFixedSize(512, 512);
		for (int square = 0; square < 64; ++square) {
			QLabel *label = new QLabel(m_board);
			label->setGeometry((square % 8) * 64, (square / 8) * 64, 64, 64);
			label->setAlignment(Qt::AlignCenter);
			m_squares[square] = label;
			m_board->registerSquare(label, square);
		}
		root->addWidget(m_board);

		QVBoxLayout *tools = new QVBoxLayout;
		tools->addWidget(new QLabel(tr("FEN position"), this));
		tools->addWidget(m_fen);
		QHBoxLayout *buttons = new QHBoxLayout;
		QPushButton *load = new QPushButton(tr("Load FEN"), this);
		QPushButton *reset = new QPushButton(tr("Reset"), this);
		QPushButton *copy = new QPushButton(tr("Copy FEN"), this);
		buttons->addWidget(load);
		buttons->addWidget(reset);
		buttons->addWidget(copy);
		tools->addLayout(buttons);
		m_log->setReadOnly(true);
		m_log->setLineWrapMode(QTextEdit::NoWrap);
		tools->addWidget(m_log, 1);
		root->addLayout(tools, 1);

		m_board->setStateHandlers(
		        [this]() { return m_position.fen(); },
		        [this](const QString &fen, QString *error) {
				const bool loaded = m_position.loadFen(fen, error);
				if (loaded) render();
				return loaded;
			});
		connect(m_board, &ChessBoard::squareActivated,
		        this, [this](int square) { activate(square); });
		connect(m_board, &ChessBoard::moveProduced,
		        this, [this](int from, int to, char promotion) {
				ChessMove move(from, to,
				        static_cast<char>(promotion == '-' ? 0 : promotion));
				append(tr("MOVE %1 hash=%2").arg(move.uci(), m_position.hash()));
			});
		connect(m_board, &ChessBoard::stateLoadRejected,
		        this, [this](const QString &, const QString &reason) {
				append(tr("FEN REJECTED: %1").arg(reason));
			});
		connect(load, &QPushButton::clicked, this, [this]() {
			QString error;
			if (!m_board->loadState(m_fen->text(), &error))
				QMessageBox::warning(this, tr("Invalid FEN"), error);
			else append(tr("FEN LOADED hash=%1").arg(m_position.hash()));
		});
		connect(reset, &QPushButton::clicked, this, [this]() {
			m_position.reset(); m_selected = -1; m_board->setSelectedSquare(-1);
			render(); append(tr("POSITION RESET"));
		});
		connect(copy, &QPushButton::clicked, this, [this]() {
			QApplication::clipboard()->setText(m_position.fen());
		});
		render();
	}

private:
	void activate(int square)
	{
		const char piece = m_position.pieceAt(square);
		if (m_selected < 0) {
			if (!piece || ((piece >= 'A' && piece <= 'Z') != m_position.whiteToMove())) return;
			m_selected = square;
			m_board->setSelectedSquare(square);
			render();
			return;
		}
		if (square == m_selected) {
			m_selected = -1; m_board->setSelectedSquare(-1); render(); return;
		}
		if (piece && ((piece >= 'A' && piece <= 'Z') == m_position.whiteToMove())) {
			m_selected = square; m_board->setSelectedSquare(square); render(); return;
		}
		ChessMove move(m_selected, square);
		const char movingPiece = m_position.pieceAt(m_selected);
		if ((movingPiece == 'P' && square / 8 == 0)
		        || (movingPiece == 'p' && square / 8 == 7)) move.promotion = 'q';
		QString error;
		const int from = m_selected;
		if (!m_position.applyMove(move, &error)) {
			append(tr("REJECT %1: %2").arg(move.uci(), error));
			return;
		}
		m_selected = -1;
		m_board->notifyMoveProduced(from, square, move.promotion ? move.promotion : '-');
		render();
	}

	void render()
	{
		for (int square = 0; square < 64; ++square) {
			QLabel *label = m_squares[square];
			const QColor colour = ((square / 8 + square % 8) % 2)
			        ? QColor("#769656") : QColor("#eeeed2");
			const QString border = square == m_selected ? "border: 3px solid #ffb000;" : QString();
			label->setStyleSheet(QString("background: %1; %2").arg(colour.name(), border));
			label->clear();
			const char piece = m_position.pieceAt(square);
			if (!piece) continue;
			const QChar colourCode = piece >= 'A' && piece <= 'Z' ? 'w' : 'b';
			const QChar pieceCode = QChar(piece).toUpper();
			label->setPixmap(QIcon(QString(":/piece/%1%2.svg")
			        .arg(colourCode).arg(pieceCode)).pixmap(56, 56));
		}
		m_fen->setText(m_position.fen());
	}

	void append(const QString &message)
	{
		m_log->append(QDateTime::currentDateTime().toString("HH:mm:ss.zzz ")
		        + message.toHtmlEscaped());
	}

	ChessBoard *m_board;
	ChessPosition m_position;
	QLabel *m_squares[64];
	QLineEdit *m_fen;
	QTextEdit *m_log;
	int m_selected;
};

int main(int argc, char **argv)
{
	QApplication application(argc, argv);
	ChessBoardDebugWindow window;
	window.show();
	return application.exec();
}
