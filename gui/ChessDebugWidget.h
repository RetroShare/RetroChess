/*******************************************************************************
 * gui/ChessDebugWidget.h                                                      *
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

#ifndef CHESSDEBUGWIDGET_H
#define CHESSDEBUGWIDGET_H

#include <QDialog>
#include <QStringList>
#include <functional>

class QLabel;
class QLineEdit;
class QTextEdit;

class ChessDebugWidget : public QDialog
{
public:
	using FenProvider = std::function<QString()>;

	explicit ChessDebugWidget(
	        const QString &gameDescription, FenProvider fenProvider,
	        QWidget *parent = nullptr);
	void appendEvent(const QString &event);
	void refresh();
	QString report() const;
	static QString hashFen(const QString &fen);

private:
	QString m_gameDescription;
	FenProvider m_fenProvider;
	QStringList m_events;
	QLineEdit *m_fenEdit;
	QLabel *m_hashLabel;
	QTextEdit *m_logEdit;
};

#endif // CHESSDEBUGWIDGET_H
