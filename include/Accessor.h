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
#include "Transport_NEW.h"
#include "Gateway.h"
#include "Logger.h"

#include <condition_variable>
#include <mutex>

namespace FireboltSDK::Transport {

    using OnConnectionChanged = std::function<void(const bool connected, const Firebolt::Error error)>;

    class FIREBOLTSDK_EXPORT Accessor {
    private:
        static constexpr uint8_t JSONVersion = 2;

    private:
        //Singleton
        Accessor(const std::string& configLine);

    public:

        Accessor(const Accessor&) = delete;
        Accessor& operator= (const Accessor&) = delete;
        Accessor() = delete;
        ~Accessor();

        static Accessor& Instance(const std::string& configLine = "")
        {
            static Accessor *instance = new Accessor(configLine);
            return *instance;
        }

        static void Dispose()
        {
            if (_singleton != nullptr) {
                delete _singleton;
                _singleton = nullptr;
            }
        }

        Firebolt::Error Connect(const OnConnectionChanged listener)
        {
            RegisterConnectionChangeListener(listener);
            Firebolt::Error status = CreateTransport(_config["WsUrl"], _config["WaitTime"]);
            if (status == Firebolt::Error::None) {
                Gateway::Instance().TransportUpdated(&_transport_pp);
            }
            _connectionChangeListener(true, Firebolt::Error::None);
            return status;
        }

        void RegisterConnectionChangeListener(const OnConnectionChanged listener)
        {
            _connectionChangeListener = listener;
        }

        void UnregisterConnnectionChangeListener()
        {
            _connectionChangeListener = nullptr;
        }

        Firebolt::Error Disconnect()
        {
            Gateway::Instance().TransportUpdated(nullptr);
            DestroyTransport();

            return Firebolt::Error::None;
        }

        bool IsConnected() const
        {
            return _connected;
        }

    private:
        Firebolt::Error CreateTransport(const std::string& url, const uint32_t waitTime);
        Firebolt::Error DestroyTransport();

        void ConnectionChanged(const bool connected, const Firebolt::Error error);

    private:
        static constexpr uint32_t DefaultWaitTime = 10000;

        Transport_PP _transport_pp;
        static Accessor* _singleton;
        nlohmann::json _config;

        bool _connected = false;
        OnConnectionChanged _connectionChangeListener = nullptr;
    };
}
