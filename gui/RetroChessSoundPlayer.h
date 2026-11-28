/*******************************************************************************
 * gui/RetroChessSoundPlayer.h                                                 *
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

#ifndef RETROCHESSSOUNDPLAYER_H
#define RETROCHESSSOUNDPLAYER_H

#include <QMediaPlayer>
#include <QUrl>

// A QMediaPlayer that drops its media before it is destroyed. Every sound of
// the plugin is played through this class.
//
// The GStreamer backend of Qt 6.4 (the Qt of Ubuntu 24.04 and Debian 12) keeps
// the decoder's "pad-removed" handler connected while ~QGstreamerMediaPlayer
// destroys its members. When the decoder releases a pad after the backend's
// pad map is gone, the handler reads freed memory and the GUI crashes in
// QGstreamerMediaPlayer::decoderPadRemoved(). Clearing the source first
// removes the decoder while the backend is still whole, so its destructor has
// no decoder left to tear down. Qt 6.8 disconnects the handler itself; there
// clearing the source costs nothing.
//
// disconnect() comes first, as in ~QMediaPlayer: clearing the source emits
// playbackStateChanged(), mediaStatusChanged() and sourceChanged(), and the
// owner of the player may already be half destroyed (a QWidget deletes its
// children before its own connections are cut).
class RetroChessSoundPlayer : public QMediaPlayer
{
public:
	explicit RetroChessSoundPlayer(QObject *parent = nullptr) : QMediaPlayer(parent) {}

	~RetroChessSoundPlayer() override
	{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		disconnect();
		setSource(QUrl());
#endif
	}
};

#endif // RETROCHESSSOUNDPLAYER_H
