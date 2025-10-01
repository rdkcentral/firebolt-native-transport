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

#include <chrono>

namespace FireboltSDK::Transport {

    Accessor* Accessor::_singleton = nullptr;

    Accessor::Accessor(const string& configLine)
        : _workerPool()
        , _transport(nullptr)
    {
        ASSERT(_singleton == nullptr);
        _singleton = this;

        _config["WaitTime"] = DefaultWaitTime;
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

        FIREBOLT_LOG_INFO(Logger::Category::OpenRPC, Logger::Module<Accessor>(), "Url = %s", _config["WsUrl"].get<std::string>().c_str());
    }

    Accessor::~Accessor()
    {
        WPEFramework::Core::IWorkerPool::Assign(nullptr);
        _workerPool->Stop();

        ASSERT(_singleton != nullptr);
        _singleton = nullptr;
    }

    Firebolt::Error Accessor::CreateTransport(const string& url, const uint32_t waitTime = DefaultWaitTime)
    {
        _transport_pp.Connect(url);

        if (_transport != nullptr) {
            delete _transport;
        }

        _transport = new Transport<WPEFramework::Core::JSON::IElement>(
                static_cast<WPEFramework::Core::URL>(url),
                waitTime,
                std::bind(&Accessor::ConnectionChanged, this, std::placeholders::_1, std::placeholders::_2));

        ASSERT(_transport != nullptr);
        return ((_transport != nullptr) ? Firebolt::Error::None : Firebolt::Error::Timedout);
    }

    Firebolt::Error Accessor::DestroyTransport()
    {
        if (_transport != nullptr) {
            delete _transport;
            _transport = nullptr;
        }
        return Firebolt::Error::None;
    }

    void Accessor::ConnectionChanged(const bool connected, const Firebolt::Error error)
    {
        _connected = connected;
        if (_connectionChangeListener != nullptr) { // Notify a listener about the connection change
             _connectionChangeListener(connected, error);
        }
    }
}

