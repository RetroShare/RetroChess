/*******************************************************************************
 * gui/ChessTimeControl.h                                                      *
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

#include <QString>

/**
 * @brief Represents a chess time control (unlimited or minutes+increment).
 *
 * This is a plain value type — no QObject, no heap allocation.
 * Use ChessTimeControl::unlimited() for an unlimited game (the default).
 * Use ChessTimeControl(minutes, increment) for a real-time game.
 */
struct ChessTimeControl
{
    bool unlimited  = true; ///< When true all other fields are ignored.
    int  minutes    = 10;   ///< Minutes per side (1–180).
    int  increment  = 0;    ///< Seconds added after each move (0–180).

    ChessTimeControl() = default;

    ChessTimeControl(int minutes_, int increment_)
        : unlimited(false), minutes(minutes_), increment(increment_) {}

    // -----------------------------------------------------------------------
    // Derived properties
    // -----------------------------------------------------------------------

    /// Total estimated game length in seconds (used for category classification).
    int estimatedSeconds() const
    {
        return minutes * 60 + increment * 40;
    }

    /**
     * @brief Speed category name: "Bullet", "Blitz", "Rapid", or "Classical".
     *
     * Thresholds match Lichess conventions:
     *   Bullet   < 3 minutes estimated
     *   Blitz    < 8 minutes estimated
     *   Rapid    < 25 minutes estimated
     *   Classical ≥ 25 minutes estimated
     */
    QString category() const
    {
        if (unlimited) return QStringLiteral("Unlimited");
        const int s = estimatedSeconds();
        if (s < 179)  return QStringLiteral("Bullet");
        if (s < 479)  return QStringLiteral("Blitz");
        if (s < 1499) return QStringLiteral("Rapid");
        return QStringLiteral("Classical");
    }

    /// Human-readable label, e.g. "5+3 Blitz" or "Unlimited".
    QString label() const
    {
        if (unlimited) return QStringLiteral("Unlimited");
        return toNetString() + QLatin1Char(' ') + category();
    }

    // -----------------------------------------------------------------------
    // Serialization helpers (compact wire format used in game actions)
    // -----------------------------------------------------------------------

    /**
     * @brief Compact network string: "unlimited" or "N+I" (e.g. "5+3").
     */
    QString toNetString() const
    {
        if (unlimited) return QStringLiteral("unlimited");
        return QString::number(minutes) + QLatin1Char('+') + QString::number(increment);
    }

    /**
     * @brief Parse from a network string produced by toNetString().
     *        Returns an unlimited control on any parse error.
     */
    static ChessTimeControl fromNetString(const QString &s)
    {
        if (s.isEmpty() || s == QLatin1String("unlimited"))
            return {};
        const int plus = s.indexOf(QLatin1Char('+'));
        if (plus < 1) return {};
        bool okM, okI;
        const int m = s.left(plus).toInt(&okM);
        const int i = s.mid(plus + 1).toInt(&okI);
        if (!okM || !okI || m < 1 || m > 180 || i < 0 || i > 180)
            return {};
        return ChessTimeControl(m, i);
    }

    // -----------------------------------------------------------------------
    // Convenience timing values
    // -----------------------------------------------------------------------

    /// Initial clock value in milliseconds.
    qint64 initialMs() const
    {
        return static_cast<qint64>(minutes) * 60 * 1000;
    }

    /// Increment in milliseconds.
    qint64 incrementMs() const
    {
        return static_cast<qint64>(increment) * 1000;
    }

    bool operator==(const ChessTimeControl &o) const
    {
        if (unlimited && o.unlimited) return true;
        if (unlimited != o.unlimited) return false;
        return minutes == o.minutes && increment == o.increment;
    }
    bool operator!=(const ChessTimeControl &o) const { return !(*this == o); }
};
