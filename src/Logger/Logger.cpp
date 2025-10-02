/*
 * Copyright 2023 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "error.h"
#include "Logger.h"
#include <stdio.h>
#include <unistd.h>
#include <cstdarg>
#include <map>
#include <chrono>
#include <ctime>
#include <sys/time.h>
#include <cstring>
#include <sys/syscall.h>

#ifdef ENABLE_SYSLOG
#define LOG_MESSAGE(message) \
    do { syslog(sLOG_NOTIC, "%s", message); } while (0)
#else
#define LOG_MESSAGE(message) \
    do { fprintf(stderr, "%s", message); fflush(stdout); } while (0)
#endif

namespace FireboltSDK::Transport {
    /* static */  Logger::LogLevel Logger::_logLevel = Logger::LogLevel::Error;

std::map<FireboltSDK::Transport::Logger::LogLevel, const char*> _logLevelNames = {
    { FireboltSDK::Transport::Logger::LogLevel::Error, "Error" },
    { FireboltSDK::Transport::Logger::LogLevel::Warning, "Warning" },
    { FireboltSDK::Transport::Logger::LogLevel::Info, "Info" },
    { FireboltSDK::Transport::Logger::LogLevel::Debug, "Debug" },
};

    void Logger::SetLogLevel(Logger::LogLevel logLevel)
    {
        if (logLevel < Logger::LogLevel::MaxLevel) {
            _logLevel = logLevel;
        }
    }

    void Logger::Log(LogLevel logLevel, const std::string& module, const std::string file, const std::string function, const uint16_t line, const std::string& format, ...)
    {
        if (logLevel <= _logLevel) {
            va_list arg;
            char msg[Logger::MaxBufSize];
            va_start(arg, format);
            int length = vsnprintf(msg, Logger::MaxBufSize, format.c_str(), arg);
            va_end(arg);

            uint32_t position = (length >= Logger::MaxBufSize) ? (Logger::MaxBufSize - 1) : length;
            msg[position] = '\0';

            char formattedMsg[Logger::MaxBufSize];
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
            localtime_r(&t, &tm);
            char timeBuf[16];
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d.%03ld", tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<long>(ms.count()));
            const std::string time(timeBuf);
            const std::string levelName = _logLevelNames[logLevel];

            static bool colorSet     = false;
            static char colorOn[16]  = { 0 };
            static char colorOff[16] = { 0 };
            if (!colorSet) {
                colorSet = true;
                if (isatty(fileno(stderr)) == 1) {
                    strncpy(colorOn,  "\033[1;32m", 15);
                    strncpy(colorOff, "\033[0m",    15);
                }
            }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(formattedMsg, sizeof(formattedMsg),
            "%s%s: [%s][%s][%s:%d](%s)<PID:%u><TID:%u> : %s%s\n",
            colorOn, time.c_str(), levelName.c_str(), module.c_str(), file.c_str(), line, function.c_str(), ::getpid(), ::gettid(), msg, colorOff);
#pragma GCC diagnostic pop
            LOG_MESSAGE(formattedMsg);
        }
    }
}
