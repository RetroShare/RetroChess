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
#include <QTimer>
#include <QHBoxLayout>

// Tick interval in ms — fine enough for smooth display without CPU waste.
static constexpr int TICK_MS = 100;

// Threshold below which the clock turns red.
static constexpr qint64 LOW_TIME_MS = 30 * 1000;

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
    updateDisplay();
    applyStyle();
}

void ChessClockWidget::setIncrementMs(qint64 ms)
{
    m_incrementMs = ms;
}

void ChessClockWidget::startClock()
{
    if (m_remainingMs <= 0) return;
    m_active = true;
    m_timer->start();
    applyStyle();
}

void ChessClockWidget::pauseClock()
{
    m_active = false;
    m_timer->stop();
    // Add increment now that the move has been made.
    if (m_incrementMs > 0)
        m_remainingMs = qMin(m_remainingMs + m_incrementMs, m_remainingMs + m_incrementMs);
    updateDisplay();
    applyStyle();
}

void ChessClockWidget::syncTo(qint64 remainingMs)
{
    m_remainingMs = remainingMs;
    if (!m_expiredEmitted && m_remainingMs <= 0) {
        m_remainingMs = 0;
        m_active = false;
        m_timer->stop();
        m_expiredEmitted = true;
        updateDisplay();
        applyStyle();
        emit expired();
        return;
    }
    updateDisplay();
    applyStyle();
}

void ChessClockWidget::onTick()
{
    if (!m_active) return;
    m_remainingMs -= TICK_MS;
    if (m_remainingMs <= 0) {
        m_remainingMs = 0;
        m_active = false;
        m_timer->stop();
        updateDisplay();
        applyStyle();
        if (!m_expiredEmitted) {
            m_expiredEmitted = true;
            emit expired();
        }
        return;
    }
    updateDisplay();
    applyStyle();
}

void ChessClockWidget::updateDisplay()
{
    m_label->setText(formatTime(m_remainingMs));
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

void ChessClockWidget::applyStyle()
{
    const bool lowTime = (m_remainingMs > 0 && m_remainingMs <= LOW_TIME_MS);
    const bool expired = (m_remainingMs <= 0);

    QString bg, fg, fontWeight;

    if (expired) {
        bg         = QStringLiteral("#8b0000");
        fg         = QStringLiteral("white");
        fontWeight = QStringLiteral("bold");
    } else if (lowTime && m_active) {
        bg         = QStringLiteral("#c0392b");
        fg         = QStringLiteral("white");
        fontWeight = QStringLiteral("bold");
    } else if (lowTime) {
        bg         = QStringLiteral("#e74c3c");
        fg         = QStringLiteral("white");
        fontWeight = QStringLiteral("bold");
    } else if (m_active) {
        bg         = QStringLiteral("#1a1a1a");
        fg         = QStringLiteral("white");
        fontWeight = QStringLiteral("bold");
    } else {
        // Inactive — use default palette
        setStyleSheet(QString());
        m_label->setStyleSheet(QString());
        return;
    }

    setStyleSheet(QStringLiteral("ChessClockWidget { background-color: %1; border-radius: 4px; }").arg(bg));
    m_label->setStyleSheet(
        QStringLiteral("color: %1; font-size: 18pt; font-weight: %2;").arg(fg, fontWeight));
}
