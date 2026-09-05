/*******************************************************************************
 * gui/ChessBoard.cpp                                                         *
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

#include "ChessBoard.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QLabel>

#include <utility>

namespace
{
const char *CHESS_TILE_MIME = "application/x-retrochess-tile";
}

ChessBoard::ChessBoard(QWidget *parent)
    : QWidget(parent), m_selectedSquare(-1)
{
	setAcceptDrops(true);
}

void ChessBoard::registerSquare(QWidget *square, int squareNumber)
{
	if (!square) return;
	square->setProperty("retroChessSquare", squareNumber);
	square->installEventFilter(this);
	square->setAcceptDrops(true);
}

void ChessBoard::setSelectedSquare(int square)
{
	m_selectedSquare = square >= 0 && square < 64 ? square : -1;
}

void ChessBoard::notifyMoveProduced(int fromSquare, int toSquare, char promotion)
{
	m_selectedSquare = -1;
	emit moveProduced(fromSquare, toSquare, promotion);
}

void ChessBoard::setStateHandlers(
		std::function<QString()> saveHandler,
		std::function<bool(const QString &, QString *)> loadHandler)
{
	m_saveHandler = std::move(saveHandler);
	m_loadHandler = std::move(loadHandler);
}

QString ChessBoard::saveState() const
{
	return m_saveHandler ? m_saveHandler() : QString();
}

QString ChessBoard::positionHash() const
{
	return QString::fromLatin1(QCryptographicHash::hash(
	        saveState().toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool ChessBoard::loadState(const QString &fen, QString *error)
{
	QString localError;
	QString *targetError = error ? error : &localError;
	if (!m_loadHandler || !m_loadHandler(fen, targetError)) {
		emit stateLoadRejected(fen, *targetError);
		return false;
	}
	emit stateLoaded(saveState(), positionHash());
	return true;
}

int ChessBoard::squareNumber(QObject *object) const
{
	return object ? object->property("retroChessSquare").toInt() : -1;
}

bool ChessBoard::eventFilter(QObject *watched, QEvent *event)
{
	QWidget *square = qobject_cast<QWidget *>(watched);
	if (!square || !square->property("retroChessSquare").isValid())
		return QWidget::eventFilter(watched, event);
	const int tileNumber = squareNumber(square);

	if (event->type() == QEvent::MouseButtonPress) {
		QMouseEvent *mouse = static_cast<QMouseEvent *>(event);
		if (mouse->button() != Qt::LeftButton) return false;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		m_dragStartPosition = mouse->position().toPoint();
#else
		m_dragStartPosition = mouse->pos();
#endif
		emit squareActivated(tileNumber);
		return true;
	}

	if (event->type() == QEvent::MouseMove) {
		QMouseEvent *mouse = static_cast<QMouseEvent *>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		const QPoint position = mouse->position().toPoint();
#else
		const QPoint position = mouse->pos();
#endif
		if (!(mouse->buttons() & Qt::LeftButton)
		        || tileNumber != m_selectedSquare
		        || (position - m_dragStartPosition).manhattanLength()
		           < QApplication::startDragDistance()) return true;
		QDrag *drag = new QDrag(square);
		QMimeData *mime = new QMimeData;
		mime->setData(CHESS_TILE_MIME, QByteArray::number(tileNumber));
		drag->setMimeData(mime);
		QLabel *squareLabel = qobject_cast<QLabel *>(square);
		QPixmap displayedPiece;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		if (squareLabel) displayedPiece = squareLabel->pixmap();
#else
		if (squareLabel) displayedPiece = squareLabel->pixmap(Qt::ReturnByValue);
#endif
		if (!displayedPiece.isNull()) {
			drag->setPixmap(displayedPiece);
			drag->setHotSpot(position);
		}
		drag->exec(Qt::MoveAction);
		return true;
	}

	if (event->type() == QEvent::DragEnter) {
		QDragEnterEvent *drag = static_cast<QDragEnterEvent *>(event);
		if (drag->mimeData()->hasFormat(CHESS_TILE_MIME)
		        && drag->mimeData()->data(CHESS_TILE_MIME).toInt() == m_selectedSquare)
			drag->acceptProposedAction();
		else drag->ignore();
		return true;
	}

	if (event->type() == QEvent::Drop) {
		QDropEvent *drop = static_cast<QDropEvent *>(event);
		if (!drop->mimeData()->hasFormat(CHESS_TILE_MIME)
		        || drop->mimeData()->data(CHESS_TILE_MIME).toInt() != m_selectedSquare) {
			drop->ignore();
			return true;
		}
		drop->acceptProposedAction();
		emit squareActivated(tileNumber);
		return true;
	}

	return QWidget::eventFilter(watched, event);
}
