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

#include "Module.h"
#include "error.h"
#include "Logger.h"
#include <stdio.h>
#include <unistd.h>

#ifdef ENABLE_SYSLOG
#define LOG_MESSAGE(message) \
    do { syslog(sLOG_NOTIC, "%s", message); } while (0)
#else
#define LOG_MESSAGE(message) \
    do { fprintf(stderr, "%s", message); fflush(stdout); } while (0)
#endif

namespace WPEFramework {

ENUM_CONVERSION_BEGIN(FireboltSDK::Transport::Logger::LogLevel)

    { FireboltSDK::Transport::Logger::LogLevel::Error, _TXT("Error") },
    { FireboltSDK::Transport::Logger::LogLevel::Warning, _TXT("Warning") },
    { FireboltSDK::Transport::Logger::LogLevel::Info, _TXT("Info") },
    { FireboltSDK::Transport::Logger::LogLevel::Debug, _TXT("Debug") },

ENUM_CONVERSION_END(FireboltSDK::Transport::Logger::LogLevel)

ENUM_CONVERSION_BEGIN(FireboltSDK::Transport::Logger::Category)

    { FireboltSDK::Transport::Logger::Category::OpenRPC, _TXT("FireboltSDK::OpenRPC") },
    { FireboltSDK::Transport::Logger::Category::Core, _TXT("FireboltSDK::Core") },
    { FireboltSDK::Transport::Logger::Category::Manage, _TXT("FireboltSDK::Manage") },
    { FireboltSDK::Transport::Logger::Category::Discovery, _TXT("FireboltSDK::Discovery") },
    { FireboltSDK::Transport::Logger::Category::PlayerProvider, _TXT("FireboltSDK::PlayerProvider") },
    { FireboltSDK::Transport::Logger::Category::PlayerProvider, _TXT("FireboltSDK::PlayerManager") },

ENUM_CONVERSION_END(FireboltSDK::Transport::Logger::Category)

}

namespace FireboltSDK::Transport {
    /* static */  Logger::LogLevel Logger::_logLevel = Logger::LogLevel::Error;

    Firebolt::Error Logger::SetLogLevel(Logger::LogLevel logLevel)
    {
        ASSERT(logLevel < Logger::LogLevel::MaxLevel);
        Firebolt::Error status = Firebolt::Error::General;
        if (logLevel < Logger::LogLevel::MaxLevel) {
            _logLevel = logLevel;
            status = Firebolt::Error::None;
        }
        return status;
    }

    void Logger::Log(LogLevel logLevel, Category category, const std::string& module, const std::string file, const std::string function, const uint16_t line, const std::string& format, ...)
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
            const string time = WPEFramework::Core::Time::Now().ToTimeOnly(true);
            const string categoryName =  WPEFramework::Core::EnumerateType<Logger::Category>(category).Data();
            const string levelName =     WPEFramework::Core::EnumerateType<Logger::LogLevel>(logLevel).Data();

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
            if (categoryName.empty() != true) {
                snprintf(formattedMsg, sizeof(formattedMsg), "%s%s: [%s][%s]:[%s][%s:%d](%s)<PID:%d><TID:%ld> : %s%s\n", colorOn, time.c_str(), levelName.c_str(), categoryName.c_str(), module.c_str(), WPEFramework::Core::File::FileName(file).c_str(), line, function.c_str(), TRACE_PROCESS_ID, TRACE_THREAD_ID, msg, colorOff);
            } else {
                snprintf(formattedMsg, sizeof(formattedMsg), "%s%s: [%s][%s][%s:%d](%s)<PID:%d><TID:%ld> : %s%s\n", colorOn, time.c_str(), levelName.c_str(), module.c_str(), WPEFramework::Core::File::FileName(file).c_str(), line, function.c_str(), TRACE_PROCESS_ID, TRACE_THREAD_ID, msg, colorOff);
            }
#pragma GCC diagnostic pop
            LOG_MESSAGE(formattedMsg);
        }
    }
}

