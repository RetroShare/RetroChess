/*******************************************************************************
 * gui/BackgammonSettings.cpp                                                  *
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

#include "BackgammonSettings.h"

#include "services/BackgammonDebug.h"

#include "gui/settings/rsharesettings.h"

#include <retroshare/rsinit.h>

namespace {
const QString Group = QStringLiteral("Backgammon");
}

namespace BackgammonSettings
{

bool openInvitationWindow()
{
    return Settings->valueFromGroup(Group, "OpenInvitationWindow", true).toBool();
}

void setOpenInvitationWindow(bool enabled)
{
    Settings->setValueToGroup(Group, "OpenInvitationWindow", enabled);
}

bool invitationToaster()
{
    return Settings->valueFromGroup(Group, "InvitationToaster", true).toBool();
}

void setInvitationToaster(bool enabled)
{
    Settings->setValueToGroup(Group, "InvitationToaster", enabled);
}

bool tunnelLogging()
{
    return Settings->valueFromGroup(Group, "TunnelDebugLogging", false).toBool();
}

void setTunnelLogging(bool enabled)
{
    Settings->setValueToGroup(Group, "TunnelDebugLogging", enabled);
    BackgammonDebug::setEnabled(BackgammonDebug::Tunnel, enabled);
}

bool gameLogging()
{
    return Settings->valueFromGroup(Group, "GameDebugLogging", false).toBool();
}

void setGameLogging(bool enabled)
{
    Settings->setValueToGroup(Group, "GameDebugLogging", enabled);
    BackgammonDebug::setEnabled(BackgammonDebug::Game, enabled);
}

void applyDebugLogging()
{
    BackgammonDebug::setLogDirectory(RsAccounts::AccountDirectory());
    // A log switched on with BACKGAMMON_DEBUG stays on for this session.
    if (tunnelLogging())
        BackgammonDebug::setEnabled(BackgammonDebug::Tunnel, true);
    if (gameLogging())
        BackgammonDebug::setEnabled(BackgammonDebug::Game, true);
}

}
