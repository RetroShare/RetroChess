/*******************************************************************************
 * 4connect/FourBoard.h                                                        *
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
#include <array>

class FourBoard
{
public:
    int at(int row, int column) const { return cells[row][column]; }
    int landing(int column) const
    {
        if (column >= 0 && column < 7)
            for (int row = 5; row >= 0; --row)
                if (!at(row, column)) return row;
        return -1;
    }
    int turn() const { return moves % 2 + 1; }
    bool drop(int column)
    {
        const int row = landing(column);
        if (row < 0 || winner || moves == 42) return false;
        const int player = turn();
        cells[row][column] = player;
        ++moves;
        const int directions[][2] = {{1,0},{0,1},{1,1},{1,-1}};
        for (const auto &d : directions) {
            int count = 1;
            for (int sign : {-1,1}) {
                int r = row + sign*d[0], c = column + sign*d[1];
                while (r >= 0 && r < 6 && c >= 0 && c < 7 && at(r,c) == player) {
                    ++count;
                    r += sign*d[0];
                    c += sign*d[1];
                }
            }
            if (count >= 4) winner = player;
        }
        return true;
    }
    int moves = 0;
    int winner = 0;
private:
    std::array<std::array<int,7>,6> cells{};
};
