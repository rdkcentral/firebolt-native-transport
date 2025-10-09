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

#include "Accessor.h"
#include "Logger.h"
#include "Gateway.h"
#include "Transport.h"
#include "error.h"
#include "TypesPriv.h"
#include <nlohmann/json.hpp>
#include <chrono>

namespace FireboltSDK::Transport {

    Accessor* Accessor::_singleton = nullptr;

    Accessor::Accessor(const std::string& configLine)
    {
        _singleton = this;

        _config["WaitTime"] = Config::DefaultWaitTime;
        _config["LogLevel"] = "Info";
        _config["WsUrl"] = "ws://127.0.0.1:9998";
        _config["RPCv2"] = true;

        _config.merge_patch(nlohmann::json::parse(configLine));

        FireboltSDK::JSON::EnumType<Logger::LogLevel> logLevels = {
            { "Error", Logger::LogLevel::Error },
            { "Warning", Logger::LogLevel::Warning },
            { "Info", Logger::LogLevel::Info },
            { "Debug", Logger::LogLevel::Debug }
        };

        Logger::SetLogLevel(logLevels[_config["LogLevel"]]);
    }

    Accessor::~Accessor()
    {
        _singleton = nullptr;
    }

    Firebolt::Error Accessor::CreateTransport(const std::string& url)
    {
        _transport.Connect(url);

        return Firebolt::Error::None;
    }

    Firebolt::Error Accessor::DestroyTransport()
    {
        return Firebolt::Error::None;
    }

    void Accessor::ConnectionChanged(const bool connected, const Firebolt::Error error)
    {
        _connected = connected;
        if (_connectionChangeListener != nullptr) {
             _connectionChangeListener(connected, error);
        }
    }
}

