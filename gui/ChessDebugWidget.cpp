/*******************************************************************************
 * gui/ChessDebugWidget.cpp                                                    *
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

#include "ChessDebugWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>
#include <utility>

ChessDebugWidget::ChessDebugWidget(
        const QString &gameDescription, FenProvider fenProvider, QWidget *parent)
    : QDialog(parent), m_gameDescription(gameDescription),
      m_fenProvider(std::move(fenProvider)),
      m_fenEdit(new QLineEdit(this)), m_hashLabel(new QLabel(this)),
      m_logEdit(new QTextEdit(this))
{
	setWindowTitle(tr("RetroChess Debug"));
	resize(760, 520);
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addWidget(new QLabel(tr("FEN position"), this));
	m_fenEdit->setReadOnly(true);
	layout->addWidget(m_fenEdit);
	layout->addWidget(m_hashLabel);

	QHBoxLayout *buttons = new QHBoxLayout;
	QPushButton *copyFen = new QPushButton(tr("Copy FEN"), this);
	QPushButton *copy = new QPushButton(tr("Copy report"), this);
	QPushButton *save = new QPushButton(tr("Save report"), this);
	buttons->addWidget(copyFen);
	buttons->addWidget(copy);
	buttons->addWidget(save);
	buttons->addStretch();
	layout->addLayout(buttons);

	m_logEdit->setReadOnly(true);
	m_logEdit->setLineWrapMode(QTextEdit::NoWrap);
	layout->addWidget(m_logEdit, 1);

	connect(copyFen, &QPushButton::clicked, this, [this]() {
		QApplication::clipboard()->setText(m_fenProvider());
	});
	connect(copy, &QPushButton::clicked, this, [this]() {
		QApplication::clipboard()->setText(report());
	});
	connect(save, &QPushButton::clicked, this, [this]() {
		const QString path = QFileDialog::getSaveFileName(
		        this, tr("Save chess debug report"), "retrochess-debug.txt",
		        tr("Text files (*.txt)"));
		if (path.isEmpty()) return;
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			QMessageBox::warning(this, tr("Save failed"), file.errorString());
			return;
		}
		QTextStream(&file) << report();
	});
}

QString ChessDebugWidget::hashFen(const QString &fen)
{
	return QString::fromLatin1(QCryptographicHash::hash(
	        fen.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
}

void ChessDebugWidget::appendEvent(const QString &event)
{
	const QString entry = QDateTime::currentDateTime().toString(
	        "yyyy-MM-dd HH:mm:ss.zzz ") + event;
	m_events.append(entry);
	if (m_events.size() > 1000) m_events.removeFirst();
	m_logEdit->append(entry.toHtmlEscaped());
	m_logEdit->verticalScrollBar()->setValue(m_logEdit->verticalScrollBar()->maximum());
	refresh();
}

void ChessDebugWidget::refresh()
{
	const QString fen = m_fenProvider();
	if (!m_fenEdit->hasFocus()) m_fenEdit->setText(fen);
	m_hashLabel->setText(tr("Position hash: %1").arg(hashFen(fen)));
}

QString ChessDebugWidget::report() const
{
	const QString fen = m_fenProvider();
	return QString("RetroChess debug report\nGame: %1\nFEN: %2\nPosition hash: %3\n\nEvents:\n%4\n")
	        .arg(m_gameDescription, fen, hashFen(fen), m_events.join('\n'));
}
