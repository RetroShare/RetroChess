/*******************************************************************************
 * gui/BoardTheme.cpp                                                          *
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

#include "BoardTheme.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPolygonF>

namespace {
QString t(const char *text)
{
    return QCoreApplication::translate("BoardTheme", text);
}

int currentIndex = 0;
bool backgroundEnabled = false;
}

const QList<BoardTheme> &BoardTheme::all()
{
    // id, name, felt, frame, frameLine, tray, pointLight, pointDark, white, black, highlight,
    // background (wooden boards on green, like a card table)
    static const QList<BoardTheme> themes = {
        { "wood", t("Light wood"),
          QColor("#d8b886"), QColor("#e8d3a8"), QColor("#b89565"), QColor("#dcc49b"),
          QColor("#f1deb6"), QColor("#734023"), QColor("#ece5d3"), QColor("#2e2e2e"), QColor("#e8412c"), QColor("#0f5a14") },
        { "retroshare", t("RetroShare blue"),
          QColor("#eaf6fc"), QColor("#cfe7f4"), QColor("#7fbfe0"), QColor("#bfe0f1"),
          QColor("#9fd3ec"), QColor("#039bd5"), QColor("#ffffff"), QColor("#2b2b2b"), QColor("#f5a623"), QColor("#1f5f86") },
        { "walnut", t("Walnut"),
          QColor("#6b4226"), QColor("#e8d3a8"), QColor("#b89565"), QColor("#dcc49b"),
          QColor("#e2c38f"), QColor("#b05c38"), QColor("#ece5d3"), QColor("#2e2e2e"), QColor("#f5d33b"), QColor("#10461a") },
        { "tournament", t("Red and black on sand"),
          QColor("#e2c38f"), QColor("#e8d3a8"), QColor("#b89565"), QColor("#dcc49b"),
          QColor("#a8232b"), QColor("#1f1a1a"), QColor("#ece5d3"), QColor("#2e2e2e"), QColor("#2f8fe0"), QColor("#0f5a14") },
        { "tournament-brown", t("Red and black on brown"),
          QColor("#855c3a"), QColor("#e8d3a8"), QColor("#b89565"), QColor("#dcc49b"),
          QColor("#a8232b"), QColor("#1f1a1a"), QColor("#ece5d3"), QColor("#2e2e2e"), QColor("#f5d33b"), QColor("#0f5a14") },
        { "felt", t("Green felt"),
          QColor("#1d6a52"), QColor("#2a7a61"), QColor("#145441"), QColor("#145441"),
          QColor("#3a8c71"), QColor("#155a44"), QColor("#f5f5f5"), QColor("#232323"), QColor("#f5d33b"), QColor("#0d3529") },
        { "marrakesh", t("Marrakesh"),
          QColor("#e2d9cf"), QColor("#b8aeaa"), QColor("#a39994"), QColor("#d3c9c0"),
          QColor("#9e1426"), QColor("#66222c"), QColor("#e8e6e4"), QColor("#d4304e"), QColor("#f2b51d"), QColor("#3d0610") },
        { "night", t("Night"),
          QColor("#2b2f3a"), QColor("#3a404f"), QColor("#1c1f27"), QColor("#232731"),
          QColor("#5a6275"), QColor("#8a5cf6"), QColor("#e9e9ef"), QColor("#111318"), QColor("#4dd0e1"), QColor("#15171d") },
    };
    return themes;
}

const BoardTheme &BoardTheme::current()
{
    return all().at(currentIndex);
}

void BoardTheme::setCurrent(const QString &id)
{
    int index = 0;
    for (int i = 0; i < all().size(); ++i)
        if (all().at(i).id == id)
            index = i;
    if (index == currentIndex)
        return;
    currentIndex = index;
    emit BoardThemeNotifier::instance().changed();
}

bool BoardTheme::useBackground()
{
    return backgroundEnabled;
}

void BoardTheme::setUseBackground(bool use)
{
    if (use == backgroundEnabled)
        return;
    backgroundEnabled = use;
    emit BoardThemeNotifier::instance().changed();
}

QPixmap BoardTheme::preview(const QSize &size) const
{
    QPixmap pixmap(size);
    pixmap.fill(backgroundEnabled ? background : QColor(Qt::transparent));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // With the background on, the board sits on it with a small margin.
    const qreal margin = backgroundEnabled ? qMax(4.0, size.height() * 0.06) : 0.0;
    painter.translate(margin, margin);
    const qreal w = size.width() - 2 * margin, h = size.height() - 2 * margin;
    const qreal side = w * 0.06, bar = w * 0.05;
    const qreal pointW = (w - 2 * side - bar) / 12.0, pointH = h * 0.4;

    painter.setPen(QPen(frameLine, 1));
    painter.setBrush(frame);
    painter.drawRect(QRectF(0.5, 0.5, w - 1, h - 1));
    painter.setBrush(felt);
    painter.drawRect(QRectF(side, 0.5, w - 2 * side, h - 1));
    painter.setBrush(frame);
    painter.drawRect(QRectF((w - bar) / 2, 0.5, bar, h - 1));

    painter.setPen(Qt::NoPen);
    for (int i = 0; i < 12; ++i) {
        const qreal x = side + i * pointW + (i >= 6 ? bar : 0);
        painter.setBrush(i % 2 ? pointDark : pointLight);
        painter.drawPolygon(QPolygonF({ QPointF(x, 1), QPointF(x + pointW, 1), QPointF(x + pointW / 2, pointH) }));
        painter.setBrush(i % 2 ? pointLight : pointDark);
        painter.drawPolygon(QPolygonF({ QPointF(x, h - 1), QPointF(x + pointW, h - 1), QPointF(x + pointW / 2, h - pointH) }));
    }

    const qreal r = pointW * 0.45;
    auto checker = [&](qreal x, qreal y, const QColor &color) {
        painter.setPen(QPen(color.lightness() > 128 ? color.darker(140) : color.lighter(180), 1));
        painter.setBrush(color);
        painter.drawEllipse(QPointF(x, y), r, r);
    };
    for (int i = 0; i < 3; ++i) {
        checker(side + pointW / 2, 1 + r + i * 2 * r, whiteChecker);
        checker(w - side - pointW / 2, h - 1 - r - i * 2 * r, blackChecker);
    }
    return pixmap;
}

BoardThemeNotifier &BoardThemeNotifier::instance()
{
    static BoardThemeNotifier notifier;
    return notifier;
}
