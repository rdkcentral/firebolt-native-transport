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
#include "Module.h"
#include "error.h"
#include <stdint.h>
#include <string>

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

        enum class Category : uint8_t {
            OpenRPC,
            Core,
            Manage,
            Discovery,
            PlayerProvider,
            PlayerManager,
        };

    public:
        Logger() = default;
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        ~Logger() = default;

    public:
        static Firebolt::Error SetLogLevel(LogLevel logLevel);
        static void Log(LogLevel logLevel, Category category, const std::string& module, const std::string file, const std::string function, const uint16_t line, const std::string& format, ...);

    public:
        template<typename CLASS>
        static const string Module()
        {
            return WPEFramework::Core::ClassNameOnly(typeid(CLASS).name()).Text(); 
        }

    private:
        static LogLevel _logLevel;
    };
}
#define FIREBOLT_LOG(level, category, module, ...) \
    do { FireboltSDK::Transport::Logger::Log(level, category, module, __FILE__, __func__, __LINE__, __VA_ARGS__); } while (0)

#define FIREBOLT_LOG_ERROR(category, module, ...) \
    do { FIREBOLT_LOG(FireboltSDK::Transport::Logger::LogLevel::Error, category, module, __VA_ARGS__); } while (0)
#define FIREBOLT_LOG_WARNING(category, module, ...) \
    do { FIREBOLT_LOG(FireboltSDK::Transport::Logger::LogLevel::Warning, category, module, __VA_ARGS__); } while (0)
#define FIREBOLT_LOG_INFO(category, module, ...) \
    do { FIREBOLT_LOG(FireboltSDK::Transport::Logger::LogLevel::Info, category, module, __VA_ARGS__); } while (0)
#define FIREBOLT_LOG_DEBUG(category, module, ...) \
    do { FIREBOLT_LOG(FireboltSDK::Transport::Logger::LogLevel::Debug, category, module, __VA_ARGS__); } while (0)

