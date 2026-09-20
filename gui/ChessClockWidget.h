/*******************************************************************************
 * gui/ChessClockWidget.h                                                      *
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

#pragma once

#include <QFrame>

class QLabel;
class QTimer;

/**
 * @brief A compact countdown clock for one player.
 *
 * Displays remaining time as MM:SS (or H:MM:SS when >= 1 hour).
 * Background turns red when less than 30 seconds remain.
 * The font becomes bold when the clock is actively ticking.
 *
 * Typical usage:
 * @code
 *   auto *clock = new ChessClockWidget(this);
 *   clock->setTotalMs(5 * 60 * 1000);   // 5 minutes
 *   clock->setIncrementMs(3 * 1000);    // +3 s per move
 *   clock->startClock();                // start ticking
 *   // ... after a move ...
 *   clock->pauseClock();                // stop + add increment
 * @endcode
 */
class ChessClockWidget : public QFrame
{
    Q_OBJECT

public:
    explicit ChessClockWidget(QWidget *parent = nullptr);

    /// Set the initial total time in milliseconds (call before startClock).
    void setTotalMs(qint64 ms);

    /// Set the increment added after each move, in milliseconds.
    void setIncrementMs(qint64 ms);

    /// Start ticking (it is this player's turn).
    void startClock();

    /// Stop ticking and add the increment (move was made).
    void pauseClock();

    /// Force-set remaining time (used to sync with remote move packet).
    void syncTo(qint64 remainingMs);

    /// Remaining time in milliseconds.
    qint64 remainingMs() const { return m_remainingMs; }

    /// True when the clock has reached zero.
    bool isExpired() const { return m_remainingMs <= 0; }

signals:
    /// Emitted exactly once when the clock reaches zero.
    void expired();

private slots:
    void onTick();

private:
    void updateDisplay();
    QString formatTime(qint64 ms) const;
    void applyStyle();

    QLabel  *m_label        = nullptr;
    QTimer  *m_timer        = nullptr;
    qint64   m_remainingMs  = 0;
    qint64   m_incrementMs  = 0;
    bool     m_active       = false;
    bool     m_expiredEmitted = false;
};
