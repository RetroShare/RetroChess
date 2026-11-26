/*******************************************************************************
 * gui/toaster/BackgammonToasterNotify.cpp                                     *
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

#include "BackgammonToasterNotify.h"

#include "BackgammonToaster.h"
#include "gui/BackgammonGameWindow.h"
#include "gui/BackgammonSettings.h"
#include "services/BackgammonService.h"

#include "gui/toaster/ToasterItem.h"

BackgammonToasterNotify::BackgammonToasterNotify(QObject *parent)
    : ToasterNotify(parent)
{
    connect(&BackgammonService::instance(), &BackgammonService::invitationReceived, this,
            [this](BackgammonGameWindow *window) {
        if (!notifyEnabled() || !window)
            return;
        // Forget invitations already answered, keep the queue short.
        m_pending.removeAll(QPointer<BackgammonGameWindow>());
        if (m_pending.size() < 10)
            m_pending.append(window);
    });
}

bool BackgammonToasterNotify::hasSetting(QString &name)
{
    name = tr("Backgammon invitations");
    return true;
}

bool BackgammonToasterNotify::notifyEnabled()
{
    return BackgammonSettings::invitationToaster();
}

void BackgammonToasterNotify::setNotifyEnabled(bool enabled)
{
    BackgammonSettings::setInvitationToaster(enabled);
    if (!enabled)
        m_pending.clear();
}

ToasterItem *BackgammonToasterNotify::toasterItem()
{
    while (!m_pending.isEmpty()) {
        QPointer<BackgammonGameWindow> window = m_pending.takeFirst();
        // Skip invitations already answered or withdrawn.
        if (!window || window->phase() != BackgammonGameWindow::Offered)
            continue;
        ToasterItem *item = new ToasterItem(new BackgammonToaster(window));
        item->timeToLive = 15000; // time to react, not RetroShare's 3 s default
        return item;
    }
    return nullptr;
}

ToasterItem *BackgammonToasterNotify::testToasterItem()
{
    return new ToasterItem(new BackgammonToaster(nullptr));
}
