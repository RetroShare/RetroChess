/*******************************************************************************
 * gui/BoardWidget.cpp                                                         *
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

#include "BoardWidget.h"
#include "BoardTheme.h"

#include <QFontMetricsF>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QToolTip>
#include <QVariantAnimation>

#include <algorithm>
#include <cmath>

namespace {
constexpr int RollAnimationMs = 650;   // how long new dice tumble
constexpr qreal RollDistance = 2.2;    // board units the rolling dice travel
constexpr qreal Pi = 3.14159265358979323846;

// Board units (one unit = one point = one checker diameter).
constexpr qreal Frame = 0.25;
constexpr qreal Width = 16.0 + 2 * Frame;
constexpr qreal Height = 12.0 + 2 * Frame;
constexpr qreal SideBar = 1.5;
constexpr qreal PointHeight = 5.0;
constexpr int VisibleStack = 5;

// Left edge of the left half, the bar, the right half and the tray.
constexpr qreal LeftHalf = Frame + SideBar;
constexpr qreal BarX = LeftHalf + 6;
constexpr qreal RightHalf = BarX + 1;
constexpr qreal TrayX = RightHalf + 6;
}

BoardWidget::BoardWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_position = bg::Position::initial(bg::Variant::Backgammon);
    m_shown = m_position;
    connect(&BoardThemeNotifier::instance(), &BoardThemeNotifier::changed, this, [this] { update(); });
}

/* ---------------------------------------------------------------------- */
/* State                                                                   */
/* ---------------------------------------------------------------------- */

void BoardWidget::setLocalSide(bg::Side side)
{
    m_local = side;
    update();
}

void BoardWidget::setPosition(const bg::Position &position)
{
    m_position = position;
    m_shown = position;
    m_played.clear();
    m_selected = -1;
    m_dragging = false;
    updateNextMoves();
    update();
}

void BoardWidget::setDice(const std::vector<int> &dice, bg::Side roller)
{
    const bool newRoll = dice.size() >= 2 && (dice != m_dice || roller != m_roller);
    m_dice = dice;
    m_roller = roller;
    update();

    if (!newRoll || !isVisible())
        return;
    if (!m_rollAnimation) {
        m_rollAnimation = new QVariantAnimation(this);
        m_rollAnimation->setStartValue(0.0);
        m_rollAnimation->setEndValue(1.0);
        m_rollAnimation->setDuration(RollAnimationMs);
        connect(m_rollAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_rollProgress = value.toReal();
            // New faces about every 70 ms, slower towards the end.
            const int frame = int(std::sqrt(m_rollProgress) * 9);
            if (frame != m_rollFrame && m_rollProgress < 0.85) {
                m_rollFrame = frame;
                for (int &face : m_rollFaces)
                    face = 1 + int(QRandomGenerator::global()->bounded(6));
            }
            update(rollArea().toAlignedRect());
        });
        connect(m_rollAnimation, &QVariantAnimation::finished, this, [this] {
            m_rollProgress = 1.0;
            update(rollArea().toAlignedRect());
        });
    }
    m_rollAnimation->stop();
    m_rollFrame = -1;
    m_rollProgress = 0.0;
    m_rollAnimation->start();
}

// The dice area plus the way the dice come in from (the bar side) and their
// shadows: the only part repainted while the dice roll.
QRectF BoardWidget::rollArea() const
{
    const qreal u = unit();
    const QRectF area = diceArea();
    const bool rightHalf = m_roller == m_local;
    return area.adjusted(rightHalf ? -RollDistance * u - 0.6 * u : -0.6 * u, -0.8 * u,
                         rightHalf ? 0.6 * u : RollDistance * u + 0.6 * u, 0.8 * u);
}

void BoardWidget::setCube(int value, std::optional<bg::Side> owner, bool visible)
{
    m_cubeValue = value;
    m_cubeOwner = owner;
    m_cubeVisible = visible;
    update();
}

void BoardWidget::setLegalTurns(const std::vector<bg::Turn> &turns)
{
    m_legal = turns;
    // A roll that cannot be played has one empty turn: nothing to move.
    if (m_legal.size() == 1 && m_legal.front().empty())
        m_legal.clear();
    m_played.clear();
    m_shown = m_position;
    m_selected = -1;
    m_dragging = false;
    updateNextMoves();
    setCursor(Qt::ArrowCursor);
    update();
}

bool BoardWidget::turnComplete() const
{
    return m_complete;
}

void BoardWidget::undo()
{
    if (m_played.empty())
        return;
    m_played.pop_back();
    m_shown = m_position;
    m_shown.apply(m_local, m_played);
    m_selected = -1;
    updateNextMoves();
    update();
    emit movesChanged();
}

void BoardWidget::updateNextMoves()
{
    m_next.clear();
    m_complete = false;
    for (const bg::Turn &turn : m_legal) {
        if (turn.size() < m_played.size() || !std::equal(m_played.begin(), m_played.end(), turn.begin()))
            continue;
        if (turn.size() == m_played.size()) {
            m_complete = true;
            continue;
        }
        const bg::Move &next = turn[m_played.size()];
        if (std::find(m_next.begin(), m_next.end(), next) == m_next.end())
            m_next.push_back(next);
    }
}

std::vector<int> BoardWidget::destinations(int from) const
{
    std::vector<int> result;
    for (const bg::Move &move : nextMoves())
        if (move.from == from && std::find(result.begin(), result.end(), move.to) == result.end())
            result.push_back(move.to);
    return result;
}

void BoardWidget::play(const bg::Move &move)
{
    const bool hit = move.to != bg::OffPoint
        && m_shown.checkersAt(bg::opponent(m_local), m_shown.absolute(m_local, move.to)) == 1;
    m_played.push_back(move);
    m_shown.apply(m_local, move);
    m_selected = -1;
    updateNextMoves();
    m_dragging = false;
    update();
    emit checkerMoved(hit);
    emit movesChanged();
}

void BoardWidget::setRollPrompt(bool roll)
{
    m_rollPrompt = roll;
    update();
}

void BoardWidget::setDoubleButton(bool shown, bool enabled)
{
    m_doubleShown = shown;
    m_doubleEnabled = shown && enabled;
    update();
}

void BoardWidget::select(int point)
{
    m_selected = point;
    update();
}

/* ---------------------------------------------------------------------- */
/* Geometry                                                                */
/* ---------------------------------------------------------------------- */

QRectF BoardWidget::boardRect() const
{
    const qreal scale = qMin(width() / Width, height() / Height);
    const qreal w = Width * scale, h = Height * scale;
    // Centred across, at the top: the players' row sits right above it.
    return QRectF((width() - w) / 2, 0, w, h);
}

qreal BoardWidget::unit() const
{
    return boardRect().width() / Width;
}

QPointF BoardWidget::toPixels(qreal x, qreal y) const
{
    const QRectF board = boardRect();
    return QPointF(board.left() + x * unit(), board.top() + y * unit());
}

QRectF BoardWidget::pointRect(int slot) const
{
    qreal x, y;
    if (slot <= 6)       { x = RightHalf + (6 - slot);  y = Frame + 12 - PointHeight; }
    else if (slot <= 12) { x = LeftHalf + (12 - slot);  y = Frame + 12 - PointHeight; }
    else if (slot <= 18) { x = LeftHalf + (slot - 13);  y = Frame; }
    else                 { x = RightHalf + (slot - 19); y = Frame; }
    return QRectF(toPixels(x, y), QSizeF(unit(), PointHeight * unit()));
}

QRectF BoardWidget::barRect(bool top) const
{
    return QRectF(toPixels(BarX, top ? Frame : Frame + 6), QSizeF(unit(), 6 * unit()));
}

QRectF BoardWidget::trayRect(bool top) const
{
    return QRectF(toPixels(TrayX + 0.15, top ? Frame + 0.3 : Frame + 6.3), QSizeF(1.2 * unit(), 5.4 * unit()));
}

QPointF BoardWidget::checkerCentre(int slot, int index) const
{
    const QRectF rect = pointRect(slot);
    const qreal u = unit();
    const int shown = qMin(index, VisibleStack - 1);
    const bool bottom = slot <= 12;
    return QPointF(rect.center().x(),
                   bottom ? rect.bottom() - 0.5 * u - shown * u : rect.top() + 0.5 * u + shown * u);
}

int BoardWidget::hitTest(const QPointF &pos) const
{
    if (trayRect(false).adjusted(-0.15 * unit(), -0.3 * unit(), 0.15 * unit(), 0.3 * unit()).contains(pos))
        return bg::OffPoint;
    if (barRect(false).contains(pos) || barRect(true).contains(pos))
        return bg::BarPoint;
    for (int slot = 1; slot <= 24; ++slot) {
        // The whole half of the board above or below a point counts.
        QRectF area = pointRect(slot);
        if (slot <= 12)
            area.setTop(area.top() - unit());
        else
            area.setBottom(area.bottom() + unit());
        if (area.contains(pos))
            return slot;
    }
    return -1;
}

bg::Side BoardWidget::sideAt(int slot) const
{
    return m_shown.checkersAt(bg::White, absoluteOf(slot)) ? bg::White : bg::Black;
}

int BoardWidget::countAt(int slot) const
{
    const int a = absoluteOf(slot);
    return m_shown.checkersAt(bg::White, a) + m_shown.checkersAt(bg::Black, a);
}

/* ---------------------------------------------------------------------- */
/* Input                                                                   */
/* ---------------------------------------------------------------------- */

void BoardWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF pos = event->position();
#else
    const QPointF pos = event->localPos();
#endif
    switch (buttonAt(pos)) {
    case RollButton:    emit rollRequested(); return;
    case DoubleButton:  emit doubleRequested(); return;
    case UndoButton:    undo(); return;
    case ConfirmButton: emit endTurnRequested(); return;
    case CubeButton:    emit doubleRequested(); return;
    case NoButton:      break;
    }
    if (m_legal.empty())
        return;
    const int point = hitTest(pos);

    // Second click of click-to-move: the destination.
    if (m_selected >= 0) {
        for (const bg::Move &move : nextMoves())
            if (move.from == m_selected && move.to == point) {
                play(move);
                return;
            }
    }

    m_selected = -1;
    if (point >= 0 && point != bg::OffPoint && !destinations(point).empty()) {
        m_selected = point;
        m_dragging = true;
        m_pressPos = pos;
        m_dragPos = pos;
        setCursor(Qt::ClosedHandCursor);
    }
    update();
}

void BoardWidget::mouseMoveEvent(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF pos = event->position();
#else
    const QPointF pos = event->localPos();
#endif
    if (m_dragging) {
        m_dragPos = pos;
        update();
        return;
    }
    const Button hover = buttonAt(pos);
    if (hover != m_hover) {
        m_hover = hover;
        update();
    }
    if (hover != NoButton) {
        setCursor(Qt::PointingHandCursor);
        if (hover == CubeButton)
            QToolTip::showText(mapToGlobal(pos.toPoint()), tr("Double"), this);
        return;
    }
    // Disabled Double button: say why it cannot be clicked.
    if (m_doubleShown && !m_doubleEnabled && buttonRect(DoubleButton).contains(pos))
        QToolTip::showText(mapToGlobal(pos.toPoint()),
                           tr("You can double before rolling when the cube is in the middle or yours "
                              "(not in the Crawford game)."), this);
    const int point = hitTest(pos);
    const bool movable = !m_legal.empty() && point >= 0 && point != bg::OffPoint && !destinations(point).empty();
    setCursor(movable ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

void BoardWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_dragging)
        return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF pos = event->position();
#else
    const QPointF pos = event->localPos();
#endif
    m_dragging = false;
    setCursor(Qt::ArrowCursor);
    const int point = hitTest(pos);

    if (point != m_selected) {
        for (const bg::Move &move : nextMoves())
            if (move.from == m_selected && move.to == point) {
                play(move);
                return;
            }
        // Dropped elsewhere: put the checker back.
        m_selected = -1;
    } else if (destinations(m_selected).size() == 1
               && (pos - m_pressPos).manhattanLength() < 4) {
        // A plain click on a checker with a single possible move plays it.
        for (const bg::Move &move : nextMoves())
            if (move.from == m_selected) {
                play(move);
                return;
            }
    }
    // A click without moving keeps the checker selected: click the destination.
    update();
}

void BoardWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const QRectF board = boardRect();
    emit boardMarginsChanged(qRound(board.left()), qRound(width() - board.right()));
}

void BoardWidget::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    if (m_hover != NoButton) {
        m_hover = NoButton;
        update();
    }
    if (!m_dragging)
        setCursor(Qt::ArrowCursor);
}

/* ---------------------------------------------------------------------- */
/* Painting                                                                */
/* ---------------------------------------------------------------------- */

void BoardWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (BoardTheme::useBackground())
        p.fillRect(rect(), BoardTheme::current().background);
    drawFrame(p);
    drawPoints(p);
    drawTrays(p);
    drawCube(p);
    drawCheckers(p);
    drawDice(p);
    drawOverlayButtons(p);
}

void BoardWidget::drawFrame(QPainter &p) const
{
    const BoardTheme &theme = BoardTheme::current();
    const QRectF board = boardRect();
    const qreal u = unit();

    p.setPen(QPen(theme.frameLine, 0.04 * u));
    p.setBrush(theme.frame);
    p.drawRoundedRect(board, 0.25 * u, 0.25 * u);

    p.setPen(Qt::NoPen);
    p.setBrush(theme.felt);
    p.drawRect(QRectF(toPixels(LeftHalf, Frame), QSizeF(6 * u, 12 * u)));
    p.drawRect(QRectF(toPixels(RightHalf, Frame), QSizeF(6 * u, 12 * u)));

    // Bar and side bars: frame colour with a thin edge.
    p.setPen(QPen(theme.frameLine, 0.03 * u));
    p.setBrush(theme.frame);
    p.drawRect(QRectF(toPixels(BarX, Frame), QSizeF(u, 12 * u)));
}

void BoardWidget::drawPoints(QPainter &p) const
{
    const BoardTheme &theme = BoardTheme::current();
    const std::vector<int> targets = m_selected >= 0 ? destinations(m_selected) : std::vector<int>();

    for (int slot = 1; slot <= 24; ++slot) {
        const QRectF rect = pointRect(slot);
        const bool bottom = slot <= 12;
        QPolygonF triangle;
        if (bottom)
            triangle << rect.bottomLeft() << rect.bottomRight() << QPointF(rect.center().x(), rect.top());
        else
            triangle << rect.topLeft() << rect.topRight() << QPointF(rect.center().x(), rect.bottom());
        p.setPen(Qt::NoPen);
        p.setBrush(slot % 2 ? theme.pointDark : theme.pointLight);
        p.drawPolygon(triangle);

        if (std::find(targets.begin(), targets.end(), slot) != targets.end()) {
            QColor glow = theme.highlight;
            glow.setAlpha(110);
            p.setBrush(glow);
            p.setPen(QPen(theme.highlight, 0.06 * unit()));
            p.drawPolygon(triangle);
        }
    }
}

void BoardWidget::drawChecker(QPainter &p, const QPointF &centre, qreal radius, bg::Side side, bool highlight) const
{
    // A raised rim lit from above with a recessed dish in the middle, on a
    // soft shadow. All colours come from the theme's checker colour.
    const BoardTheme &theme = BoardTheme::current();
    const QColor base = theme.checker(side);
    const bool light = base.lightness() > 128;
    const qreal r = radius * 0.96;

    p.save();
    p.setPen(Qt::NoPen);

    // Soft shadow below the checker.
    QRadialGradient shadow(centre + QPointF(0, 0.14 * r), 1.12 * r);
    shadow.setColorAt(0.80, QColor(0, 0, 0, 70));
    shadow.setColorAt(1.00, QColor(0, 0, 0, 0));
    p.setBrush(shadow);
    p.drawEllipse(centre + QPointF(0, 0.14 * r), 1.12 * r, 1.12 * r);

    // Edge: a thin darker outline.
    p.setBrush(light ? base.darker(135) : base.darker(115));
    p.drawEllipse(centre, r, r);

    // Rim: lighter at the top, darker at the bottom.
    const qreal rim = r * 0.95;
    QLinearGradient rimGradient(centre.x(), centre.y() - rim, centre.x(), centre.y() + rim);
    rimGradient.setColorAt(0.0, light ? base.lighter(106) : base.lighter(185));
    rimGradient.setColorAt(0.5, light ? base : base.lighter(130));
    rimGradient.setColorAt(1.0, light ? base.darker(118) : base);
    p.setBrush(rimGradient);
    p.drawEllipse(centre, rim, rim);

    // Dish: recessed, so its shading runs the other way (shadow at the top).
    const qreal dish = r * 0.70;
    const QPointF dishCentre = centre + QPointF(0, 0.03 * r);
    QLinearGradient lip(dishCentre.x(), dishCentre.y() - dish, dishCentre.x(), dishCentre.y() + dish);
    lip.setColorAt(0.0, light ? base.darker(130) : QColor(0, 0, 0));
    lip.setColorAt(1.0, light ? base.lighter(108) : base.lighter(200));
    p.setBrush(lip);
    p.drawEllipse(dishCentre, dish, dish);

    const qreal inner = dish * 0.94;
    QLinearGradient dishGradient(dishCentre.x(), dishCentre.y() - inner, dishCentre.x(), dishCentre.y() + inner);
    dishGradient.setColorAt(0.0, light ? base.darker(112) : base.darker(110));
    dishGradient.setColorAt(1.0, light ? base.lighter(104) : base.lighter(150));
    p.setBrush(dishGradient);
    p.drawEllipse(dishCentre + QPointF(0, 0.02 * r), inner, inner);

    // Movable checker or drag: a ring in the highlight colour.
    if (highlight) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(theme.highlight, 0.12 * radius));
        p.drawEllipse(centre, r - 0.06 * radius, r - 0.06 * radius);
    }
    p.restore();
}

void BoardWidget::drawCheckers(QPainter &p) const
{
    const qreal u = unit();
    const qreal radius = 0.47 * u;
    std::vector<int> sources;
    for (const bg::Move &move : nextMoves())
        sources.push_back(move.from);
    auto movable = [&](int point) {
        return !m_dragging && std::find(sources.begin(), sources.end(), point) != sources.end();
    };

    QFont font = this->font();
    font.setBold(true);
    font.setPixelSize(qMax(8, int(0.45 * u)));

    for (int slot = 1; slot <= 24; ++slot) {
        int count = countAt(slot);
        if (m_dragging && slot == m_selected)
            --count;   // the dragged checker follows the mouse
        if (count <= 0)
            continue;
        const bg::Side side = sideAt(slot);
        for (int i = 0; i < qMin(count, VisibleStack); ++i) {
            const bool top = i == qMin(count, VisibleStack) - 1;
            drawChecker(p, checkerCentre(slot, i), radius, side,
                        top && (movable(slot) || m_selected == slot));
        }
        if (count > VisibleStack) {
            const QPointF c = checkerCentre(slot, VisibleStack - 1);
            p.setFont(font);
            p.setPen(BoardTheme::current().checker(side).lightness() > 128 ? Qt::black : Qt::white);
            p.drawText(QRectF(c.x() - radius, c.y() - radius, 2 * radius, 2 * radius), Qt::AlignCenter,
                       QString::number(count));
        }
    }

    // Bar: the local player's checkers below the middle, the opponent's above.
    for (bg::Side side : { m_local, bg::opponent(m_local) }) {
        int count = m_shown.bar(side);
        if (m_dragging && side == m_local && m_selected == bg::BarPoint)
            --count;
        const bool mine = side == m_local;
        for (int i = 0; i < qMin(count, 4); ++i) {
            const qreal y = mine ? Frame + 6 + 0.8 + i : Frame + 6 - 0.8 - i;
            drawChecker(p, toPixels(BarX + 0.5, y), radius, side,
                        mine && i == qMin(count, 4) - 1 && (movable(bg::BarPoint) || m_selected == bg::BarPoint));
        }
        if (count > 4) {
            const qreal y = mine ? Frame + 6 + 0.8 + 3 : Frame + 6 - 0.8 - 3;
            const QPointF c = toPixels(BarX + 0.5, y);
            p.setFont(font);
            p.setPen(BoardTheme::current().checker(side).lightness() > 128 ? Qt::black : Qt::white);
            p.drawText(QRectF(c.x() - radius, c.y() - radius, 2 * radius, 2 * radius), Qt::AlignCenter,
                       QString::number(count));
        }
    }

    if (m_dragging)
        drawChecker(p, m_dragPos, radius, m_local, true);
}

void BoardWidget::drawTrays(QPainter &p) const
{
    const BoardTheme &theme = BoardTheme::current();
    const qreal u = unit();
    const std::vector<int> targets = m_selected >= 0 ? destinations(m_selected) : std::vector<int>();
    const bool offTarget = std::find(targets.begin(), targets.end(), bg::OffPoint) != targets.end();

    for (bool top : { true, false }) {
        const QRectF tray = trayRect(top);
        p.setPen(QPen(!top && offTarget ? theme.highlight : theme.frameLine, (!top && offTarget ? 0.08 : 0.03) * u));
        p.setBrush(theme.tray);
        p.drawRoundedRect(tray, 0.1 * u, 0.1 * u);

        // Borne-off checkers seen from the side, stacked from the middle out.
        const bg::Side side = top ? bg::opponent(m_local) : m_local;
        const int count = m_shown.off(side);
        const qreal slice = tray.height() / bg::CheckersPerSide;
        const QColor colour = theme.checker(side);
        for (int i = 0; i < count; ++i) {
            const qreal y = top ? tray.top() + i * slice : tray.bottom() - (i + 1) * slice;
            QRectF piece(tray.left() + 0.08 * u, y + 0.02 * u, tray.width() - 0.16 * u, slice - 0.04 * u);
            p.setPen(QPen(colour.lightness() > 128 ? colour.darker(150) : colour.lighter(170), 0.02 * u));
            p.setBrush(colour);
            p.drawRoundedRect(piece, 0.05 * u, 0.05 * u);
        }
    }

    // Left side bar trays (decoration, the cube lives there)
    p.setPen(QPen(theme.frameLine, 0.03 * u));
    p.setBrush(theme.tray);
    for (bool top : { true, false })
        p.drawRoundedRect(QRectF(toPixels(Frame + 0.15, top ? Frame + 0.3 : Frame + 6.3), QSizeF(1.2 * u, 5.4 * u)),
                          0.1 * u, 0.1 * u);
}

void BoardWidget::drawDie(QPainter &p, const QRectF &face, int value, qreal opacity, bg::Side roller) const
{
    static const QPointF pips[6][6] = {
        { { 0.50, 0.50 } },
        { { 0.72, 0.28 }, { 0.28, 0.72 } },
        { { 0.72, 0.28 }, { 0.50, 0.50 }, { 0.28, 0.72 } },
        { { 0.28, 0.28 }, { 0.28, 0.72 }, { 0.72, 0.28 }, { 0.72, 0.72 } },
        { { 0.28, 0.28 }, { 0.28, 0.72 }, { 0.72, 0.28 }, { 0.72, 0.72 }, { 0.50, 0.50 } },
        { { 0.28, 0.25 }, { 0.28, 0.50 }, { 0.28, 0.75 }, { 0.72, 0.25 }, { 0.72, 0.50 }, { 0.72, 0.75 } },
    };
    const qreal side = face.width();
    const qreal radius = 0.16 * side;

    p.save();
    p.setOpacity(opacity);   // played dice fade
    p.setPen(Qt::NoPen);
    for (int i = 3; i >= 1; --i) {
        const qreal grow = 0.025 * side * i;
        p.setBrush(QColor(0, 0, 0, 22));
        p.drawRoundedRect(face.translated(0.04 * side, 0.07 * side).adjusted(-grow, -grow, grow, grow),
                          radius + grow, radius + grow);
    }
    // The dice have the colour of the roller's checkers: light dice with dark
    // pips for the light side, dark dice with light pips for the dark side.
    const QColor base = BoardTheme::current().checker(roller);
    const bool light = base.lightness() > 128;
    QLinearGradient body(face.topLeft(), face.bottomLeft());
    body.setColorAt(0.0, light ? base.lighter(110) : base.lighter(170));
    body.setColorAt(1.0, light ? base.darker(105) : base);
    p.setBrush(body);
    p.setPen(QPen(light ? base.darker(130) : QColor(0, 0, 0, 160), 0.02 * side));
    p.drawRoundedRect(face, radius, radius);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(light ? QColor(255, 255, 255, 200) : QColor(255, 255, 255, 40), 0.03 * side));
    p.drawRoundedRect(face.adjusted(0.04 * side, 0.04 * side, -0.04 * side, -0.04 * side), radius * 0.8, radius * 0.8);

    p.setPen(Qt::NoPen);
    const qreal r = 0.095 * side;
    for (int i = 0; i < value; ++i) {
        const QPointF centre = face.topLeft() + QPointF(pips[value - 1][i].x() * side, pips[value - 1][i].y() * side);
        QRadialGradient pip(centre + QPointF(0.25 * r, 0.3 * r), r);
        pip.setColorAt(0.0, light ? QColor("#3a3a3a") : QColor("#ffffff"));
        pip.setColorAt(1.0, light ? QColor("#0a0a0a") : QColor("#d8d8d8"));
        p.setBrush(pip);
        p.drawEllipse(centre, r, r);
    }
    p.restore();
}

void BoardWidget::drawDice(QPainter &p) const
{
    if (m_dice.size() < 2)
        return;
    const qreal u = unit();
    const std::vector<int> dice = { m_dice[0], m_dice[1] };
    const bool doubles = dice[0] == dice[1];

    // How used each die is: 0 = not yet, 1 = fully. A double is played four
    // times with two dice: each die fades halfway after its first move and
    // fully after its second.
    qreal used[2] = { 0, 0 };
    if (m_roller == m_local && !m_played.empty()) {
        if (doubles) {
            const int moves = int(m_played.size());
            used[0] = qMin(moves, 2) / 2.0;
            used[1] = qBound(0, moves - 2, 2) / 2.0;
        } else {
            for (const bg::Move &move : m_played) {
                const int needed = move.from == bg::BarPoint ? 25 - move.to
                                 : move.to == bg::OffPoint ? -move.from   // any die at least this large
                                 : move.from - move.to;
                int best = -1;
                for (int i = 0; i < 2; ++i) {
                    if (used[i] > 0)
                        continue;
                    const bool fits = needed > 0 ? dice[i] == needed : dice[i] >= -needed;
                    if (fits && (best < 0 || (needed < 0 && dice[i] < dice[best])))
                        best = i;
                }
                if (best >= 0)
                    used[best] = 1;
            }
        }
    }

    const QRectF area = diceArea();
    const qreal size = area.height();
    const qreal gap = 0.2 * u;
    if (m_rollProgress >= 1.0) {
        for (int i = 0; i < 2; ++i) {
            const QRectF face(area.left() + i * (size + gap), area.top(), size, size);
            drawDie(p, face, dice[i], 1.0 - 0.65 * used[i], m_roller);
        }
        return;
    }

    // Rolling: the dice come in from the bar side, spinning and hopping,
    // and slow down until they lie in their place with the real faces.
    const qreal t = m_rollProgress;
    const qreal ease = 1.0 - std::pow(1.0 - t, 3);                     // fast, then slow
    const qreal direction = m_roller == m_local ? -1.0 : 1.0;          // the bar is on that side
    const qreal hop = std::abs(std::sin(t * 3.0 * Pi)) * (1.0 - t) * 0.35 * u;
    for (int i = 0; i < 2; ++i) {
        const QRectF face(area.left() + i * (size + gap), area.top(), size, size);
        const QPointF offset(direction * (1.0 - ease) * (RollDistance + 0.4 * i) * u, -hop * (i ? 0.7 : 1.0));
        const qreal angle = (1.0 - ease) * (i ? -320.0 : 400.0);
        p.save();
        p.translate(face.center() + offset);
        p.rotate(angle);
        drawDie(p, QRectF(-size / 2, -size / 2, size, size), t < 0.85 ? m_rollFaces[i] : dice[i], 1.0, m_roller);
        p.restore();
    }
}

QRectF BoardWidget::cubeRect() const
{
    const qreal size = 0.9;
    qreal y = Frame + 6 - size / 2;                             // middle: nobody owns it
    if (m_cubeOwner)
        y = *m_cubeOwner == m_local ? Frame + 12 - 0.4 - size   // the local player's side
                                    : Frame + 0.4;
    return QRectF(toPixels(Frame + (SideBar - size) / 2, y), QSizeF(size * unit(), size * unit()));
}

void BoardWidget::drawCube(QPainter &p) const
{
    if (!m_cubeVisible)
        return;
    const qreal u = unit();
    const QRectF face = cubeRect();
    const qreal radius = 0.14 * face.width();

    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 50));
    p.drawRoundedRect(face.translated(0.04 * u, 0.06 * u), radius, radius);
    QLinearGradient body(face.topLeft(), face.bottomLeft());
    body.setColorAt(0.0, QColor("#ffffff"));
    body.setColorAt(1.0, QColor("#dedede"));
    p.setBrush(body);
    // A cube that can be clicked to double gets the highlight colour.
    p.setPen(m_doubleEnabled ? QPen(BoardTheme::current().highlight, (m_hover == CubeButton ? 0.1 : 0.06) * u)
                             : QPen(QColor("#9a9a9a"), 0.03 * u));
    p.drawRoundedRect(face, radius, radius);
    QFont font = this->font();
    font.setBold(true);
    const int shown = m_cubeOwner ? m_cubeValue : 64;
    font.setPixelSize(qMax(8, int((shown >= 10 ? 0.42 : 0.55) * u)));
    p.setFont(font);
    p.setPen(QColor("#1a1a1a"));
    p.drawText(face, Qt::AlignCenter, QString::number(shown));
    p.restore();
}

/* ---------------------------------------------------------------------- */
/* Buttons on the board                                                    */
/* ---------------------------------------------------------------------- */

QRectF BoardWidget::diceArea() const
{
    const int count = 2;   // doubles too: two dice, see drawDice()
    const qreal size = 0.9;
    const qreal gap = 0.2;
    const qreal total = count * size + (count - 1) * gap;
    const qreal left = (m_roller == m_local ? RightHalf : LeftHalf) + (6 - total) / 2;
    return QRectF(toPixels(left, Frame + 6 - size / 2), QSizeF(total * unit(), size * unit()));
}

QRectF BoardWidget::buttonRect(Button button) const
{
    const qreal u = unit();
    switch (button) {
    case RollButton:
        return QRectF(toPixels(RightHalf + 1.5, Frame + 6 - 0.55), QSizeF(3 * u, 1.1 * u));
    case DoubleButton:
        return QRectF(toPixels(LeftHalf + 1.5, Frame + 6 - 0.55), QSizeF(3 * u, 1.1 * u));
    case UndoButton:
        return QRectF(toPixels(BarX + 0.5 - 0.48, Frame + 6 - 0.48), QSizeF(0.96 * u, 0.96 * u));
    case ConfirmButton:
        return diceArea().adjusted(-0.15 * u, -0.15 * u, 0.15 * u, 0.15 * u);
    case NoButton:
        break;
    }
    return QRectF();
}

BoardWidget::Button BoardWidget::buttonAt(const QPointF &pos) const
{
    if (m_rollPrompt && buttonRect(RollButton).contains(pos))
        return RollButton;
    if (m_doubleEnabled && buttonRect(DoubleButton).contains(pos))
        return DoubleButton;
    if (m_doubleEnabled && m_cubeVisible && cubeRect().contains(pos))
        return CubeButton;
    if (!m_legal.empty() && !m_dragging) {
        if (canUndo() && buttonRect(UndoButton).contains(pos))
            return UndoButton;
        if (turnComplete() && !m_dice.empty() && buttonRect(ConfirmButton).contains(pos))
            return ConfirmButton;
    }
    return NoButton;
}

void BoardWidget::drawPill(QPainter &p, const QRectF &rect, const QString &text, bool hover, PillIcon icon) const
{
    const qreal u = unit();
    const qreal radius = rect.height() / 2;
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 70));
    p.drawRoundedRect(rect.translated(0, 0.06 * u), radius, radius);
    p.setBrush(hover ? QColor("#2a2a2a") : QColor("#000000"));
    p.drawRoundedRect(rect, radius, radius);

    QFont font = this->font();
    font.setBold(true);
    font.setPixelSize(qMax(9, int(0.42 * u)));
    p.setFont(font);

    // Icon and text centred together: [icon] Text
    const qreal iconSize = icon == NoIcon ? 0 : 0.5 * u;
    const qreal gap = icon == NoIcon ? 0 : 0.22 * u;
    const qreal textWidth = QFontMetricsF(font).horizontalAdvance(text);
    const qreal left = rect.center().x() - (iconSize + gap + textWidth) / 2;
    const QRectF iconRect(left, rect.center().y() - iconSize / 2, iconSize, iconSize);

    if (icon == DiceIcon)
        drawDiceIcon(p, iconRect);
    else if (icon == CubeIcon) {
        // The doubling cube: a small white square with a 2.
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawRoundedRect(iconRect, 0.08 * u, 0.08 * u);
        QFont small = font;
        small.setPixelSize(qMax(7, int(0.36 * u)));
        p.setFont(small);
        p.setPen(Qt::black);
        p.drawText(iconRect, Qt::AlignCenter, QStringLiteral("2"));
        p.setFont(font);
    }

    p.setPen(Qt::white);
    p.drawText(QRectF(left + iconSize + gap, rect.top(), textWidth + 1, rect.height()),
               Qt::AlignVCenter | Qt::AlignLeft, text);
    p.restore();
}

void BoardWidget::drawDiceIcon(QPainter &p, const QRectF &area) const
{
    // Two small dice, the back one tilted, like the icons of online games.
    const qreal side = area.width() * 0.62;
    auto die = [&](const QPointF &centre, qreal angle, const std::vector<QPointF> &pips) {
        p.save();
        p.translate(centre);
        p.rotate(angle);
        const QRectF face(-side / 2, -side / 2, side, side);
        p.setPen(QPen(Qt::black, side * 0.08));
        p.setBrush(Qt::white);
        p.drawRoundedRect(face, side * 0.2, side * 0.2);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        const qreal r = side * 0.1;
        for (const QPointF &pip : pips)
            p.drawEllipse(QPointF(face.left() + pip.x() * side, face.top() + pip.y() * side), r, r);
        p.restore();
    };
    die(area.topLeft() + QPointF(side * 0.55, side * 0.55), -15,
        { { 0.28, 0.28 }, { 0.72, 0.28 }, { 0.5, 0.5 }, { 0.28, 0.72 }, { 0.72, 0.72 } });
    die(area.bottomRight() - QPointF(side * 0.55, side * 0.55), 12,
        { { 0.3, 0.3 }, { 0.7, 0.7 } });
}

void BoardWidget::drawOverlayButtons(QPainter &p) const
{
    const qreal u = unit();
    if (m_rollPrompt)
        drawPill(p, buttonRect(RollButton), tr("Roll"), m_hover == RollButton, DiceIcon);
    if (m_doubleShown) {
        p.save();
        if (!m_doubleEnabled)
            p.setOpacity(0.4);   // shown, but doubling is not allowed now
        drawPill(p, buttonRect(DoubleButton), tr("Double"), m_hover == DoubleButton, CubeIcon);
        p.restore();
    }

    if (m_legal.empty() || m_dragging)
        return;

    // Undo: a round green button with a turning arrow, on the bar.
    if (canUndo()) {
        const QRectF rect = buttonRect(UndoButton);
        p.save();
        p.setPen(QPen(Qt::white, 0.06 * u));
        p.setBrush(m_hover == UndoButton ? QColor("#14a01a") : QColor("#0e8a14"));
        p.drawEllipse(rect);
        const QRectF arc = rect.adjusted(0.27 * u, 0.27 * u, -0.27 * u, -0.27 * u);
        p.setPen(QPen(Qt::white, 0.08 * u, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawArc(arc, 100 * 16, 290 * 16);
        // Arrow head at the start of the arc (upper left), pointing left.
        const QPointF tip(arc.center().x() - 0.03 * u, arc.top());
        QPolygonF head;
        head << tip + QPointF(-0.13 * u, 0) << tip + QPointF(0.06 * u, -0.12 * u) << tip + QPointF(0.06 * u, 0.12 * u);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawPolygon(head);
        p.restore();
    }

    // Complete turn: a check mark on the dice; clicking them ends the turn.
    if (turnComplete() && !m_dice.empty()) {
        const QRectF dice = diceArea();
        const qreal r = 0.32 * u;
        const QPointF c = dice.center();
        p.save();
        p.setPen(QPen(Qt::white, 0.05 * u));
        p.setBrush(m_hover == ConfirmButton ? QColor("#14a01a") : QColor("#0e8a14"));
        p.drawEllipse(c, r, r);
        p.setPen(QPen(Qt::white, 0.07 * u, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPolygonF check;
        check << c + QPointF(-0.14 * u, 0.0) << c + QPointF(-0.04 * u, 0.1 * u) << c + QPointF(0.15 * u, -0.11 * u);
        p.drawPolyline(check);
        p.restore();
    }
}
