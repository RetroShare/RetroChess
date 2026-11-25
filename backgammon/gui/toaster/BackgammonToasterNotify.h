/*******************************************************************************
 * gui/toaster/BackgammonToasterNotify.h                                       *
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

#include "gui/common/ToasterNotify.h"

#include <QList>
#include <QPointer>

class BackgammonGameWindow;

// Gives RetroShare a toaster for each new invitation (it asks regularly).
class BackgammonToasterNotify : public ToasterNotify
{
    Q_OBJECT

public:
    explicit BackgammonToasterNotify(QObject *parent = nullptr);

    bool hasSetting(QString &name) override;
    bool notifyEnabled() override;
    void setNotifyEnabled(bool enabled) override;
    ToasterItem *toasterItem() override;
    ToasterItem *testToasterItem() override;

private:
    QList<QPointer<BackgammonGameWindow>> m_pending;
};
