/*******************************************************************************
 * core/BgMatch.h                                                              *
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

// A match: games played until one side reaches the match length, with the
// doubling cube and the Crawford rule. Plain C++17, no Qt.
//
// Both peers of a network match run their own Match with the same seeds, so
// the dice agree without ever being sent; only decisions (roll, turn,
// double, take, drop) travel between them.

#include "BgRules.h"

#include <optional>

namespace bg {

class Match
{
public:
    enum class Phase {
        Rolling,        // the side on roll has to roll (or double)
        Moving,         // dice rolled, the side on roll plays them
        DoubleOffered,  // waiting for the opponent to take or drop
        GameOver,       // call startGame() for the next game
        MatchOver
    };

    struct GameResult {
        Side winner = White;
        int points = 0;       // cube included
        bool dropped = false; // a double was dropped
    };

    Match(Variant variant, int length);

    // Starts the next game; its dice come from the seed. The opening roll is
    // made at once: each side throws one die, the higher one starts.
    void startGame(uint32_t seed);

    Variant variant() const { return m_variant; }
    int length() const { return m_length; }
    Phase phase() const { return m_phase; }
    Side onRoll() const { return m_onRoll; }
    const Position &position() const { return m_position; }
    const std::vector<int> &dice() const { return m_dice; }
    // Index of the current game (0 = first) and turns played in it.
    int gameIndex() const { return m_gameIndex; }
    int ply() const { return m_ply; }
    int score(Side side) const { return m_score[side]; }
    int cube() const { return m_cube; }
    std::optional<Side> cubeOwner() const { return m_cubeOwner; }
    bool crawfordGame() const { return m_crawford; }
    // The opening dice of the current game (white's die, black's die).
    std::pair<int, int> openingDice() const { return m_opening; }
    const GameResult &lastResult() const { return m_lastResult; }

    // Legal turns while Moving.
    const std::vector<Turn> &legalTurns() const { return m_legal; }
    bool cannotMove() const { return m_legal.size() == 1 && m_legal.front().empty(); }

    bool canDouble(Side side) const;

    // Actions; each returns false (and changes nothing) when not allowed.
    bool roll(Side side);
    // Accepts any legal turn; also one played in a different order that
    // gives the same position as a legal turn.
    bool play(Side side, const Turn &turn);
    bool offerDouble(Side side);
    bool take(Side side);
    bool drop(Side side);
    // Ends the match: the other side wins.
    void resign(Side side);

private:
    void endGame(Side winner, int points, bool dropped);
    void computeLegalTurns();

    Variant m_variant;
    int m_length;
    Phase m_phase = Phase::GameOver;
    Position m_position;
    Side m_onRoll = White;
    std::vector<int> m_dice;
    std::vector<Turn> m_legal;
    Dice m_rng;
    int m_gameIndex = -1;
    int m_ply = 0;
    int m_turnsPlayed[2] = { 0, 0 };
    int m_score[2] = { 0, 0 };
    int m_cube = 1;
    std::optional<Side> m_cubeOwner;
    bool m_crawford = false;
    bool m_crawfordDone = false;
    std::pair<int, int> m_opening { 0, 0 };
    GameResult m_lastResult;
};

} // namespace bg
