/*******************************************************************************
 * core/BgRules.h                                                              *
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

// Backgammon and Long Nardy rules. Plain C++17, no Qt, so the rules can be
// compiled and tested on their own (see tests/core-test.cpp).

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace bg {

enum Side : int { White = 0, Black = 1 };
inline Side opponent(Side side) { return side == White ? Black : White; }

enum class Variant { Backgammon, LongNardy };

constexpr int BarPoint = 25;        // "from" of a checker entering from the bar
constexpr int OffPoint = 0;         // "to" of a checker borne off
constexpr int CheckersPerSide = 15;

// Points are numbered from the moving side's own point of view: every side
// moves from its 24 point down to its 1 point, its home board is 1-6.
struct Move {
    int from = 0;   // 1..24 or BarPoint
    int to = 0;     // 1..24 or OffPoint
    bool operator==(const Move &other) const { return from == other.from && to == other.to; }
    bool operator!=(const Move &other) const { return !(*this == other); }
};
using Turn = std::vector<Move>;

// "24/18 13/11", "bar/20", "6/off"; an empty turn is "".
std::string toString(const Move &move);
std::string toString(const Turn &turn);
bool parseTurn(const std::string &text, Turn &turn);

class Position
{
public:
    Position() = default;
    static Position initial(Variant variant);
    // Any position: points[1..24] in White's numbering, > 0 white, < 0 black.
    static Position custom(Variant variant, const std::array<int, 25> &points,
                           int whiteBar = 0, int blackBar = 0);

    Variant variant() const { return m_variant; }

    // Absolute point (1..24, White's numbering) of a side's own point.
    int absolute(Side side, int point) const;
    // Checkers of a side on its own point 1..24.
    int checkers(Side side, int point) const;
    // Checkers of a side on an absolute point.
    int checkersAt(Side side, int absolutePoint) const;
    int bar(Side side) const { return m_bar[side]; }
    int off(Side side) const { return m_off[side]; }
    int pipCount(Side side) const;

    bool allHome(Side side) const;
    bool finished(Side side) const { return m_off[side] == CheckersPerSide; }

    // Plays a move that is known to be legal (hits included).
    void apply(Side side, const Move &move);
    void apply(Side side, const Turn &turn);

    bool operator==(const Position &other) const;
    bool operator!=(const Position &other) const { return !(*this == other); }

private:
    Variant m_variant = Variant::Backgammon;
    std::array<int, 25> m_points {};   // [1..24]: > 0 white, < 0 black checkers
    std::array<int, 2> m_bar {};
    std::array<int, 2> m_off {};

    void add(Side side, int absolutePoint, int count);
};

// Every legal way to play the dice: as many dice as possible, the larger die
// when only one of two can be played, no forbidden six-point block in Long
// Nardy. A roll that cannot be played gives one empty turn.
// headMovesAllowed: Long Nardy only, checkers that may leave the head (24).
std::vector<Turn> legalTurns(const Position &position, Side side,
                             const std::vector<int> &dice, int headMovesAllowed = 1);

// Points for the winner of a finished game, before the cube: 1, gammon 2,
// backgammon 3 (Long Nardy: 1, or 2 for a mars).
int gameValue(const Position &position, Side winner);

// Dice that give the same rolls on every platform for the same seed.
class Dice
{
public:
    void seed(uint32_t value) { m_rng.seed(value); }
    int roll();
private:
    std::mt19937 m_rng { std::random_device {}() };
};

} // namespace bg
