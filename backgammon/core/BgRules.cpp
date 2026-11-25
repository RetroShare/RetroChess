/*******************************************************************************
 * core/BgRules.cpp                                                            *
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

#include "BgRules.h"

#include <algorithm>
#include <cstdlib>
#include <set>
#include <sstream>

namespace bg {

/* ---------------------------------------------------------------------- */
/* Moves as text                                                           */
/* ---------------------------------------------------------------------- */

std::string toString(const Move &move)
{
    const std::string from = move.from == BarPoint ? "bar" : std::to_string(move.from);
    const std::string to = move.to == OffPoint ? "off" : std::to_string(move.to);
    return from + "/" + to;
}

std::string toString(const Turn &turn)
{
    std::string text;
    for (const Move &move : turn) {
        if (!text.empty())
            text += ' ';
        text += toString(move);
    }
    return text;
}

static bool parsePoint(const std::string &text, int &point, bool from)
{
    if (from && text == "bar") { point = BarPoint; return true; }
    if (!from && text == "off") { point = OffPoint; return true; }
    if (text.empty() || text.size() > 2 || !std::all_of(text.begin(), text.end(), ::isdigit))
        return false;
    point = std::atoi(text.c_str());
    return point >= 1 && point <= 24;
}

bool parseTurn(const std::string &text, Turn &turn)
{
    turn.clear();
    std::istringstream stream(text);
    std::string word;
    while (stream >> word) {
        const auto slash = word.find('/');
        if (slash == std::string::npos)
            return false;
        Move move;
        if (!parsePoint(word.substr(0, slash), move.from, true)
                || !parsePoint(word.substr(slash + 1), move.to, false))
            return false;
        turn.push_back(move);
        if (turn.size() > 4)
            return false;
    }
    return true;
}

/* ---------------------------------------------------------------------- */
/* Position                                                                */
/* ---------------------------------------------------------------------- */

Position Position::initial(Variant variant)
{
    Position position;
    position.m_variant = variant;
    for (Side side : { White, Black }) {
        if (variant == Variant::Backgammon) {
            position.add(side, position.absolute(side, 24), 2);
            position.add(side, position.absolute(side, 13), 5);
            position.add(side, position.absolute(side, 8), 3);
            position.add(side, position.absolute(side, 6), 5);
        } else {
            position.add(side, position.absolute(side, 24), CheckersPerSide); // the head
        }
    }
    return position;
}

Position Position::custom(Variant variant, const std::array<int, 25> &points, int whiteBar, int blackBar)
{
    Position position;
    position.m_variant = variant;
    position.m_points = points;
    position.m_points[0] = 0;
    position.m_bar = { whiteBar, blackBar };
    int white = whiteBar, black = blackBar;
    for (int point = 1; point <= 24; ++point) {
        white += std::max(points[point], 0);
        black += std::max(-points[point], 0);
    }
    position.m_off = { CheckersPerSide - white, CheckersPerSide - black };
    return position;
}

int Position::absolute(Side side, int point) const
{
    if (side == White)
        return point;
    // Backgammon: the sides move towards each other.
    // Long Nardy: both move the same way round, heads opposite each other.
    return m_variant == Variant::Backgammon ? 25 - point : (point + 11) % 24 + 1;
}

int Position::checkersAt(Side side, int absolutePoint) const
{
    const int value = m_points[absolutePoint];
    return side == White ? std::max(value, 0) : std::max(-value, 0);
}

int Position::checkers(Side side, int point) const
{
    return checkersAt(side, absolute(side, point));
}

void Position::add(Side side, int absolutePoint, int count)
{
    m_points[absolutePoint] += side == White ? count : -count;
}

int Position::pipCount(Side side) const
{
    int pips = 25 * m_bar[side];
    for (int point = 1; point <= 24; ++point)
        pips += point * checkers(side, point);
    return pips;
}

bool Position::allHome(Side side) const
{
    if (m_bar[side])
        return false;
    for (int point = 7; point <= 24; ++point)
        if (checkers(side, point))
            return false;
    return true;
}

void Position::apply(Side side, const Move &move)
{
    if (move.from == BarPoint)
        --m_bar[side];
    else
        add(side, absolute(side, move.from), -1);

    if (move.to == OffPoint) {
        ++m_off[side];
        return;
    }
    const int target = absolute(side, move.to);
    const Side other = opponent(side);
    if (checkersAt(other, target) == 1) {   // a hit (only possible in Backgammon)
        add(other, target, -1);
        ++m_bar[other];
    }
    add(side, target, 1);
}

void Position::apply(Side side, const Turn &turn)
{
    for (const Move &move : turn)
        apply(side, move);
}

bool Position::operator==(const Position &other) const
{
    return m_variant == other.m_variant && m_points == other.m_points
        && m_bar == other.m_bar && m_off == other.m_off;
}

/* ---------------------------------------------------------------------- */
/* Legal turns                                                             */
/* ---------------------------------------------------------------------- */

namespace {

bool blocked(const Position &position, Side side, int point)
{
    const int opponentCheckers = position.checkersAt(opponent(side), position.absolute(side, point));
    return position.variant() == Variant::Backgammon ? opponentCheckers >= 2 : opponentCheckers >= 1;
}

// Long Nardy: six points in a row are only allowed if at least one opposing
// checker has already gone past them.
bool forbiddenBlock(const Position &position, Side side)
{
    const Side other = opponent(side);
    for (int start = 1; start <= 19; ++start) {
        bool block = true;
        for (int point = start; point < start + 6 && block; ++point)
            block = position.checkers(side, point) > 0;
        if (!block)
            continue;

        // The block seen from the opponent: it is passed by any opposing
        // checker on a lower point (or already borne off).
        int lowest = 25;
        for (int point = start; point < start + 6; ++point) {
            const int absolutePoint = position.absolute(side, point);
            for (int theirs = 1; theirs <= 24; ++theirs)
                if (position.absolute(other, theirs) == absolutePoint)
                    lowest = std::min(lowest, theirs);
        }
        if (position.off(other) > 0)
            continue;
        bool passed = false;
        for (int theirs = 1; theirs < lowest && !passed; ++theirs)
            passed = position.checkers(other, theirs) > 0;
        if (!passed)
            return true;
    }
    return false;
}

void singleMoves(const Position &position, Side side, int die, bool headAllowed, std::vector<Move> &moves)
{
    moves.clear();
    if (position.bar(side)) {
        const int target = 25 - die;
        if (!blocked(position, side, target))
            moves.push_back({ BarPoint, target });
        return;   // nothing else moves while a checker is on the bar
    }

    const bool bearingOff = position.allHome(side);
    for (int point = 24; point >= 1; --point) {
        if (!position.checkers(side, point))
            continue;
        if (point == 24 && position.variant() == Variant::LongNardy && !headAllowed)
            continue;

        const int target = point - die;
        if (target >= 1) {
            if (!blocked(position, side, target))
                moves.push_back({ point, target });
        } else if (bearingOff) {
            bool higher = false;
            for (int above = point + 1; above <= 6 && !higher; ++above)
                higher = position.checkers(side, above) > 0;
            // Exact number, or a larger one when no checker is further back.
            if (target == 0 || !higher)
                moves.push_back({ point, OffPoint });
        }
    }
}

struct Search {
    const Side side;
    const int headMovesAllowed;
    std::vector<Turn> turns;
    std::vector<std::vector<int>> diceUsed;

    void run(const Position &position, std::vector<int> dice, Turn &turn, std::vector<int> &used, int headMoves)
    {
        bool moved = false;
        std::vector<Move> moves;
        for (size_t i = 0; i < dice.size(); ++i) {
            if (i > 0 && dice[i] == dice[i - 1])
                continue; // same number: same moves
            singleMoves(position, side, dice[i], headMoves < headMovesAllowed, moves);
            for (const Move &move : moves) {
                Position next = position;
                next.apply(side, move);
                if (position.variant() == Variant::LongNardy && forbiddenBlock(next, side))
                    continue;
                moved = true;
                std::vector<int> rest = dice;
                rest.erase(rest.begin() + i);
                turn.push_back(move);
                used.push_back(dice[i]);
                run(next, rest, turn, used, headMoves + (move.from == 24 ? 1 : 0));
                turn.pop_back();
                used.pop_back();
            }
        }
        if (!moved) {
            turns.push_back(turn);
            diceUsed.push_back(used);
        }
    }
};

} // namespace

std::vector<Turn> legalTurns(const Position &position, Side side, const std::vector<int> &rolled, int headMovesAllowed)
{
    std::vector<int> dice = rolled;
    if (dice.size() == 2 && dice[0] == dice[1])
        dice = { dice[0], dice[0], dice[0], dice[0] };
    std::sort(dice.rbegin(), dice.rend());

    Search search { side, position.variant() == Variant::LongNardy ? headMovesAllowed : 99, {}, {} };
    Turn turn;
    std::vector<int> used;
    search.run(position, dice, turn, used, 0);

    // Use as many dice as possible.
    size_t longest = 0;
    for (const Turn &candidate : search.turns)
        longest = std::max(longest, candidate.size());

    // If only one of two different dice can be played, play the larger one.
    const bool largerOnly = longest == 1 && dice.size() == 2 && dice[0] != dice[1]
        && std::any_of(search.diceUsed.begin(), search.diceUsed.end(),
                       [&](const std::vector<int> &d) { return d.size() == 1 && d[0] == dice[0]; });

    // The same moves can be found twice (bearing off a checker with either
    // of two larger dice): keep each sequence once.
    std::vector<Turn> result;
    std::set<std::vector<int>> seen;
    for (size_t i = 0; i < search.turns.size(); ++i) {
        if (search.turns[i].size() != longest)
            continue;
        if (largerOnly && search.diceUsed[i][0] != dice[0])
            continue;
        std::vector<int> key;
        for (const Move &move : search.turns[i]) {
            key.push_back(move.from);
            key.push_back(move.to);
        }
        if (seen.insert(key).second)
            result.push_back(search.turns[i]);
    }
    if (result.empty())
        result.push_back({});
    return result;
}

int gameValue(const Position &position, Side winner)
{
    const Side loser = opponent(winner);
    if (position.off(loser) > 0)
        return 1;
    if (position.variant() == Variant::LongNardy)
        return 2; // mars
    if (position.bar(loser))
        return 3;
    for (int point = 1; point <= 6; ++point)   // the winner's home board
        if (position.checkersAt(loser, position.absolute(winner, point)))
            return 3;
    return 2;
}

int Dice::roll()
{
    // std::uniform_int_distribution differs between standard libraries;
    // the output of std::mt19937 does not.
    constexpr uint32_t limit = 4294967292u; // largest multiple of 6 below 2^32
    uint32_t value;
    do {
        value = static_cast<uint32_t>(m_rng());
    } while (value >= limit);
    return 1 + static_cast<int>(value % 6);
}

} // namespace bg
