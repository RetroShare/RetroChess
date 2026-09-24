/*******************************************************************************
 * services/RetroChessTunnelDebug.h                                            *
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

#ifndef RETROCHESS_TUNNEL_DEBUG_H
#define RETROCHESS_TUNNEL_DEBUG_H

// Runtime debug logging for RetroChess. Two checkboxes in RetroChess
// settings -> General -> Debug, each with its own log file in the RetroShare
// account directory:
//
//   Log tunnel activity   retrochess_tunnels.log   (CHESS_TLOG)
//       tunnel requests, status changes, closes, packets, presence
//       probes/backoff, CONFIG save/load, tunnel overview every minute
//
//   Log chess activity    retrochess_activity.log  - all categories below,
//                         each line tagged with its category:
//       [RetroChess/leaderboard] (CHESS_LBLOG)    results accepted/rejected/
//           waiting for witnesses, relays, sync requests and answers
//       [RetroChess/invite]      (CHESS_INVLOG)   invitations sent/queued/
//           received, accept, reject, cancel, busy, seeks, toaster
//       [RetroChess/game]        (CHESS_GAMELOG)  game start/end, moves,
//           resend queue, resign/draw/abort/timeout, desynchronization
//       [RetroChess/spectate]    (CHESS_SPECLOG)  watch requests, watch state,
//           relayed moves, spectators joining/leaving
//       [RetroChess/storage]     (CHESS_STORELOG) leaderboard file loaded/
//           saved and file errors
//
// Environment variable RETROCHESS_DEBUG: "1" enables the tunnel log as
// before, "activity" the chess activity log, "all" both. A comma separated
// list of single categories also works, e.g. RETROCHESS_DEBUG=tunnel,game.
// Every line is also written with a timestamp and a [RetroChess/<category>]
// tag to stderr.
//
// When a category is disabled its log macro costs one atomic load and builds
// no strings.

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#include <sys/time.h>

namespace RetroChessTunnelDebug
{
// Log file is rotated to "<name>.1" when it grows beyond this size.
static const std::streamoff kMaxLogFileSize = 5 * 1024 * 1024;

enum Category {
	Tunnel = 0,
	Leaderboard,
	Invite,
	Game,
	Spectate,
	Storage,
	CategoryCount
};

// Name used in log lines, settings keys and RETROCHESS_DEBUG.
inline const char *categoryName(int category)
{
	switch (category) {
	case Tunnel:      return "tunnel";
	case Leaderboard: return "leaderboard";
	case Invite:      return "invite";
	case Game:        return "game";
	case Spectate:    return "spectate";
	case Storage:     return "storage";
	default:          return "?";
	}
}

inline bool enabledFromEnvironment(int category)
{
	const char *env = std::getenv("RETROCHESS_DEBUG");
	if (!env) return false;
	const std::string value(env);
	if (value.empty() || value == "0") return false;
	if (value == "all") return true;
	if (value == "1") return category == Tunnel; // unchanged meaning of RETROCHESS_DEBUG=1
	if (value == "activity") return category != Tunnel;
	const std::string name = categoryName(category);
	size_t start = 0;
	while (start <= value.size()) {
		size_t end = value.find(',', start);
		if (end == std::string::npos) end = value.size();
		if (value.compare(start, end - start, name) == 0) return true;
		start = end + 1;
	}
	return false;
}

inline std::atomic<bool> &enabledFlag(int category = Tunnel)
{
	static std::atomic<bool> flags[CategoryCount] = {
		{enabledFromEnvironment(Tunnel)},   {enabledFromEnvironment(Leaderboard)},
		{enabledFromEnvironment(Invite)},   {enabledFromEnvironment(Game)},
		{enabledFromEnvironment(Spectate)}, {enabledFromEnvironment(Storage)}
	};
	if (category < 0 || category >= CategoryCount) category = Tunnel;
	return flags[category];
}

inline std::mutex &fileMutex()
{
	static std::mutex mutex;
	return mutex;
}

// Log file: tunnel has its own, all other categories share the activity log.
inline const char *logFileName(int category)
{
	return category == Tunnel ? "retrochess_tunnels.log" : "retrochess_activity.log";
}

// Account directory the log files are written to (empty: stderr only).
inline std::string &logDirectory()
{
	static std::string dir;
	return dir;
}

inline bool enabled(int category = Tunnel)
{ return enabledFlag(category).load(std::memory_order_relaxed); }

inline bool anyEnabled()
{
	for (int c = 0; c < CategoryCount; ++c)
		if (enabled(c)) return true;
	return false;
}

inline void setLogDirectory(const std::string &dir)
{
	std::lock_guard<std::mutex> lock(fileMutex());
	logDirectory() = dir;
}

inline void setLogDirectoryIfUnset(const std::string &dir)
{
	std::lock_guard<std::mutex> lock(fileMutex());
	if (logDirectory().empty() && !dir.empty()) logDirectory() = dir;
}

inline std::string timestamp()
{
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	const time_t secs = tv.tv_sec;
	struct tm local;
#ifdef _WIN32
	local = *localtime(&secs);   // per-thread buffer on Windows
#else
	localtime_r(&secs, &local);
#endif
	char buf[32];
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local);
	char out[48];
	snprintf(out, sizeof(out), "%s.%03d", buf, static_cast<int>(tv.tv_usec / 1000));
	return out;
}

inline void write(int category, const std::string &message)
{
	const std::string line = timestamp() + " [RetroChess/" + categoryName(category) + "] " + message;
	std::lock_guard<std::mutex> lock(fileMutex());
	std::cerr << line << std::endl;
	if (logDirectory().empty()) return;
	const std::string path = logDirectory() + "/" + logFileName(category);
	{
		std::ifstream size(path, std::ios::binary | std::ios::ate);
		if (size && size.tellg() > kMaxLogFileSize) {
			size.close();
			const std::string old = path + ".1";
			std::remove(old.c_str());
			std::rename(path.c_str(), old.c_str());
		}
	}
	std::ofstream file(path, std::ios::app);
	if (file) file << line << '\n';
}

inline void write(const std::string &message) { write(Tunnel, message); }

inline void setEnabled(int category, bool on)
{
	if (category < 0 || category >= CategoryCount) return;
	const bool was = enabledFlag(category).exchange(on);
	if (on != was)
		write(category, std::string(categoryName(category))
		      + (on ? " debug logging ENABLED" : " debug logging DISABLED"));
}

inline void setEnabled(bool on) { setEnabled(Tunnel, on); }

inline const char *statusName(uint32_t status)
{
	switch (status) {
	case 0: return "UNKNOWN";
	case 1: return "TUNNEL_DN";
	case 2: return "CAN_TALK";
	case 3: return "REMOTELY_CLOSED";
	default: return "?";
	}
}

// Short description of a tunnel payload: JSON "type" field, or "raw".
inline std::string payloadType(const uint8_t *data, uint32_t size)
{
	const std::string text(reinterpret_cast<const char *>(data), size < 256 ? size : 256);
	const std::string key = "\"type\":\"";
	const size_t start = text.find(key);
	if (start == std::string::npos) return "raw";
	const size_t begin = start + key.size();
	const size_t end = text.find('"', begin);
	if (end == std::string::npos) return "raw";
	return text.substr(begin, end - begin);
}
} // namespace RetroChessTunnelDebug

#define CHESS_DLOG(category, expr) \
	do { \
		if (RetroChessTunnelDebug::enabled(category)) { \
			std::ostringstream chess_tlog_stream; \
			chess_tlog_stream << expr; \
			RetroChessTunnelDebug::write(category, chess_tlog_stream.str()); \
		} \
	} while (0)

#define CHESS_TLOG(expr)      CHESS_DLOG(RetroChessTunnelDebug::Tunnel, expr)
#define CHESS_LBLOG(expr)     CHESS_DLOG(RetroChessTunnelDebug::Leaderboard, expr)
#define CHESS_INVLOG(expr)    CHESS_DLOG(RetroChessTunnelDebug::Invite, expr)
#define CHESS_GAMELOG(expr)   CHESS_DLOG(RetroChessTunnelDebug::Game, expr)
#define CHESS_SPECLOG(expr)   CHESS_DLOG(RetroChessTunnelDebug::Spectate, expr)
#define CHESS_STORELOG(expr)  CHESS_DLOG(RetroChessTunnelDebug::Storage, expr)

#endif // RETROCHESS_TUNNEL_DEBUG_H
