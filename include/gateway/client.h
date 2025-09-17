/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 Sky UK
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
 */

#pragma once

#ifndef MODULE_NAME
#define MODULE_NAME OpenRPCNativeSDK
#endif
#include <core/core.h>
#include "error.h"

#include "Transport.h"
#include "Transport_NEW.h"

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <nlohmann/json.hpp>

#include "gateway/common.h"

namespace FireboltSDK::Transport
{
class Client
{
    struct Caller
    {
        Caller(MessageID id_)
            : id(id_)
            , timestamp(std::chrono::steady_clock::now())
        {}

        MessageID id;
        Timestamp timestamp;
        std::string response;
        Firebolt::Error error = Firebolt::Error::None;
        bool ready = false;
        std::mutex mtx;
        std::condition_variable waiter;
    };

    std::map <MessageID, std::shared_ptr<Caller>> queue;
    mutable std::mutex queue_mtx;
    Transport<WPEFramework::Core::JSON::IElement>* transport;
    Config config;
    Transport_PP* transport_pp = nullptr;

    std::atomic<bool> running { false };
    std::thread watchdogThread;

    void watchdog()
    {
        auto watchdogTimer = std::chrono::milliseconds(config.watchdogCycle_ms);
        std::vector<std::shared_ptr<Caller>> outdated;

        while (running) {
            Timestamp now = std::chrono::steady_clock::now();
            {
                std::lock_guard lck(queue_mtx);
                for (auto it = queue.begin(); it != queue.end();) {
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second->timestamp).count() > config.watchdogThreshold_ms) {
                        outdated.push_back(it->second);
                        it = queue.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            for (auto &c : outdated) {
                std::unique_lock<std::mutex> lk(c->mtx);
                std::cout << "Watchdog : message-id: " << c->id << " - timed out" << std::endl;
                c->ready = true;
                c->error = Firebolt::Error::Timedout;
                c->waiter.notify_one();
            }
            outdated.clear();
            std::this_thread::sleep_for(watchdogTimer);
        }
    }

public:
    Client(const Config &config_)
      : config(config_)
    {
        running = true;
        watchdogThread = std::thread(std::bind(&Client::watchdog, this));
    }

    void SetTransport(Transport<WPEFramework::Core::JSON::IElement>* transport, Transport_PP* transportPP)
    {
        this->transport = transport;
        this->transport_pp = transportPP;
    }

    virtual ~Client()
    {
        running = false;
        if (watchdogThread.joinable()) {
            watchdogThread.join();
        }
    }

#ifdef UNIT_TEST
    template <typename RESPONSE>
    Firebolt::Error Request(const std::string &method, const nlohmann::json &parameters, RESPONSE &response)
    {
        return transport->Invoke(method, parameters, response);
    }
    template <typename RESPONSE>
    Firebolt::Error Request(const std::string &method, const JsonObject &parameters, RESPONSE &response)
    {
        return transport->Invoke(method, parameters, response);
    }
#else
    template <typename RESPONSE>
    Firebolt::Error Request(const std::string &method, const nlohmann::json &parameters, RESPONSE &response)
    {
        if (transport_pp == nullptr) {
            return Firebolt::Error::NotConnected;
        }
        MessageID id = transport_pp->GetNextMessageID();
        std::shared_ptr<Caller> c = std::make_shared<Caller>(id);
        {
            std::lock_guard lck(queue_mtx);
            queue[id] = c;
        }

        printf("TB] II using PP\n");
        Firebolt::Error result = transport_pp->Send(method, parameters, id);
        if (result == Firebolt::Error::None) {
            {
                std::unique_lock<std::mutex> lk(c->mtx);
                c->waiter.wait(lk, [&]{ return c->ready; });
                {
                    std::lock_guard lck(queue_mtx);
                    queue.erase(c->id);
                }
            }
            if (c->error == Firebolt::Error::None) {
                response.FromString(c->response);
            } else {
                result = c->error;
            }
        }

        return result;
    }
    template <typename RESPONSE>
    Firebolt::Error Request(const std::string &method, const JsonObject &parameters, RESPONSE &response)
    {
        printf("TB] II using OR\n");
        if (transport == nullptr) {
            return Firebolt::Error::NotConnected;
        }
        MessageID id = transport->GetNextMessageID();
        std::shared_ptr<Caller> c = std::make_shared<Caller>(id);
        {
            std::lock_guard lck(queue_mtx);
            queue[id] = c;
        }

        Firebolt::Error result = transport->Send(method, parameters, id);
        if (result == Firebolt::Error::None) {
            {
                std::unique_lock<std::mutex> lk(c->mtx);
                c->waiter.wait(lk, [&]{ return c->ready; });
                {
                    std::lock_guard lck(queue_mtx);
                    queue.erase(c->id);
                }
            }
            if (c->error == Firebolt::Error::None) {
                response.FromString(c->response);
            } else {
                result = c->error;
            }
        }

        return result;
    }
#endif

    bool IdRequested(MessageID id)
    {
        std::lock_guard lck(queue_mtx);
        return queue.find(id) != queue.end();
    }

    void Response(const nlohmann::json &message)
    {
        MessageID id = message["id"];
        try {
            std::lock_guard lck(queue_mtx);
            auto c = queue.at(id);
            std::unique_lock<std::mutex> lk(c->mtx);

            if (!message.contains("error")) {
                c->response = to_string(message["result"]);
            } else {
                c->error = static_cast<Firebolt::Error>(message["error"]["code"]);
            }
            c->ready = true;
            c->waiter.notify_one();
        } catch (const std::out_of_range &e) {
            std::cout << "No receiver for message-id: " << id << std::endl;
        }
    }

    void Response(const WPEFramework::Core::JSONRPC::Message& message)
    {
        MessageID id = message.Id.Value();
        try {
            std::lock_guard lck(queue_mtx);
            auto c = queue.at(id);
            std::unique_lock<std::mutex> lk(c->mtx);

            if (!message.Error.IsSet()) {
                c->response = message.Result.Value();
            } else {
                c->error = static_cast<Firebolt::Error>(message.Error.Code.Value());
            }
            c->ready = true;
            c->waiter.notify_one();
        } catch (const std::out_of_range &e) {
            std::cout << "No receiver for message-id: " << id << std::endl;
        }
    }
};
} // namespace Firebolt::Transport
