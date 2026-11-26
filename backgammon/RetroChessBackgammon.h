/*******************************************************************************
 * backgammon/RetroChessBackgammon.h                                           *
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

// Backgammon as an extra game of RetroChess, like 4connect/.
//
// core/, services/ and gui/ are copied from the standalone Backgammon plugin
// (plugins/Backgammon) without changes, minus its main page, toaster and
// Preferences page: invitations are sent from RetroChess' contact menu.
// RetroChess only uses the calls below.
//
// The protocol is the one of the Backgammon plugin (service id "BGMN"), so
// a RetroChess player can play against a Backgammon plugin user. Only one of
// the two can own the service in one RetroShare: when the Backgammon plugin
// is loaded too, whichever registers first wins and the other stays inactive.

#include <retroshare/rsgxstunnel.h>

#include <QList>
#include <QString>

#include <functional>
#include <string>
#include <vector>

class QMenu;
class QObject;
class ToasterItem;

namespace RetroChessBackgammon {
void registerService(RsGxsTunnelService *service);
// The plugin is being unloaded: stop handling tunnel callbacks.
void shutdown();
// Adds an "Invite to Backgammon" sub-menu to a contact menu of RetroChess.
// online: the contact is reachable right now (available, playing or busy).
void addContactAction(QMenu &menu, const RsGxsId &peer, const RsGxsId &local, bool online);
// Next Backgammon invitation toaster, or nullptr. Backgammon's own setting
// decides whether there are any; the chess toaster setting does not apply.
// RetroShare takes one ToasterNotify per plugin, so RetroChessToasterNotify
// asks here when it has no chess invitation to show.
ToasterItem *toasterItem();

// For the RetroChess debug window (Tunnels and Contacts tabs): the tunnels
// of Backgammon matches, with RetroShare's statistics in `info`.
struct DebugMatch {
    RsGxsTunnelService::GxsTunnelInfo info {};
    std::string what;              // "Playing, game 2", "Invitation sent"...
    unsigned int packetsSent = 0;  // Backgammon packets only
    unsigned int packetsReceived = 0;
};
std::vector<DebugMatch> debugMatches();

// Invitations not answered yet, for the "Available players" list.
struct Invitation {
    RsGxsId peer;
    bool incoming = false;  // received (Accept / Reject) or sent (Pending...)
    QString game;           // "Backgammon, 3 points", "Long Nardy, 1 point"
};
QList<Invitation> invitations();
void acceptInvitation(const RsGxsId &peer);
// Declines a received invitation, or cancels a sent one.
void declineInvitation(const RsGxsId &peer);
// Calls `changed` (in the GUI thread, coalesced) when invitations or matches
// change, as long as `context` exists.
void onChanged(QObject *context, const std::function<void()> &changed);
}
