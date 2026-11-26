/*******************************************************************************
 * gui/BackgammonSounds.cpp                                                    *
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

#include "BackgammonSounds.h"

#include "gui/SoundManager.h"
#include "gui/settings/rsharesettings.h"

#include <QApplication>
#include <QCoreApplication>
#include <QSoundEffect>
#include <QUrl>

namespace {
const QString Group = QStringLiteral("Backgammon");

struct EventInfo {
    const char *key;
    const char *file;
    const char *label;
};

// Order must match BackgammonSounds::Event.
const EventInfo Events[BackgammonSounds::EventCount] = {
    { "Invitation",   "double.wav",    QT_TRANSLATE_NOOP("BackgammonSounds", "Invitation received") },
    { "GameStart",    "take.wav",      QT_TRANSLATE_NOOP("BackgammonSounds", "Match starts") },
    { "Declined",     "drop.wav",      QT_TRANSLATE_NOOP("BackgammonSounds", "Invitation declined or opponent left") },
    { "Roll",         "roll.wav",      QT_TRANSLATE_NOOP("BackgammonSounds", "Dice rolled") },
    { "Checker",      "chequer.wav",   QT_TRANSLATE_NOOP("BackgammonSounds", "You move a checker") },
    { "OpponentMove", "move.wav",      QT_TRANSLATE_NOOP("BackgammonSounds", "Opponent plays a turn") },
    { "CannotMove",   "dance.wav",     QT_TRANSLATE_NOOP("BackgammonSounds", "No legal move (dance)") },
    { "Resign",       "resign.wav",    QT_TRANSLATE_NOOP("BackgammonSounds", "Resignation") },
    { "GameWon",      "gameover.wav",  QT_TRANSLATE_NOOP("BackgammonSounds", "Game won") },
    { "GameLost",     "gameover.wav",  QT_TRANSLATE_NOOP("BackgammonSounds", "Game lost") },
    { "MatchWon",     "fanfare.wav",   QT_TRANSLATE_NOOP("BackgammonSounds", "Match won") },
    { "MatchLost",    "matchover.wav", QT_TRANSLATE_NOOP("BackgammonSounds", "Match lost") },
    { "DoubleOffered", "double.wav",   QT_TRANSLATE_NOOP("BackgammonSounds", "Double offered") },
    { "DoubleTaken",  "take.wav",      QT_TRANSLATE_NOOP("BackgammonSounds", "Double taken") },
    { "DoubleDropped", "drop.wav",     QT_TRANSLATE_NOOP("BackgammonSounds", "Double dropped") },
};

QString eventKey(BackgammonSounds::Event event)
{
    return QStringLiteral("Sound%1").arg(QLatin1String(Events[event].key));
}

QSoundEffect *effects[BackgammonSounds::EventCount] = {};
}

namespace BackgammonSounds
{

QString label(Event event)
{
    return QCoreApplication::translate("BackgammonSounds", Events[event].label);
}

bool soundsEnabled()
{
    return Settings->valueFromGroup(Group, "SoundsEnabled", true).toBool();
}

void setSoundsEnabled(bool enabled)
{
    Settings->setValueToGroup(Group, "SoundsEnabled", enabled);
}

bool eventEnabled(Event event)
{
    return Settings->valueFromGroup(Group, eventKey(event), true).toBool();
}

void setEventEnabled(Event event, bool enabled)
{
    Settings->setValueToGroup(Group, eventKey(event), enabled);
}

int volume()
{
    return qBound(0, Settings->valueFromGroup(Group, "SoundVolume", 80).toInt(), 100);
}

void setVolume(int value)
{
    Settings->setValueToGroup(Group, "SoundVolume", qBound(0, value, 100));
}

void play(Event event)
{
    if (event < 0 || event >= EventCount)
        return;
    if (SoundManager::isMute() || !soundsEnabled() || !eventEnabled(event))
        return;
    preview(event);
}

void preview(Event event)
{
    if (event < 0 || event >= EventCount)
        return;

    QSoundEffect *&effect = effects[event];
    if (!effect) {
        // Owned by the application: effects live as long as RetroShare.
        effect = new QSoundEffect(qApp);
        effect->setSource(QUrl(QStringLiteral("qrc:/backgammon/sounds/%1")
                                   .arg(QLatin1String(Events[event].file))));
    }
    effect->setVolume(volume() / 100.0);
    effect->stop();
    effect->play();
}

}
