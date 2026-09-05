/*******************************************************************************
 * gui/ChessBoard.h                                                           *
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

#ifndef CHESSBOARD_H
#define CHESSBOARD_H

#include <QPoint>
#include <QWidget>

#include <functional>

class ChessBoard : public QWidget
{
	Q_OBJECT

public:
	explicit ChessBoard(QWidget *parent = nullptr);

	void registerSquare(QWidget *square, int squareNumber);
	void setSelectedSquare(int square);
	void notifyMoveProduced(int fromSquare, int toSquare, char promotion);
	void setStateHandlers(
	        std::function<QString()> saveHandler,
	        std::function<bool(const QString &, QString *)> loadHandler);

	QString saveState() const;
	QString positionHash() const;
	bool loadState(const QString &fen, QString *error = nullptr);

signals:
	void squareActivated(int square);
	void moveProduced(int fromSquare, int toSquare, char promotion);
	void stateLoaded(const QString &fen, const QString &hash);
	void stateLoadRejected(const QString &fen, const QString &reason);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	int squareNumber(QObject *object) const;

	QPoint m_dragStartPosition;
	int m_selectedSquare;
	std::function<QString()> m_saveHandler;
	std::function<bool(const QString &, QString *)> m_loadHandler;
};

#endif // CHESSBOARD_H
