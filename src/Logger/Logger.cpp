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
#include <filesystem>

#ifdef ENABLE_SYSLOG
#define LOG_MESSAGE(message) \
    do { syslog(sLOG_NOTIC, "%s", message); } while (0)
#else
#define LOG_MESSAGE(message) \
    do { fprintf(stderr, "%s", message); fflush(stdout); } while (0)
#endif

namespace WPEFramework {

ENUM_CONVERSION_BEGIN(FireboltSDK::Helpers::Logger::LogLevel)

    { FireboltSDK::Helpers::Logger::LogLevel::Error, _TXT("Error") },
    { FireboltSDK::Helpers::Logger::LogLevel::Warning, _TXT("Warning") },
    { FireboltSDK::Helpers::Logger::LogLevel::Info, _TXT("Info") },
    { FireboltSDK::Helpers::Logger::LogLevel::Debug, _TXT("Debug") },

ENUM_CONVERSION_END(FireboltSDK::Helpers::Logger::LogLevel)

}

namespace FireboltSDK::Helpers {
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
            const string time = WPEFramework::Core::Time::Now().ToTimeOnly(true);
            const string levelName =     WPEFramework::Core::EnumerateType<Logger::LogLevel>(logLevel).Data();

            static char colorErr[16] = { 0 };
            static char colorWrn[16] = { 0 };
            static char colorInf[16] = { 0 };
            static char colorDbg[16] = { 0 };
            static char colorOff[16] = { 0 };
            static bool colorSet     = false;
            if (!colorSet) {
                colorSet = true;
                if (isatty(fileno(stderr)) == 1) {
                    strncpy(colorErr, "\033[0;31m", 15); // red
                    strncpy(colorWrn, "\033[0;33m", 15); // yellow
                    strncpy(colorInf, "\033[0;39m", 15); // default terminal colour
                    strncpy(colorDbg, "\033[0;90m", 15); // bright-black -> gray
                    strncpy(colorOff, "\033[0m",    15);
                }
            }
            char *color;
            switch (logLevel) {
                case Logger::LogLevel::Error:   color = colorErr; break;
                case Logger::LogLevel::Warning: color = colorWrn; break;
                case Logger::LogLevel::Info:    color = colorInf; break;
                case Logger::LogLevel::Debug:   color = colorDbg; break;
                default:                        color = colorInf; break;
            }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(formattedMsg, sizeof(formattedMsg), "%s: [%s%s%s][%s][%s(), %s:%d]<pid:%d, tid:%ld>: %s\n", time.c_str(), color, levelName.c_str(), colorOff, module.c_str(), function.c_str(), std::filesystem::path(file).filename().c_str(), line, TRACE_PROCESS_ID, TRACE_THREAD_ID, msg);
#pragma GCC diagnostic pop
            LOG_MESSAGE(formattedMsg);
        }
    }
}

