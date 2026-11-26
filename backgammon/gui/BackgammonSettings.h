/*******************************************************************************
 * gui/BackgammonSettings.h                                                    *
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

// Plugin options kept in RetroShare's settings (group "Backgammon").
namespace BackgammonSettings
{
// Open the game window as soon as an invitation arrives. When off, the
// invitation only shows in "Your games" (and as a toaster).
bool openInvitationWindow();
void setOpenInvitationWindow(bool enabled);

// Show a toaster for new invitations (also in Preferences > Notify).
bool invitationToaster();
void setInvitationToaster(bool enabled);

// Debug logs (see services/BackgammonDebug.h).
bool tunnelLogging();
void setTunnelLogging(bool enabled);
bool gameLogging();
void setGameLogging(bool enabled);
// At start-up: log folder, and the logs switched on in the settings.
void applyDebugLogging();
}
