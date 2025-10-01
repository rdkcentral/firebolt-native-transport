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
#include "WorkerPool.h"
#include "Transport.h"
#include "Transport_NEW.h"
#include "Async.h"
#include "Gateway.h"
#include "Logger.h"

#include <condition_variable>
#include <mutex>

namespace FireboltSDK::Transport {
    class FIREBOLTSDK_EXPORT Accessor {
    private:
        static constexpr uint8_t JSONVersion = 2;

    private:
        //Singleton
        Accessor(const string& configLine);

    public:

        Accessor(const Accessor&) = delete;
        Accessor& operator= (const Accessor&) = delete;
        Accessor() = delete;
        ~Accessor();

        static Accessor& Instance(const string& configLine = "")
        {
            static Accessor *instance = new Accessor(configLine);
            ASSERT(instance != nullptr);
            return *instance;
        }

        static void Dispose()
        {
            ASSERT(_singleton != nullptr);

            if (_singleton != nullptr) {
                delete _singleton;
                _singleton = nullptr;
            }
        }

        Firebolt::Error Connect(const Transport<WPEFramework::Core::JSON::IElement>::Listener& listener)
        {
            RegisterConnectionChangeListener(listener);
            Firebolt::Error status = CreateTransport(_config["WsUrl"], _config["WaitTime"]);
            if (status == Firebolt::Error::None) {
                Async::Instance().Configure(_transport);
                Gateway::Instance().TransportUpdated(&_transport_pp);
            }
            return status;
        }

        void RegisterConnectionChangeListener(const Transport<WPEFramework::Core::JSON::IElement>::Listener& listener)
        {
            _connectionChangeListener = listener;
        }

        void UnregisterConnnectionChangeListener()
        {
            _connectionChangeListener = nullptr;
        }

        Firebolt::Error Disconnect()
        {
            if (_transport == nullptr) {
                return Firebolt::Error::None;
            }

            Async::Dispose();
            Gateway::Instance().TransportUpdated(nullptr);
            DestroyTransport();

            return Firebolt::Error::None;
        }

        bool IsConnected() const
        {
            return _connected;
        }

    private:
        Firebolt::Error CreateTransport(const string& url, const uint32_t waitTime);
        Firebolt::Error DestroyTransport();

        void ConnectionChanged(const bool connected, const Firebolt::Error error);

    private:
        static constexpr uint32_t DefaultWaitTime = 10000;

        WPEFramework::Core::ProxyType<WorkerPoolImplementation> _workerPool;
        Transport<WPEFramework::Core::JSON::IElement>* _transport;
        Transport_PP _transport_pp;
        static Accessor* _singleton;
        nlohmann::json _config;

        bool _connected = false;
        Transport<WPEFramework::Core::JSON::IElement>::Listener _connectionChangeListener = nullptr;
    };
}
