/*******************************************************************************
 * gui/BackgammonSounds.h                                                      *
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

// Game sounds, each of which can be switched off in the settings page.
// Nothing plays while RetroShare's sounds are muted.
namespace BackgammonSounds
{
enum Event {
    Invitation,    // someone invites you
    GameStart,     // a match starts
    Declined,      // invitation declined, opponent left
    Roll,          // dice rolled (you or your opponent)
    Checker,       // you moved a checker
    OpponentMove,  // your opponent played a turn
    CannotMove,    // the player on roll has no legal move
    Resign,        // you or your opponent resigned
    GameWon,
    GameLost,
    MatchWon,
    MatchLost,
    DoubleOffered, // a player doubles
    DoubleTaken,
    DoubleDropped,
    EventCount
};

void play(Event event);
// Plays the sound even if it is switched off (settings page preview).
void preview(Event event);

QString label(Event event);

bool soundsEnabled();
void setSoundsEnabled(bool enabled);
bool eventEnabled(Event event);
void setEventEnabled(Event event, bool enabled);
// 0..100
int volume();
void setVolume(int volume);
}
