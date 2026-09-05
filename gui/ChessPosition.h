/*******************************************************************************
 * gui/ChessPosition.h                                                        *
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

#ifndef CHESSPOSITION_H
#define CHESSPOSITION_H

#include <QString>
#include <QVector>

struct ChessMove
{
	ChessMove() : from(-1), to(-1), promotion(0) {}
	ChessMove(int fromSquare, int toSquare, char promotionPiece = 0)
	    : from(fromSquare), to(toSquare), promotion(promotionPiece) {}

	int from = -1;
	int to = -1;
	char promotion = 0;

	QString uci() const;
	static ChessMove fromUci(const QString &text);
};

class ChessPosition
{
public:
	ChessPosition();

	void reset();
	bool loadFen(const QString &fen, QString *error = nullptr);
	QString fen() const;
	QString hash() const;
	char pieceAt(int square) const;
	bool whiteToMove() const { return m_whiteToMove; }

	bool isLegalMove(const ChessMove &move, QString *error = nullptr) const;
	bool applyMove(const ChessMove &move, QString *error = nullptr);
	QVector<ChessMove> legalMoves() const;
	QVector<ChessMove> legalMovesFrom(int square) const;
	bool isInCheck(bool white) const;

private:
	bool isPseudoLegal(const ChessMove &move, QString *error = nullptr) const;
	bool squareAttacked(int square, bool byWhite) const;
	bool clearPath(int from, int to) const;
	void applyUnchecked(const ChessMove &move);
	void setError(QString *error, const QString &message) const;

	char m_board[64];
	bool m_whiteToMove;
	bool m_whiteKingSide;
	bool m_whiteQueenSide;
	bool m_blackKingSide;
	bool m_blackQueenSide;
	int m_enPassant;
	int m_halfmoveClock;
	int m_fullmoveNumber;
};

#endif // CHESSPOSITION_H
