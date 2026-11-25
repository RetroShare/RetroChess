/*******************************************************************************
 * core/BgMatch.cpp                                                            *
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

#include "BgMatch.h"

#include <algorithm>

namespace bg {

Match::Match(Variant variant, int length)
    : m_variant(variant),
      m_length(std::max(1, length)),
      m_position(Position::initial(variant))
{
}

void Match::startGame(uint32_t seed)
{
    if (m_phase == Phase::MatchOver)
        return;

    m_rng.seed(seed);
    m_position = Position::initial(m_variant);
    ++m_gameIndex;
    m_ply = 0;
    m_turnsPlayed[White] = m_turnsPlayed[Black] = 0;
    m_cube = 1;
    m_cubeOwner.reset();
    m_lastResult = GameResult();

    int white, black;
    do {
        white = m_rng.roll();
        black = m_rng.roll();
    } while (white == black);
    m_opening = { white, black };
    m_onRoll = white > black ? White : Black;

    if (m_variant == Variant::Backgammon) {
        // The opening dice are the first roll.
        m_dice = { std::max(white, black), std::min(white, black) };
        m_phase = Phase::Moving;
        computeLegalTurns();
    } else {
        // Long Nardy: the starter rolls again.
        m_dice.clear();
        m_legal.clear();
        m_phase = Phase::Rolling;
    }
}

void Match::computeLegalTurns()
{
    int head = 1;
    // First turn of each side in Long Nardy: 6-6, 4-4 and 3-3 may take two
    // checkers from the head (the only way to use those rolls at all).
    if (m_variant == Variant::LongNardy && m_turnsPlayed[m_onRoll] == 0 && m_dice.size() == 2
            && m_dice[0] == m_dice[1] && (m_dice[0] == 6 || m_dice[0] == 4 || m_dice[0] == 3))
        head = 2;
    m_legal = bg::legalTurns(m_position, m_onRoll, m_dice, head);
}

bool Match::canDouble(Side side) const
{
    return m_phase == Phase::Rolling && m_onRoll == side
        && m_variant == Variant::Backgammon && m_length > 1 && !m_crawford
        && m_cube < 64 && (!m_cubeOwner || *m_cubeOwner == side);
}

bool Match::roll(Side side)
{
    if (m_phase != Phase::Rolling || side != m_onRoll)
        return false;
    const int first = m_rng.roll();
    const int second = m_rng.roll();
    m_dice = { first, second };
    m_phase = Phase::Moving;
    computeLegalTurns();
    return true;
}

bool Match::play(Side side, const Turn &turn)
{
    if (m_phase != Phase::Moving || side != m_onRoll)
        return false;

    const Turn *legal = nullptr;
    for (const Turn &candidate : m_legal)
        if (candidate == turn)
            legal = &candidate;
    if (!legal) {
        // Same checkers, other order: compare the resulting positions.
        if (turn.size() > 4)
            return false;
        Position wanted = m_position;
        for (const Move &move : turn) {
            if (move.from < 1 || move.from > BarPoint || move.to < OffPoint || move.to > 24)
                return false;
        }
        // Only replay moves that start from a checker of the side.
        for (const Move &move : turn) {
            if (move.from == BarPoint ? !wanted.bar(side) : !wanted.checkers(side, move.from))
                return false;
            wanted.apply(side, move);
        }
        for (const Turn &candidate : m_legal) {
            Position result = m_position;
            result.apply(side, candidate);
            if (result == wanted)
                legal = &candidate;
        }
    }
    if (!legal)
        return false;

    m_position.apply(side, *legal);
    ++m_ply;
    ++m_turnsPlayed[side];
    m_dice.clear();
    m_legal.clear();

    if (m_position.finished(side)) {
        endGame(side, gameValue(m_position, side) * m_cube, false);
        return true;
    }
    m_onRoll = opponent(side);
    m_phase = Phase::Rolling;
    return true;
}

bool Match::offerDouble(Side side)
{
    if (!canDouble(side))
        return false;
    m_phase = Phase::DoubleOffered;
    return true;
}

bool Match::take(Side side)
{
    if (m_phase != Phase::DoubleOffered || side == m_onRoll)
        return false;
    m_cube *= 2;
    m_cubeOwner = side;
    m_phase = Phase::Rolling;
    return true;
}

bool Match::drop(Side side)
{
    if (m_phase != Phase::DoubleOffered || side == m_onRoll)
        return false;
    endGame(m_onRoll, m_cube, true);
    return true;
}

void Match::resign(Side side)
{
    if (m_phase == Phase::MatchOver)
        return;
    const Side winner = opponent(side);
    m_score[winner] = std::max(m_score[winner], m_length);
    m_lastResult = { winner, 0, false };
    m_phase = Phase::MatchOver;
}

void Match::endGame(Side winner, int points, bool dropped)
{
    m_score[winner] += points;
    m_lastResult = { winner, points, dropped };
    m_dice.clear();
    m_legal.clear();

    if (m_score[winner] >= m_length) {
        m_phase = Phase::MatchOver;
        return;
    }

    // Crawford rule: no doubling in the game right after a side first gets
    // one point away from winning the match.
    m_crawford = false;
    if (!m_crawfordDone && (m_score[White] == m_length - 1 || m_score[Black] == m_length - 1)) {
        m_crawford = true;
        m_crawfordDone = true;
    }
    m_phase = Phase::GameOver;
}

} // namespace bg
