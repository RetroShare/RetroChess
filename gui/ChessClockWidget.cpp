/*******************************************************************************
 * gui/ChessClockWidget.cpp                                                    *
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

#include "ChessClockWidget.h"

#include <QLabel>
#include <QShowEvent>
#include <QTimer>
#include <QHBoxLayout>

// Tick interval in ms — fine enough for smooth display without CPU waste.
static constexpr int TICK_MS = 100;

ChessClockWidget::ChessClockWidget(QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Sunken);

    m_label = new QLabel(QStringLiteral("--:--"), this);
    m_label->setAlignment(Qt::AlignCenter);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->addWidget(m_label);

    m_timer = new QTimer(this);
    m_timer->setInterval(TICK_MS);
    connect(m_timer, &QTimer::timeout, this, &ChessClockWidget::onTick);

    // Initial neutral style
    applyStyle();
}

void ChessClockWidget::setTotalMs(qint64 ms)
{
    m_remainingMs   = ms;
    m_expiredEmitted = false;
    if (m_active) m_elapsed.start();
    updateDisplay();
}

void ChessClockWidget::setIncrementMs(qint64 ms)
{
    m_incrementMs = ms;
}

void ChessClockWidget::accountElapsed()
{
    // Measure real elapsed time instead of assuming that every timer tick
    // took exactly TICK_MS: a busy GUI thread (modal dialog, slow repaint)
    // delays ticks and would otherwise make the clock run slow.
    if (!m_elapsed.isValid()) {
        m_elapsed.start();
        return;
    }
    m_remainingMs -= m_elapsed.restart();
}

void ChessClockWidget::startClock()
{
    if (m_remainingMs <= 0) return;
    if (m_active) return;           // already running: keep the elapsed reference
    m_active = true;
    m_elapsed.start();
    m_timer->start();
}

void ChessClockWidget::pauseClock()
{
    const bool wasActive = m_active;
    if (wasActive) {
        accountElapsed();
        // The move was made before the flag fell; expiry is detected by
        // onTick(), never from inside a move. Keep the clock alive.
        if (m_remainingMs <= 0) m_remainingMs = 1;
    }
    m_active = false;
    m_timer->stop();
    // The increment is earned by completing a move, so it is only added when
    // this clock was actually running. pauseClock() is also called on the idle
    // side's clock every turn change and at game start, which must not
    // grant any time.
    if (wasActive && m_incrementMs > 0)
        m_remainingMs += m_incrementMs;
    updateDisplay();
}

void ChessClockWidget::stopClock()
{
    if (m_active) {
        accountElapsed();
        if (m_remainingMs < 0) m_remainingMs = 0;
    }
    m_active = false;
    m_timer->stop();
    updateDisplay();
}

void ChessClockWidget::syncTo(qint64 remainingMs)
{
    m_remainingMs = remainingMs;
    if (m_active) m_elapsed.start();
    if (!m_expiredEmitted && m_remainingMs <= 0) {
        m_remainingMs = 0;
        m_active = false;
        m_timer->stop();
        m_expiredEmitted = true;
        updateDisplay();
        emit expired();
        return;
    }
    updateDisplay();
}

void ChessClockWidget::onTick()
{
    if (!m_active) return;
    accountElapsed();
    if (m_remainingMs <= 0) {
        m_remainingMs = 0;
        m_active = false;
        m_timer->stop();
        updateDisplay();
        if (!m_expiredEmitted) {
            m_expiredEmitted = true;
            emit expired();
        }
        return;
    }
    updateDisplay();
}

void ChessClockWidget::updateDisplay()
{
    m_label->setText(formatTime(m_remainingMs));
    applyStyle(m_remainingMs <= 0);
}

QString ChessClockWidget::formatTime(qint64 ms) const
{
    if (ms < 0) ms = 0;
    const qint64 totalSec = ms / 1000;
    const qint64 hours    = totalSec / 3600;
    const qint64 minutes  = (totalSec % 3600) / 60;
    const qint64 seconds  = totalSec % 60;

    if (hours > 0)
        return QString::asprintf("%lld:%02lld:%02lld",
            static_cast<long long>(hours),
            static_cast<long long>(minutes),
            static_cast<long long>(seconds));

    return QString::asprintf("%02lld:%02lld",
        static_cast<long long>(minutes),
        static_cast<long long>(seconds));
}

void ChessClockWidget::applyStyle(bool expired)
{
    if (m_styleApplied && m_expiredStyle == expired) return;
    m_styleApplied = true;
    m_expiredStyle = expired;
    // Only override the background at zero; text styling comes from the skin.
    // Explicit border so skins (which drop the native StyledPanel frame)
    // still show a rounded box. The colour is a faint version of the skin's
    // text colour, so it is light on dark skins and grey on light skins.
    // (palette(mid) does not work here: stylesheet skins don't change the
    // palette, so it stays the light system grey even in dark mode.)
    m_label->ensurePolished();
    const QColor text = m_label->palette().color(QPalette::WindowText);
    const QString border = QStringLiteral("rgba(%1, %2, %3, 70)")
        .arg(text.red()).arg(text.green()).arg(text.blue());

    setStyleSheet(expired
        ? QStringLiteral("ChessClockWidget { background-color: #8b0000;"
                         " border: 1px solid #8b0000; border-radius: 4px; }")
        : QStringLiteral("ChessClockWidget { border: 1px solid %1;"
                         " border-radius: 4px; }").arg(border));
    m_label->setStyleSheet(QStringLiteral(
        "font-size: 18pt; font-weight: bold;"));
}

void ChessClockWidget::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);
    // The skin is only guaranteed to be applied once we are shown.
    m_styleApplied = false;
    applyStyle(isExpired());
}
