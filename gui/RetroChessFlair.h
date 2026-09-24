/*******************************************************************************
 * gui/RetroChessFlair.h                                                       *
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

// Flair: one small icon a player shows after their nickname (like chess.com).
//
// Only ids from the built-in catalogue below are ever displayed. A flair
// received from the network is checked with RetroChessFlair::normalize(), so a
// peer cannot inject text, HTML or unknown images into our labels.

#ifndef RETROCHESSFLAIR_H
#define RETROCHESSFLAIR_H

#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QVector>

struct RetroChessFlairInfo
{
	QString id;
	QString category;
	QString name; // translated display name
};

class RetroChessFlair
{
public:
	/// Longest id accepted from the network.
	static const int kMaxIdLength = 32;
	/// Item data role holding a flair id, read by RetroChessFlairDelegate.
	static const int kFlairRole = Qt::UserRole + 20;

	static const QVector<RetroChessFlairInfo> &all();
	/// Category names in display order (untranslated keys).
	static QStringList categories();
	static QString categoryTitle(const QString &category);
	static bool isValid(const QString &id);
	/// Returns id when it is a known flair, otherwise an empty string.
	static QString normalize(const QString &id);
	static QString displayName(const QString &id);
	/// Qt resource path of the icon (":/flair/<id>.png"), empty if unknown.
	static QString resource(const QString &id);
	/// Flair size for an item view row: ~1.15x the text height, capped to the row.
	static int itemSize(int textHeight, int rowHeight);
	/// Scaled icon, cached. Honours devicePixelRatio for HiDPI screens.
	static QPixmap pixmap(const QString &id, int size, qreal devicePixelRatio = 1.0);
};

/// Paints the flair (kFlairRole) right after the text of an item view cell.
/// The name is elided first so the flair always stays visible.
class RetroChessFlairDelegate : public QStyledItemDelegate
{
public:
	explicit RetroChessFlairDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif
