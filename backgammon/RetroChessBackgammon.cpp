/*******************************************************************************
 * backgammon/RetroChessBackgammon.cpp                                         *
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

#include "RetroChessBackgammon.h"

#include "core/BgRules.h"
#include "gui/BackgammonGameWindow.h"
#include "gui/BackgammonSettings.h"
#include "gui/BoardTheme.h"
#include "services/BackgammonService.h"
#include "gui/toaster/BackgammonToasterNotify.h"

#include "gui/settings/rsharesettings.h"

#include <QApplication>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>

// Q_INIT_RESOURCE must be used outside of any namespace.
static void initBackgammonResources()
{
    Q_INIT_RESOURCE(Backgammon_images);
    Q_INIT_RESOURCE(Backgammon_sounds);
    Q_INIT_RESOURCE(Backgammon_qss);
}

namespace {

// The Backgammon plugin's own toaster notifier, unchanged: it queues
// invitations itself and follows Backgammon's "Show a toaster" setting.
BackgammonToasterNotify *toasters = nullptr;

// Same keys as the standalone plugin, so both share the player's choices.
void loadSettings()
{
    BackgammonService::instance().setContactsOnly(
        Settings->valueFromGroup("Backgammon", "ContactsOnly", false).toBool());
    BoardTheme::setCurrent(
        Settings->valueFromGroup("Backgammon", "BoardTheme", QString()).toString());
    BoardTheme::setUseBackground(
        Settings->valueFromGroup("Backgammon", "ThemeBackground", false).toBool());
    BackgammonSettings::applyDebugLogging();
}

void invite(const RsGxsId &peer, const RsGxsId &local, bg::Variant variant, int length)
{
    QString error;
    if (!BackgammonService::instance().invite(peer, local, variant, length, &error))
        QMessageBox::warning(nullptr, QObject::tr("Backgammon"),
                             error.isEmpty() ? QObject::tr("Could not send the invitation.") : error);
}

} // namespace

void RetroChessBackgammon::registerService(RsGxsTunnelService *service)
{
    static bool initialised = false;
    if (!initialised) {
        initialised = true;
        initBackgammonResources();
        BackgammonService &games = BackgammonService::instance();
        toasters = new BackgammonToasterNotify(&games);
        // Toaster and game window follow the Backgammon settings, as in the
        // Backgammon plugin. RetroChess has no "Your games" list, so with
        // both switched off the window opens anyway.
        QObject::connect(&games, &BackgammonService::invitationReceived, &games,
                         [](BackgammonGameWindow *window) {
            if (window && !window->isVisible() && !BackgammonSettings::invitationToaster())
                window->show();
        });
        // Settings are read once the GUI runs.
        QTimer::singleShot(0, &games, [] { loadSettings(); });
    }
    BackgammonService::instance().connectToGxsTunnelService(service);
}

void RetroChessBackgammon::shutdown()
{
    BackgammonService::instance().shutdown();
}

void RetroChessBackgammon::addContactAction(QMenu &menu, const RsGxsId &peer,
                                            const RsGxsId &local, bool online)
{
    // Not connected: the standalone Backgammon plugin owns the service.
    const bool connected = BackgammonService::instance().isConnected();
    QMenu *sub = menu.addMenu(QIcon(QStringLiteral(":/images/backgammon-32.png")),
                              connected ? QObject::tr("Invite to Backgammon")
                                        : QObject::tr("Invite to Backgammon (use the Backgammon plugin)"));
    sub->setEnabled(connected && online && !local.isNull() && !peer.isNull() && peer != local);

    struct Choice { const char *text; bg::Variant variant; int length; };
    static const Choice choices[] = {
        { QT_TRANSLATE_NOOP("RetroChessBackgammon", "Backgammon, 1 point"),  bg::Variant::Backgammon, 1 },
        { QT_TRANSLATE_NOOP("RetroChessBackgammon", "Backgammon, 3 points"), bg::Variant::Backgammon, 3 },
        { QT_TRANSLATE_NOOP("RetroChessBackgammon", "Backgammon, 5 points"), bg::Variant::Backgammon, 5 },
        { QT_TRANSLATE_NOOP("RetroChessBackgammon", "Backgammon, 7 points"), bg::Variant::Backgammon, 7 },
        { QT_TRANSLATE_NOOP("RetroChessBackgammon", "Long Nardy, 1 point"),  bg::Variant::LongNardy, 1 },
        { QT_TRANSLATE_NOOP("RetroChessBackgammon", "Long Nardy, 3 points"), bg::Variant::LongNardy, 3 },
    };
    for (const Choice &choice : choices) {
        if (choice.variant == bg::Variant::LongNardy && choice.length == 1)
            sub->addSeparator();
        QAction *action = sub->addAction(qApp->translate("RetroChessBackgammon", choice.text));
        const bg::Variant variant = choice.variant;
        const int length = choice.length;
        QObject::connect(action, &QAction::triggered, &menu, [=] { invite(peer, local, variant, length); });
    }
}

ToasterItem *RetroChessBackgammon::toasterItem()
{
    return toasters ? toasters->toasterItem() : nullptr;
}

std::vector<RetroChessBackgammon::DebugMatch> RetroChessBackgammon::debugMatches()
{
    std::vector<DebugMatch> out;
    std::vector<BackgammonService::TunnelTraffic> tunnels;
    if (!BackgammonService::instance().tunnelTraffic(tunnels))
        return out;
    for (const BackgammonService::TunnelTraffic &t : tunnels) {
        DebugMatch match;
        match.info = t.info;
        match.what = t.match.toStdString();
        match.packetsSent = t.packetsSent;
        match.packetsReceived = t.packetsReceived;
        out.push_back(match);
    }
    return out;
}

namespace {
// Pending invitation windows: received (Offered) or sent (not answered yet).
bool pending(const BackgammonGameWindow *window, bool *incoming = nullptr)
{
    if (!window)
        return false;
    const auto phase = window->phase();
    if (incoming)
        *incoming = phase == BackgammonGameWindow::Offered;
    return phase == BackgammonGameWindow::Offered || phase == BackgammonGameWindow::Connecting
            || phase == BackgammonGameWindow::Waiting;
}
} // namespace

QList<RetroChessBackgammon::Invitation> RetroChessBackgammon::invitations()
{
    QList<Invitation> out;
    for (BackgammonGameWindow *window : BackgammonService::instance().games()) {
        Invitation invitation;
        if (!pending(window, &invitation.incoming))
            continue;
        invitation.peer = window->peerId();
        const QString game = window->variant() == bg::Variant::LongNardy
                ? QObject::tr("Long Nardy") : QObject::tr("Backgammon");
        invitation.game = window->matchLength() == 1
                ? QObject::tr("%1, 1 point").arg(game)
                : QObject::tr("%1, %2 points").arg(game).arg(window->matchLength());
        out.append(invitation);
    }
    return out;
}

void RetroChessBackgammon::acceptInvitation(const RsGxsId &peer)
{
    for (BackgammonGameWindow *window : BackgammonService::instance().games()) {
        bool incoming = false;
        if (pending(window, &incoming) && incoming && window->peerId() == peer) {
            window->acceptInvitation();   // also brings the game window up
            return;
        }
    }
}

void RetroChessBackgammon::declineInvitation(const RsGxsId &peer)
{
    for (BackgammonGameWindow *window : BackgammonService::instance().games()) {
        bool incoming = false;
        if (pending(window, &incoming) && window->peerId() == peer) {
            // Closing the window sends the decline, or withdraws our invitation.
            if (incoming)
                window->declineInvitation();
            else
                window->close();
            return;
        }
    }
}

void RetroChessBackgammon::onChanged(QObject *context, const std::function<void()> &changed)
{
    QObject::connect(&BackgammonService::instance(), &BackgammonService::gamesChanged, context,
                     [changed] { changed(); });
}
