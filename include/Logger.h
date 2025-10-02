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

#pragma once

#include "Portability.h"
#include "error.h"
#include <stdint.h>
#include <string>
#include <typeinfo>

namespace FireboltSDK::Transport {

    class FIREBOLTSDK_EXPORT Logger {
    private:
        static constexpr uint16_t MaxBufSize = 512;

    public:
        enum class LogLevel : uint8_t {
            Error,
            Warning,
            Info,
            Debug,
            MaxLevel
        };

    public:
        Logger() = default;
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        ~Logger() = default;

    public:
        static void SetLogLevel(LogLevel logLevel);
        static void Log(LogLevel logLevel, const std::string& module, const std::string file, const std::string function, const uint16_t line, const std::string& format, ...);

    public:
        template<typename CLASS>
        static const std::string Module()
        {
            std::string name = typeid(CLASS).name();
            size_t last_colon = name.find_last_of(':');
            if (last_colon != std::string::npos) {
                name = name.substr(last_colon + 1);
            }
            size_t less_than = name.find('<');
            if (less_than != std::string::npos) {
                name = name.substr(0, less_than);
            }
            return name;
        }

    private:
        static LogLevel _logLevel;
    };
}
#define FIREBOLT_LOG(level, module, ...) \
    do { FireboltSDK::Transport::Logger::Log(level, module, __FILE__, __func__, __LINE__, __VA_ARGS__); } while (0)

#define FIREBOLT_LOG_ERROR(module, ...) \
    do { FIREBOLT_LOG(FireboltSDK::Transport::Logger::LogLevel::Error, module, __VA_ARGS__); } while (0)
#define FIREBOLT_LOG_WARNING(module, ...) \
    do { FIREBOLT_LOG(FireboltSDK::Transport::Logger::LogLevel::Warning, module, __VA_ARGS__); } while (0)
#define FIREBOLT_LOG_INFO(module, ...) \
    do { FIREBOLT_LOG(FireboltSDK::Transport::Logger::LogLevel::Info, module, __VA_ARGS__); } while (0)
#define FIREBOLT_LOG_DEBUG(module, ...) \
    do { FIREBOLT_LOG(FireboltSDK::Transport::Logger::LogLevel::Debug, module, __VA_ARGS__); } while (0)

