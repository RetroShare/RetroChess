/*******************************************************************************
 * gui/RetroChessFlair.cpp                                                     *
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

#include "RetroChessFlair.h"

#include <QApplication>
#include <QCoreApplication>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QStyle>
#include <QtGlobal>

namespace {

struct FlairEntry
{
	const char *id;
	const char *category;
	const char *name;
};

// Wire ids: never rename or reuse one, peers store and send them.
// Icons: gui/flair/<id>.png (64x64). See gui/flair/LICENSE.txt.
const FlairEntry kFlairs[] = {
	{"smile", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Smile")},
	{"frown", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Frown")},
	{"angry", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Angry")},
	{"hushed", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Hushed")},
	{"worried", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Worried")},
	{"smirk", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Smirk")},
	{"disappointed", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Disappointed")},
	{"joy", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Tears of joy")},
	{"yum", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Yum")},
	{"confused", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Confused")},
	{"grin", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Grin")},
	{"grimace", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Grimace")},
	{"laugh", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Laugh")},
	{"devil-smile", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Devilish")},
	{"grinning", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Grinning")},
	{"rage", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Rage")},
	{"blush", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Blush")},
	{"fearful", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Fearful")},
	{"dizzy", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Dizzy")},
	{"sleeping", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Sleeping")},
	{"tongue", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Tongue")},
	{"sweat-smile", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Sweat smile")},
	{"sob", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Sob")},
	{"scream", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Scream")},
	{"relieved", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Relieved")},
	{"heart-eyes", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Heart eyes")},
	{"nerd", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Nerd")},
	{"cool", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Cool")},
	{"neutral", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Neutral")},
	{"thinking", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Thinking")},
	{"party", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Party face")},
	{"mind-blown", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Mind blown")},
	{"cold", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Cold face")},
	{"nauseated", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Nauseated")},
	{"kiss", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Kiss")},
	{"imp", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Imp")},
	{"clap", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Clap")},
	{"thumbs-up", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Thumbs up")},
	{"thumbs-down", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Thumbs down")},
	{"fist", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Fist bump")},
	{"raised-hands", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Raised hands")},
	{"muscle", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Muscle")},
	{"wave", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Wave")},
	{"rainbow", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Rainbow")},
	{"fire", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Fire")},
	{"hundred", "Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Hundred")},
	{"piece-wk", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "White king")},
	{"piece-wq", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "White queen")},
	{"piece-wr", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "White rook")},
	{"piece-wb", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "White bishop")},
	{"piece-wn", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "White knight")},
	{"piece-wp", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "White pawn")},
	{"piece-bk", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Black king")},
	{"piece-bq", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Black queen")},
	{"piece-br", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Black rook")},
	{"piece-bb", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Black bishop")},
	{"piece-bn", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Black knight")},
	{"piece-bp", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Black pawn")},
	{"handshake", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Handshake (draw)")},
	{"white-flag", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "White flag")},
	{"trophy", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Trophy")},
	{"swords", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Crossed swords")},
	{"lightning", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Lightning (blitz)")},
	{"stopwatch", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Stopwatch")},
	{"hourglass", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Hourglass")},
	{"target", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Target")},
	{"brain", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Brain")},
	{"fish", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Fish")},
	{"crown", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Crown")},
	{"castle", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Castle")},
	{"horse", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Horse")},
	{"gold", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Gold medal")},
	{"silver", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Silver medal")},
	{"bronze", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Bronze medal")},
	{"grad-cap", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Graduation cap")},
	{"books", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Books")},
	{"puzzle", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Puzzle")},
	{"rocket", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Rocket")},
	{"gem", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Gem")},
	{"shield", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Shield")},
	{"dice", "Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Dice")},
	{"owl", "Animals", QT_TRANSLATE_NOOP("RetroChessFlair", "Owl")},
	{"dragon", "Animals", QT_TRANSLATE_NOOP("RetroChessFlair", "Dragon")},
	{"lion", "Animals", QT_TRANSLATE_NOOP("RetroChessFlair", "Lion")},
	{"wolf", "Animals", QT_TRANSLATE_NOOP("RetroChessFlair", "Wolf")},
	{"fox", "Animals", QT_TRANSLATE_NOOP("RetroChessFlair", "Fox")},
	{"turtle", "Animals", QT_TRANSLATE_NOOP("RetroChessFlair", "Turtle")},
	{"octopus", "Animals", QT_TRANSLATE_NOOP("RetroChessFlair", "Octopus")},
	{"unicorn", "Animals", QT_TRANSLATE_NOOP("RetroChessFlair", "Unicorn")},
	{"cat", "Animals", QT_TRANSLATE_NOOP("RetroChessFlair", "Cat")},
	{"dog", "Animals", QT_TRANSLATE_NOOP("RetroChessFlair", "Dog")},
	{"penguin", "Animals", QT_TRANSLATE_NOOP("RetroChessFlair", "Penguin")},
	{"panda", "Animals", QT_TRANSLATE_NOOP("RetroChessFlair", "Panda")},
	{"cactus", "Nature", QT_TRANSLATE_NOOP("RetroChessFlair", "Cactus")},
	{"clover", "Nature", QT_TRANSLATE_NOOP("RetroChessFlair", "Four-leaf clover")},
	{"sunflower", "Nature", QT_TRANSLATE_NOOP("RetroChessFlair", "Sunflower")},
	{"moon", "Nature", QT_TRANSLATE_NOOP("RetroChessFlair", "Moon")},
	{"star", "Nature", QT_TRANSLATE_NOOP("RetroChessFlair", "Star")},
	{"sun", "Nature", QT_TRANSLATE_NOOP("RetroChessFlair", "Sun")},
	{"snowflake", "Nature", QT_TRANSLATE_NOOP("RetroChessFlair", "Snowflake")},
	{"globe", "Nature", QT_TRANSLATE_NOOP("RetroChessFlair", "Globe")},
	{"coffee", "Fun", QT_TRANSLATE_NOOP("RetroChessFlair", "Coffee")},
	{"pizza", "Fun", QT_TRANSLATE_NOOP("RetroChessFlair", "Pizza")},
	{"game", "Fun", QT_TRANSLATE_NOOP("RetroChessFlair", "Video game")},
	{"guitar", "Fun", QT_TRANSLATE_NOOP("RetroChessFlair", "Guitar")},
	{"robot", "Fun", QT_TRANSLATE_NOOP("RetroChessFlair", "Robot")},
	{"alien", "Fun", QT_TRANSLATE_NOOP("RetroChessFlair", "Alien")},
	{"ghost", "Fun", QT_TRANSLATE_NOOP("RetroChessFlair", "Ghost")},
	{"ninja", "Fun", QT_TRANSLATE_NOOP("RetroChessFlair", "Ninja")},
	{"skull", "Fun", QT_TRANSLATE_NOOP("RetroChessFlair", "Skull")},
	{"cake", "Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Birthday cake")},
	{"popper", "Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Party popper")},
	{"cheers", "Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Cheers")},
	{"gift-heart", "Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Heart gift")},
	{"heart", "Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Heart")},
	{"love-letter", "Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Love letter")},
	{"shamrock", "Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Shamrock")},
	{"palm", "Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Palm tree")},
	{"fireworks", "Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Fireworks")},
	{"pumpkin", "Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Jack-o-lantern")},
	{"maple", "Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Maple leaf")},
	{"turkey", "Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Turkey")},
	{"gift", "Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Gift")},
	{"menorah", "Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Menorah")},
	{"tree", "Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Christmas tree")},
	{"santa", "Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Santa")},
};

const char *const kCategoryTitles[][2] = {
	{"Emoji", QT_TRANSLATE_NOOP("RetroChessFlair", "Emoji")},
	{"Chess", QT_TRANSLATE_NOOP("RetroChessFlair", "Chess")},
	{"Animals", QT_TRANSLATE_NOOP("RetroChessFlair", "Animals")},
	{"Nature", QT_TRANSLATE_NOOP("RetroChessFlair", "Nature")},
	{"Fun", QT_TRANSLATE_NOOP("RetroChessFlair", "Fun")},
	{"Holiday", QT_TRANSLATE_NOOP("RetroChessFlair", "Holiday")},
};

QString tr(const char *text)
{
	return QCoreApplication::translate("RetroChessFlair", text);
}

const QHash<QString, int> &flairIndex()
{
	static const QHash<QString, int> index = []() {
		QHash<QString, int> map;
		const QVector<RetroChessFlairInfo> &flairs = RetroChessFlair::all();
		for (int i = 0; i < flairs.size(); ++i) map.insert(flairs.at(i).id, i);
		return map;
	}();
	return index;
}

} // namespace

const QVector<RetroChessFlairInfo> &RetroChessFlair::all()
{
	static const QVector<RetroChessFlairInfo> flairs = []() {
		QVector<RetroChessFlairInfo> list;
		for (const FlairEntry &entry : kFlairs)
			list.append({QString::fromLatin1(entry.id), QString::fromLatin1(entry.category), tr(entry.name)});
		return list;
	}();
	return flairs;
}

QStringList RetroChessFlair::categories()
{
	QStringList list;
	for (const auto &title : kCategoryTitles) list << QString::fromLatin1(title[0]);
	return list;
}

QString RetroChessFlair::categoryTitle(const QString &category)
{
	for (const auto &title : kCategoryTitles)
		if (category == QLatin1String(title[0])) return tr(title[1]);
	return category;
}

bool RetroChessFlair::isValid(const QString &id)
{
	return !id.isEmpty() && id.size() <= kMaxIdLength && flairIndex().contains(id);
}

QString RetroChessFlair::normalize(const QString &id)
{
	return isValid(id) ? id : QString();
}

QString RetroChessFlair::displayName(const QString &id)
{
	const auto it = flairIndex().constFind(id);
	return it == flairIndex().constEnd() ? QString() : all().at(it.value()).name;
}

QString RetroChessFlair::resource(const QString &id)
{
	return isValid(id) ? QStringLiteral(":/flair/%1.png").arg(id) : QString();
}

int RetroChessFlair::itemSize(int textHeight, int rowHeight)
{
	// Slightly bigger than the text (lists are dense), but inside the row.
	int size = qMax(16, qRound(textHeight * 1.15));
	if (rowHeight > 0) size = qMin(size, rowHeight - 2);
	return qMax(12, size);
}

QPixmap RetroChessFlair::pixmap(const QString &id, int size, qreal devicePixelRatio)
{
	if (!isValid(id) || size <= 0) return QPixmap();
	if (devicePixelRatio <= 0) devicePixelRatio = 1.0;
	static QHash<QString, QPixmap> cache;
	const QString key = QStringLiteral("%1/%2/%3").arg(id).arg(size).arg(devicePixelRatio);
	auto it = cache.constFind(key);
	if (it != cache.constEnd()) return it.value();
	const int physical = qRound(size * devicePixelRatio);
	// Halve in steps first: one big smooth downscale (64 -> 18 px) looks jagged.
	QImage image(resource(id));
	image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
	while (image.width() / 2 >= physical)
		image = image.scaled(image.width() / 2, image.height() / 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	QPixmap pix = QPixmap::fromImage(image.scaled(physical, physical, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	pix.setDevicePixelRatio(devicePixelRatio);
	cache.insert(key, pix);
	return pix;
}

void RetroChessFlairDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	const QString flair = RetroChessFlair::normalize(index.data(RetroChessFlair::kFlairRole).toString());
	if (flair.isEmpty()) {
		QStyledItemDelegate::paint(painter, option, index);
		return;
	}
	QStyleOptionViewItem opt(option);
	initStyleOption(&opt, index);
	const QWidget *widget = opt.widget;
	QStyle *style = widget ? widget->style() : QApplication::style();

	const int flairSize = RetroChessFlair::itemSize(opt.fontMetrics.height(), option.rect.height());
	const int gap = 4;
	const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, widget) + 1;
	const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);
	const int available = qMax(0, textRect.width() - 2 * textMargin - flairSize - gap);
	opt.text = opt.fontMetrics.elidedText(opt.text, opt.textElideMode, available);
	const int textWidth = opt.fontMetrics.horizontalAdvance(opt.text);
	style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

	const int x = opt.direction == Qt::RightToLeft
	        ? textRect.right() - textMargin - textWidth - gap - flairSize + 1
	        : textRect.left() + textMargin + textWidth + gap;
	const int y = textRect.top() + (textRect.height() - flairSize) / 2;
	const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
	painter->save();
	painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
	painter->setClipRect(option.rect);
	painter->drawPixmap(QRect(x, y, flairSize, flairSize), RetroChessFlair::pixmap(flair, flairSize, dpr));
	painter->restore();
}

QSize RetroChessFlairDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QSize size = QStyledItemDelegate::sizeHint(option, index);
	if (RetroChessFlair::isValid(index.data(RetroChessFlair::kFlairRole).toString()))
		size.rwidth() += RetroChessFlair::itemSize(option.fontMetrics.height(), size.height()) + 4;
	return size;
}
