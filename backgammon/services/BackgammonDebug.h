/*******************************************************************************
 * services/BackgammonDebug.h                                                  *
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

// Runtime debug logging for the Backgammon plugin. Two check boxes in
// Preferences -> Backgammon -> Debug, each with its own log file in the
// RetroShare account folder:
//
//   Log tunnel activity     backgammon_tunnels.log   (BG_TLOG)
//       tunnel requests, status changes, packets sent and received (with
//       their size), reconnection attempts, tunnels closed
//
//   Log game activity       backgammon_activity.log  (BG_GAMELOG)
//       invitations sent, received, refused (and why), accepted, declined;
//       match start and end, dice, turns, doubles, resyncs
//
// The environment variable BACKGAMMON_DEBUG turns logging on for a session:
// "tunnel", "game", or "all" ("1" means "tunnel").
// Every line is also written to stderr with a timestamp and a
// [Backgammon/<category>] tag. Log files are rotated to "<name>.1" at 5 MB.
//
// A disabled category costs one atomic load per log call and builds no
// strings. Safe to use from any thread.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace BackgammonDebug
{

enum Category { Tunnel = 0, Game, CategoryCount };

inline const char *categoryName(int category)
{
    return category == Tunnel ? "tunnel" : "game";
}

inline const char *logFileName(int category)
{
    return category == Tunnel ? "backgammon_tunnels.log" : "backgammon_activity.log";
}

inline bool enabledFromEnvironment(int category)
{
    const char *env = std::getenv("BACKGAMMON_DEBUG");
    if (!env)
        return false;
    const std::string value(env);
    if (value == "all")
        return true;
    if (value == "1")
        return category == Tunnel;
    return value.find(categoryName(category)) != std::string::npos;
}

inline std::atomic<bool> &enabledFlag(int category)
{
    static std::atomic<bool> flags[CategoryCount] = {
        { enabledFromEnvironment(Tunnel) }, { enabledFromEnvironment(Game) }
    };
    return flags[category >= 0 && category < CategoryCount ? category : Tunnel];
}

inline bool enabled(int category)
{
    return enabledFlag(category).load(std::memory_order_relaxed);
}

inline std::mutex &fileMutex()
{
    static std::mutex mutex;
    return mutex;
}

// Folder of the log files (the RetroShare account folder); empty: stderr only.
inline std::string &logDirectory()
{
    static std::string directory;
    return directory;
}

inline void setLogDirectory(const std::string &directory)
{
    std::lock_guard<std::mutex> lock(fileMutex());
    logDirectory() = directory;
}

inline std::string logFilePath(int category)
{
    std::lock_guard<std::mutex> lock(fileMutex());
    return logDirectory().empty() ? std::string() : logDirectory() + "/" + logFileName(category);
}

inline std::string timestamp()
{
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t seconds = system_clock::to_time_t(now);
    const int millis = int(duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000);
    std::tm local {};
#ifdef _WIN32
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    char text[32];
    std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &local);
    char out[40];
    std::snprintf(out, sizeof(out), "%s.%03d", text, millis);
    return out;
}

inline void write(int category, const std::string &message)
{
    const std::string line = timestamp() + " [Backgammon/" + categoryName(category) + "] " + message;
    std::lock_guard<std::mutex> lock(fileMutex());
    std::cerr << line << std::endl;
    if (logDirectory().empty())
        return;
    const std::string path = logDirectory() + "/" + logFileName(category);
    {
        std::ifstream size(path, std::ios::binary | std::ios::ate);
        if (size && size.tellg() > std::streamoff(5 * 1024 * 1024)) {
            size.close();
            const std::string old = path + ".1";
            std::remove(old.c_str());
            std::rename(path.c_str(), old.c_str());
        }
    }
    std::ofstream file(path, std::ios::app);
    if (file)
        file << line << '\n';
}

inline void setEnabled(int category, bool on)
{
    if (category < 0 || category >= CategoryCount)
        return;
    if (enabledFlag(category).exchange(on) != on)
        write(category, std::string(categoryName(category)) + (on ? " logging enabled" : " logging disabled"));
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

// Name of a packet op (see the protocol in BackgammonService.h).
inline const char *opName(char op)
{
    switch (op) {
    case 'I': return "invite";
    case 'O': return "offered";
    case 'A': return "accept";
    case 'S': return "start";
    case 'D': return "dice";
    case 'T': return "turn";
    case 'P': return "state";
    case 'Q': return "state-reply";
    case 'C': return "double";
    case 'K': return "take";
    case 'N': return "drop";
    case 'R': return "resign";
    case 'X': return "leave";
    default:  return "?";
    }
}

} // namespace BackgammonDebug

#define BG_DLOG(category, expr) \
    do { \
        if (BackgammonDebug::enabled(category)) { \
            std::ostringstream bg_log_stream; \
            bg_log_stream << expr; \
            BackgammonDebug::write(category, bg_log_stream.str()); \
        } \
    } while (0)

#define BG_TLOG(expr)    BG_DLOG(BackgammonDebug::Tunnel, expr)
#define BG_GAMELOG(expr) BG_DLOG(BackgammonDebug::Game, expr)
