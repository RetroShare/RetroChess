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

// Runtime tunnel tracing for RetroChess.
//
// Enable it with the "Log tunnel activity" option in the RetroChess settings,
// or by starting RetroShare with the environment variable RETROCHESS_DEBUG=1.
// Every tunnel request, status change, close, sent and received packet is then
// written with a timestamp to stderr and to
//     <RetroShare account directory>/retrochess_tunnels.log
// together with a periodic dump of all tunnels RetroChess knows about.
//
// When disabled, CHESS_TLOG() costs one atomic load and builds no strings.

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

inline std::atomic<bool> &enabledFlag()
{
	static std::atomic<bool> flag(std::getenv("RETROCHESS_DEBUG") != nullptr
	        && std::string(std::getenv("RETROCHESS_DEBUG")) != "0");
	return flag;
}

inline std::mutex &fileMutex()
{
	static std::mutex mutex;
	return mutex;
}

inline std::string &logFilePath()
{
	static std::string path;
	return path;
}

inline bool enabled() { return enabledFlag().load(std::memory_order_relaxed); }

inline void setLogDirectory(const std::string &dir)
{
	std::lock_guard<std::mutex> lock(fileMutex());
	logFilePath() = dir.empty() ? std::string() : dir + "/retrochess_tunnels.log";
}

inline void setLogDirectoryIfUnset(const std::string &dir)
{
	std::lock_guard<std::mutex> lock(fileMutex());
	if (logFilePath().empty() && !dir.empty()) logFilePath() = dir + "/retrochess_tunnels.log";
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

inline void write(const std::string &message)
{
	const std::string line = timestamp() + " [RetroChess/tunnel] " + message;
	std::lock_guard<std::mutex> lock(fileMutex());
	std::cerr << line << std::endl;
	const std::string &path = logFilePath();
	if (path.empty()) return;
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

inline void setEnabled(bool on)
{
	const bool was = enabledFlag().exchange(on);
	if (on != was) write(on ? "tunnel debug logging ENABLED" : "tunnel debug logging DISABLED");
}

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

#define CHESS_TLOG(expr) \
	do { \
		if (RetroChessTunnelDebug::enabled()) { \
			std::ostringstream chess_tlog_stream; \
			chess_tlog_stream << expr; \
			RetroChessTunnelDebug::write(chess_tlog_stream.str()); \
		} \
	} while (0)

#endif // RETROCHESS_TUNNEL_DEBUG_H
