/*******************************************************************************
 * gui/ChessPosition.cpp                                                      *
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

#include "ChessPosition.h"

#include <QCryptographicHash>
#include <QObject>
#include <QStringList>

#include <cctype>
#include <cstring>

namespace
{
bool isWhitePiece(char piece) { return piece >= 'A' && piece <= 'Z'; }
bool sameColour(char first, char second)
{
	return first && second && (isWhitePiece(first) == isWhitePiece(second));
}
int rowOf(int square) { return square / 8; }
int colOf(int square) { return square % 8; }
QString squareName(int square)
{
	if (square < 0 || square >= 64) return "-";
	return QString(QChar('a' + colOf(square))) + QChar('8' - rowOf(square));
}
int squareFromName(const QString &name)
{
	if (name.size() != 2 || name[0] < 'a' || name[0] > 'h'
	        || name[1] < '1' || name[1] > '8') return -1;
	return ('8' - name[1].toLatin1()) * 8 + name[0].toLatin1() - 'a';
}
}

QString ChessMove::uci() const
{
	QString text = squareName(from) + squareName(to);
	if (promotion) text += QChar(promotion).toLower();
	return text;
}

ChessMove ChessMove::fromUci(const QString &text)
{
	ChessMove move;
	if (text.size() < 4) return move;
	move.from = squareFromName(text.mid(0, 2).toLower());
	move.to = squareFromName(text.mid(2, 2).toLower());
	if (text.size() >= 5) move.promotion = text[4].toLower().toLatin1();
	return move;
}

ChessPosition::ChessPosition() { reset(); }

void ChessPosition::reset()
{
	loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void ChessPosition::setError(QString *error, const QString &message) const
{
	if (error) *error = message;
}

bool ChessPosition::loadFen(const QString &fen, QString *error)
{
	const QStringList fields = fen.simplified().split(' ');
	if (fields.size() != 6) {
		setError(error, QObject::tr("FEN must contain six fields"));
		return false;
	}
	char candidate[64] = {};
	const QStringList ranks = fields[0].split('/');
	if (ranks.size() != 8) {
		setError(error, QObject::tr("FEN must contain eight ranks"));
		return false;
	}
	int whiteKings = 0, blackKings = 0;
	for (int row = 0; row < 8; ++row) {
		int column = 0;
		for (QChar token : ranks[row]) {
			if (token.isDigit()) column += token.digitValue();
			else {
				const char piece = token.toLatin1();
				if (!std::strchr("prnbqkPRNBQK", piece) || column >= 8) {
					setError(error, QObject::tr("FEN contains an invalid piece or rank"));
					return false;
				}
				candidate[row * 8 + column++] = piece;
				if (piece == 'K') ++whiteKings;
				if (piece == 'k') ++blackKings;
			}
			if (column > 8) break;
		}
		if (column != 8) {
			setError(error, QObject::tr("Every FEN rank must contain eight squares"));
			return false;
		}
	}
	if (whiteKings != 1 || blackKings != 1) {
		setError(error, QObject::tr("FEN must contain one king of each colour"));
		return false;
	}
	if (fields[1] != "w" && fields[1] != "b") {
		setError(error, QObject::tr("Invalid side-to-move field"));
		return false;
	}
	for (QChar right : fields[2])
		if (right != '-' && !QString("KQkq").contains(right)) {
			setError(error, QObject::tr("Invalid castling rights"));
			return false;
		}
	const int enPassant = fields[3] == "-" ? -1 : squareFromName(fields[3]);
	if (fields[3] != "-" && enPassant < 0) {
		setError(error, QObject::tr("Invalid en-passant square"));
		return false;
	}
	bool halfOk = false, fullOk = false;
	const int halfmove = fields[4].toInt(&halfOk);
	const int fullmove = fields[5].toInt(&fullOk);
	if (!halfOk || !fullOk || halfmove < 0 || fullmove < 1) {
		setError(error, QObject::tr("Invalid FEN move counters"));
		return false;
	}
	std::memcpy(m_board, candidate, sizeof(m_board));
	m_whiteToMove = fields[1] == "w";
	m_whiteKingSide = fields[2].contains('K');
	m_whiteQueenSide = fields[2].contains('Q');
	m_blackKingSide = fields[2].contains('k');
	m_blackQueenSide = fields[2].contains('q');
	m_enPassant = enPassant;
	m_halfmoveClock = halfmove;
	m_fullmoveNumber = fullmove;
	return true;
}

QString ChessPosition::fen() const
{
	QString board;
	for (int row = 0; row < 8; ++row) {
		if (row) board += '/';
		int empty = 0;
		for (int column = 0; column < 8; ++column) {
			const char piece = m_board[row * 8 + column];
			if (!piece) ++empty;
			else {
				if (empty) board += QString::number(empty);
				empty = 0;
				board += QChar(piece);
			}
		}
		if (empty) board += QString::number(empty);
	}
	QString rights;
	if (m_whiteKingSide) rights += 'K';
	if (m_whiteQueenSide) rights += 'Q';
	if (m_blackKingSide) rights += 'k';
	if (m_blackQueenSide) rights += 'q';
	if (rights.isEmpty()) rights = "-";
	return QString("%1 %2 %3 %4 %5 %6").arg(
	        board, m_whiteToMove ? "w" : "b", rights,
	        squareName(m_enPassant), QString::number(m_halfmoveClock),
	        QString::number(m_fullmoveNumber));
}

QString ChessPosition::hash() const
{
	return QString::fromLatin1(QCryptographicHash::hash(
	        fen().toUtf8(), QCryptographicHash::Sha256).toHex());
}

char ChessPosition::pieceAt(int square) const
{
	return square >= 0 && square < 64 ? m_board[square] : 0;
}

bool ChessPosition::clearPath(int from, int to) const
{
	const int rowStep = (rowOf(to) > rowOf(from)) - (rowOf(to) < rowOf(from));
	const int colStep = (colOf(to) > colOf(from)) - (colOf(to) < colOf(from));
	int row = rowOf(from) + rowStep, column = colOf(from) + colStep;
	while (row != rowOf(to) || column != colOf(to)) {
		if (m_board[row * 8 + column]) return false;
		row += rowStep;
		column += colStep;
	}
	return true;
}

bool ChessPosition::squareAttacked(int square, bool byWhite) const
{
	for (int from = 0; from < 64; ++from) {
		const char piece = m_board[from];
		if (!piece || isWhitePiece(piece) != byWhite) continue;
		const int dr = rowOf(square) - rowOf(from);
		const int dc = colOf(square) - colOf(from);
		switch (std::tolower(static_cast<unsigned char>(piece))) {
		case 'p': if (dr == (byWhite ? -1 : 1) && qAbs(dc) == 1) return true; break;
		case 'n': if ((qAbs(dr) == 2 && qAbs(dc) == 1)
		                       || (qAbs(dr) == 1 && qAbs(dc) == 2)) return true; break;
		case 'b': if (qAbs(dr) == qAbs(dc) && clearPath(from, square)) return true; break;
		case 'r': if ((!dr || !dc) && clearPath(from, square)) return true; break;
		case 'q': if ((!dr || !dc || qAbs(dr) == qAbs(dc))
		                       && clearPath(from, square)) return true; break;
		case 'k': if (qMax(qAbs(dr), qAbs(dc)) == 1) return true; break;
		}
	}
	return false;
}

bool ChessPosition::isInCheck(bool white) const
{
	const char king = white ? 'K' : 'k';
	for (int square = 0; square < 64; ++square)
		if (m_board[square] == king) return squareAttacked(square, !white);
	return true;
}

bool ChessPosition::isPseudoLegal(const ChessMove &move, QString *error) const
{
	if (move.from < 0 || move.from >= 64 || move.to < 0 || move.to >= 64
	        || move.from == move.to) {
		setError(error, QObject::tr("Move contains an invalid square")); return false;
	}
	const char piece = m_board[move.from], target = m_board[move.to];
	if (!piece) { setError(error, QObject::tr("Source square is empty")); return false; }
	const bool white = isWhitePiece(piece);
	if (white != m_whiteToMove) { setError(error, QObject::tr("Wrong side to move")); return false; }
	if (sameColour(piece, target)) { setError(error, QObject::tr("Destination contains a friendly piece")); return false; }
	const int dr = rowOf(move.to) - rowOf(move.from);
	const int dc = colOf(move.to) - colOf(move.from);
	bool valid = false;
	switch (std::tolower(static_cast<unsigned char>(piece))) {
	case 'p': {
		const int direction = white ? -1 : 1;
		const int startRow = white ? 6 : 1;
		valid = !dc && !target && (dr == direction
		        || (rowOf(move.from) == startRow && dr == 2 * direction
		            && !m_board[move.from + direction * 8]));
		if (qAbs(dc) == 1 && dr == direction)
			valid = target || (move.to == m_enPassant
			        && m_board[move.to + (white ? 8 : -8)]
			           == (white ? 'p' : 'P'));
		const bool promoting = rowOf(move.to) == (white ? 0 : 7);
		if (promoting && !QString("qrbn").contains(QChar(move.promotion).toLower())) valid = false;
		if (!promoting && move.promotion) valid = false;
		break;
	}
	case 'n': valid = (qAbs(dr) == 2 && qAbs(dc) == 1)
	                       || (qAbs(dr) == 1 && qAbs(dc) == 2); break;
	case 'b': valid = qAbs(dr) == qAbs(dc) && clearPath(move.from, move.to); break;
	case 'r': valid = (!dr || !dc) && clearPath(move.from, move.to); break;
	case 'q': valid = (!dr || !dc || qAbs(dr) == qAbs(dc)) && clearPath(move.from, move.to); break;
	case 'k':
		valid = qMax(qAbs(dr), qAbs(dc)) == 1;
		if (!dr && qAbs(dc) == 2) {
			const bool kingSide = dc > 0;
			const bool right = white
			        ? (kingSide ? m_whiteKingSide : m_whiteQueenSide)
			        : (kingSide ? m_blackKingSide : m_blackQueenSide);
			const int expectedFrom = white ? 60 : 4;
			const int rookSquare = white ? (kingSide ? 63 : 56) : (kingSide ? 7 : 0);
			const int through = move.from + (kingSide ? 1 : -1);
			valid = right && move.from == expectedFrom
			        && m_board[rookSquare] == (white ? 'R' : 'r')
			        && clearPath(move.from, rookSquare)
			        && !isInCheck(white) && !squareAttacked(through, !white)
			        && !squareAttacked(move.to, !white);
		}
		break;
	}
	if (!valid) setError(error, QObject::tr("Piece cannot legally reach the destination"));
	return valid;
}

void ChessPosition::applyUnchecked(const ChessMove &move)
{
	const char piece = m_board[move.from];
	const bool white = isWhitePiece(piece);
	const char captured = m_board[move.to];
	const int previousEnPassant = m_enPassant;
	if (std::tolower(static_cast<unsigned char>(piece)) == 'p'
	        && move.to == previousEnPassant && !captured)
		m_board[move.to + (white ? 8 : -8)] = 0;
	m_board[move.to] = piece;
	m_board[move.from] = 0;
	if (std::tolower(static_cast<unsigned char>(piece)) == 'p'
	        && (rowOf(move.to) == 0 || rowOf(move.to) == 7))
		m_board[move.to] = white ? std::toupper(move.promotion) : std::tolower(move.promotion);
	if (std::tolower(static_cast<unsigned char>(piece)) == 'k' && qAbs(colOf(move.to) - colOf(move.from)) == 2) {
		const bool kingSide = colOf(move.to) > colOf(move.from);
		const int rookFrom = rowOf(move.from) * 8 + (kingSide ? 7 : 0);
		const int rookTo = move.from + (kingSide ? 1 : -1);
		m_board[rookTo] = m_board[rookFrom]; m_board[rookFrom] = 0;
	}
	if (piece == 'K') m_whiteKingSide = m_whiteQueenSide = false;
	if (piece == 'k') m_blackKingSide = m_blackQueenSide = false;
	if (move.from == 63 || move.to == 63) m_whiteKingSide = false;
	if (move.from == 56 || move.to == 56) m_whiteQueenSide = false;
	if (move.from == 7 || move.to == 7) m_blackKingSide = false;
	if (move.from == 0 || move.to == 0) m_blackQueenSide = false;
	m_enPassant = -1;
	if (std::tolower(static_cast<unsigned char>(piece)) == 'p'
	        && qAbs(rowOf(move.to) - rowOf(move.from)) == 2)
		m_enPassant = (move.from + move.to) / 2;
	m_halfmoveClock = (std::tolower(static_cast<unsigned char>(piece)) == 'p' || captured)
	        ? 0 : m_halfmoveClock + 1;
	if (!m_whiteToMove) ++m_fullmoveNumber;
	m_whiteToMove = !m_whiteToMove;
}

bool ChessPosition::isLegalMove(const ChessMove &move, QString *error) const
{
	if (!isPseudoLegal(move, error)) return false;
	ChessPosition copy = *this;
	const bool movingWhite = copy.m_whiteToMove;
	copy.applyUnchecked(move);
	if (copy.isInCheck(movingWhite)) {
		setError(error, QObject::tr("Move leaves the king in check"));
		return false;
	}
	return true;
}

bool ChessPosition::applyMove(const ChessMove &move, QString *error)
{
	if (!isLegalMove(move, error)) return false;
	applyUnchecked(move);
	return true;
}

QVector<ChessMove> ChessPosition::legalMovesFrom(int square) const
{
	QVector<ChessMove> result;
	for (int to = 0; to < 64; ++to) {
		ChessMove move{square, to, 0};
		const char piece = pieceAt(square);
		if (piece && std::tolower(static_cast<unsigned char>(piece)) == 'p'
		        && rowOf(to) == (isWhitePiece(piece) ? 0 : 7)) {
			for (char promotion : {'q', 'r', 'b', 'n'}) {
				move.promotion = promotion;
				if (isLegalMove(move)) result.append(move);
			}
		} else if (isLegalMove(move)) result.append(move);
	}
	return result;
}

QVector<ChessMove> ChessPosition::legalMoves() const
{
	QVector<ChessMove> result;
	for (int square = 0; square < 64; ++square)
		result += legalMovesFrom(square);
	return result;
}
