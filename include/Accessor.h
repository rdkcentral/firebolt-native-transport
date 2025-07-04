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
#include "Async.h"
#include "Event.h"
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
        class EXTERNAL Config : public WPEFramework::Core::JSON::Container {
        public:
            Config(const Config&) = delete;
            Config& operator=(const Config&) = delete;

            class WorkerPoolConfig : public WPEFramework::Core::JSON::Container {
                public:
                    WorkerPoolConfig& operator=(const WorkerPoolConfig&);

                    WorkerPoolConfig()
                        : WPEFramework::Core::JSON::Container()
                        , QueueSize(8)
                        , ThreadCount(3)
                        , StackSize(WPEFramework::Core::Thread::DefaultStackSize())
                    {
                        Add("queueSize", &QueueSize);
                        Add("threadCount", &ThreadCount);
                        Add("stackSize", &StackSize);
                    }

                    virtual ~WorkerPoolConfig() = default;

                public:
                    WPEFramework::Core::JSON::DecUInt32 QueueSize;
                    WPEFramework::Core::JSON::DecUInt32 ThreadCount;
                    WPEFramework::Core::JSON::DecUInt32 StackSize;
                };


            Config()
                : WPEFramework::Core::JSON::Container()
                , WaitTime(1000)
                , LogLevel(_T("Info"))
                , WorkerPool()
                , WsUrl(_T("ws://127.0.0.1:9998"))
            {
                Add(_T("waitTime"), &WaitTime);
                Add(_T("logLevel"), &LogLevel);
                Add(_T("workerPool"), &WorkerPool);
                Add(_T("wsUrl"), &WsUrl);
            }

        public:
            WPEFramework::Core::JSON::DecUInt32 WaitTime;
            WPEFramework::Core::JSON::String LogLevel;
            WorkerPoolConfig WorkerPool;
            WPEFramework::Core::JSON::String WsUrl;
        };

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
            }
        }

        Firebolt::Error Connect(const Transport<WPEFramework::Core::JSON::IElement>::Listener& listener)
        {
            RegisterConnectionChangeListener(listener);
            Firebolt::Error status = CreateTransport(_config.WsUrl.Value().c_str(), _config.WaitTime.Value());
            if (status == Firebolt::Error::None) {
                Async::Instance().Configure(_transport);
                status = CreateEventHandler();
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
            Firebolt::Error status = Firebolt::Error::None;
            status = DestroyTransport();
            if (status == Firebolt::Error::None) {
                Async::Dispose();
                status = DestroyEventHandler();
            }
            return status;
        }

        bool IsConnected() const
        {
            return _connected;
        }

        Event& GetEventManager();
        Transport<WPEFramework::Core::JSON::IElement>* GetTransport();

    private:
        Firebolt::Error CreateEventHandler();
        Firebolt::Error DestroyEventHandler();
        Firebolt::Error CreateTransport(const string& url, const uint32_t waitTime);
        Firebolt::Error DestroyTransport();

        void ConnectionChanged(const bool connected, const Firebolt::Error error);

    private:
        WPEFramework::Core::ProxyType<WorkerPoolImplementation> _workerPool;
        Transport<WPEFramework::Core::JSON::IElement>* _transport;
        static Accessor* _singleton;
        Config _config;
        struct {
            std::mutex m;
            std::condition_variable cv;
            bool ready = false;
            void reset() {
                std::lock_guard lk(m);
                ready = false;
            }
            bool wait_for(unsigned duration_ms) {
                std::unique_lock lk(m);
                bool ret = cv.wait_for(lk, std::chrono::milliseconds(duration_ms), [&]{ return ready; });
                lk.unlock();
                return ret;
            }
            void signal() {
                std::lock_guard lk(m);
                ready = true;
                cv.notify_one();
            }
        } _connectionChangeSync; // Synchronize a thread that is waiting for a connection if that one that is notified about connection changes

        bool _connected = false;
        Transport<WPEFramework::Core::JSON::IElement>::Listener _connectionChangeListener = nullptr;
    };
}
