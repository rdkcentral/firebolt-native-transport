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

#include "gateway.h"
#include "logger.h"
#include "transport.h"
#include "types/fb-errors.h"
#include "types/json_types.h"
#include <assert.h>
#include <chrono>
#include <condition_variable>
#include <list>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <thread>

namespace FireboltSDK::Transport
{

// Runtime configuration used by client/server watchdog and provider wait
static unsigned runtime_waitTime_ms = 3000;
static unsigned runtime_watchdogCycle_ms = 500;
static int runtime_providerWaitTime = -1;

using Timestamp = std::chrono::time_point<std::chrono::steady_clock>;
using MessageID = uint32_t;

IGateway::~IGateway() = default;

class IClientTransport
{
public:
    virtual ~IClientTransport() = default;
    virtual MessageID GetNextMessageID() = 0;
    virtual Firebolt::Error Send(const std::string &method, const nlohmann::json &parameters, MessageID id) = 0;
};

class IServerTransport
{
public:
    virtual ~IServerTransport() = default;
#ifdef ENABLE_MANAGE_API
    virtual void SendResponse(unsigned id, const std::string &response) = 0;
#endif
};

class Client
{
    struct Caller
    {
        Caller(MessageID id_) : id(id_), timestamp(std::chrono::steady_clock::now()) {}

        MessageID id;
        Timestamp timestamp;
        std::string response;
        nlohmann::json response_json;
        Firebolt::Error error = Firebolt::Error::None;
        bool ready = false;
        std::mutex mtx;
        std::condition_variable waiter;
    };

    std::map<MessageID, std::shared_ptr<Caller>> queue;
    mutable std::mutex queue_mtx;

    std::atomic<bool> running{false};
    std::thread watchdogThread;

    IClientTransport &transport_;

public:
    Client(IClientTransport &transport) : transport_(transport) {}

    virtual ~Client()
    {
        running = false;
        if (watchdogThread.joinable())
        {
            watchdogThread.join();
        }
    }

    void Start()
    {
        running = true;
        watchdogThread = std::thread(std::bind(&Client::watchdog, this));
    }

    Firebolt::Error Request(const std::string &method, const nlohmann::json &parameters, nlohmann::json &response)
    {
        MessageID id = transport_.GetNextMessageID();
        std::shared_ptr<Caller> c = std::make_shared<Caller>(id);
        {
            std::lock_guard lck(queue_mtx);
            queue[id] = c;
        }

        Firebolt::Error result = transport_.Send(method, parameters, id);
        if (result == Firebolt::Error::None)
        {
            {
                std::unique_lock<std::mutex> lk(c->mtx);
                c->waiter.wait(lk, [&] { return c->ready; });
                {
                    std::lock_guard lck(queue_mtx);
                    queue.erase(c->id);
                }
            }
            if (c->error == Firebolt::Error::None)
            {
                response = nlohmann::json::parse(c->response);
            }
            else
            {
                result = c->error;
            }
        }

        return result;
    }

    bool IdRequested(MessageID id)
    {
        std::lock_guard lck(queue_mtx);
        return queue.find(id) != queue.end();
    }

    void Response(const nlohmann::json &message)
    {
        MessageID id = message["id"];
        try
        {
            std::lock_guard lck(queue_mtx);
            auto c = queue.at(id);
            std::unique_lock<std::mutex> lk(c->mtx);

            if (!message.contains("error"))
            {
                c->response = to_string(message["result"]);
                c->response_json = message["result"];
            }
            else
            {
                c->error = static_cast<Firebolt::Error>(message["error"]["code"]);
            }
            c->ready = true;
            c->waiter.notify_one();
        }
        catch (const std::out_of_range &e)
        {
            std::cout << "No receiver for message-id: " << id << std::endl;
        }
    }

private:
    void watchdog()
    {
        auto watchdogTimer = std::chrono::milliseconds(runtime_watchdogCycle_ms);
        std::vector<std::shared_ptr<Caller>> outdated;

        while (running)
        {
            Timestamp now = std::chrono::steady_clock::now();
            {
                std::lock_guard lck(queue_mtx);
                for (auto it = queue.begin(); it != queue.end();)
                {
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second->timestamp).count() >
                        runtime_waitTime_ms)
                    {
                        outdated.push_back(it->second);
                        it = queue.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
            for (auto &c : outdated)
            {
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
};

class Server
{
    struct CallbackDataEvent
    {
        std::string eventName;
        const EventCallback lambda;
        void *usercb;
    };

    using EventList = std::list<CallbackDataEvent>;

    EventList eventList;
    mutable std::mutex eventMap_mtx;

    IServerTransport &transport_;

#ifdef ENABLE_MANAGE_API
    using DispatchFunctionProvider = std::function<std::string(const nlohmann::json &parameters, void *)>;

    struct Method
    {
        std::string name;
        DispatchFunctionProvider lambda;
        void *usercb;
    };

    struct Interface
    {
        std::string name;
        std::list<Method> methods;
    };

    std::map<std::string, Interface> providerMap;
    mutable std::mutex providers_mtx;
#endif

    std::string getKeyFromEvent(const std::string &event)
    {
        std::string key = event;
        size_t dotPos = key.find('.');
        if (dotPos != std::string::npos) {
            std::transform(key.begin(), key.begin() + dotPos, key.begin(),
                   [](unsigned char c) { return std::tolower(c); }); // ignore case of module name
            if (dotPos + 3 < key.size() && key.substr(dotPos + 1, 2) == "on")
            {
                key[dotPos + 3] = std::tolower(key[dotPos + 3]); // make lower-case the first latter after ".on"
                key.erase(dotPos + 1, 2); // erase "on"
            }
        }
        return key;
    }

public:
    Server(IServerTransport &transport) : transport_(transport) {}

    virtual ~Server()
    {
        {
            std::lock_guard lck(eventMap_mtx);
            eventList.clear();
        }
#ifdef ENABLE_MANAGE_API
        {
            std::lock_guard lck(providers_mtx);
            providerMap.clear();
        }
#endif
    }

    void Start() {}

    Firebolt::Error Subscribe(const std::string &event, EventCallback callback, void *usercb)
    {

        std::string key = getKeyFromEvent(event);

        CallbackDataEvent callbackData = {key, callback, usercb};

        std::lock_guard lck(eventMap_mtx);
        auto eventIt = std::find_if(eventList.begin(), eventList.end(),
            [&key, usercb](const CallbackDataEvent& e) {
                return e.eventName == key && e.usercb == usercb;
            });

        if (eventIt != eventList.end())
        {
            return Firebolt::Error::General;
        }

        eventList.push_back(callbackData);
        return Firebolt::Error::None;
    }

    Firebolt::Error Unsubscribe(const std::string &event, void *usercb)
    {
        std::lock_guard lck(eventMap_mtx);

        std::string key = getKeyFromEvent(event);
        auto it = std::find_if(eventList.begin(), eventList.end(),
            [&key, usercb](const CallbackDataEvent& e) {
                return e.eventName == key && e.usercb == usercb;
            });

        if (it == eventList.end())
        {
            return Firebolt::Error::General;
        }

        eventList.erase(it);
        return Firebolt::Error::None;
    }

    void Notify(const std::string &method, const nlohmann::json &parameters)
    {
        std::string key = method;
        size_t dotPos = key.find('.');
        if (dotPos != std::string::npos) {
            std::transform(key.begin(), key.begin() + dotPos, key.begin(),
                   [](unsigned char c) { return std::tolower(c); }); // ignore case of module name when looking for registrants
        }

        nlohmann::json params;
        if (parameters.contains("value") && parameters.size() == 1) {
            params = parameters["value"];
        } else {
            params = parameters;
        }

        std::lock_guard lck(eventMap_mtx);
        for (auto& callback : eventList)
        {
            if (callback.eventName == key)
            {
                callback.lambda(callback.usercb, params);
            }
        }
    }

    bool IsAnySubscriber(const std::string &method)
    {
        std::string key = method;
        size_t dotPos = key.find('.');
        if (dotPos != std::string::npos) {
            std::transform(key.begin(), key.begin() + dotPos, key.begin(),
                   [](unsigned char c) { return std::tolower(c); });
        }
        std::lock_guard lck(eventMap_mtx);

        for (auto& callback : eventList)
        {
            if (callback.eventName == key)
            {
                return true;
            }
        }
        return false;
    }

#ifdef ENABLE_MANAGE_API
    void Request(unsigned id, const std::string &method, const nlohmann::json &parameters)
    {
        size_t dotPos = method.find('.');
        if (dotPos == std::string::npos)
        {
            return;
        }
        std::string interface = method.substr(0, dotPos);
        ;
        std::string methodName = method.substr(dotPos + 1);
        std::lock_guard lck(providers_mtx);
        auto provider = providerMap.find(interface);
        if (provider == providerMap.end())
        {
            return;
        }
        auto &methods = provider->second.methods;
        auto it = methods.begin();
        nlohmann::json params;
        params["parameters"] = parameters;
        while (it != methods.end())
        {
            it = std::find_if(it, methods.end(), [&methodName](const Method &m) { return m.name == methodName; });
            if (it != methods.end())
            {
                std::string response = it->lambda(parameters.dump(), it->usercb);
                transport_.SendResponse(id, response);
                break;
            }
        }
    }

    Firebolt::Error RegisterProviderInterface(const std::string &fullMethod, ProviderCallback callback, void *usercb)
    {
        uint32_t waitTime = runtime_providerWaitTime;

        size_t dotPos = fullMethod.find('.');
        std::string interface = fullMethod.substr(0, dotPos);
        std::string method = fullMethod.substr(dotPos + 1);
        if (method.size() > 2 && method.substr(0, 2) == "on")
        {
            method[2] = std::tolower(method[2]);
            method.erase(0, 2); // erase "on"
        }

        std::function<std::string(void *usercb, const nlohmann::json &params)> actualCallback = callback;
        DispatchFunctionProvider lambda = [actualCallback, method, waitTime](const nlohmann::json &params, void *usercb)
        { return actualCallback(usercb, params); };
        std::lock_guard lck(providers_mtx);
        if (providerMap.find(interface) == providerMap.end())
        {
            Interface i = {
                .name = interface,
            };
            i.methods.push_back({
                .name = method,
                .lambda = lambda,
                .usercb = usercb,
            });
            providerMap[interface] = i;
        }
        else
        {
            auto &i = providerMap[interface];
            auto it = std::find_if(i.methods.begin(), i.methods.end(), [&method, usercb](const Method &m)
                                   { return m.name == method && m.usercb == usercb; });
            if (it == i.methods.end())
            {
                i.methods.push_back({
                    .name = method,
                    .lambda = lambda,
                    .usercb = usercb,
                });
            }
        }
        return Firebolt::Error::None;
    }

    Firebolt::Error UnregisterProviderInterface(const std::string &interface, const std::string &method, void *usercb)
    {
        std::lock_guard lck(providers_mtx);
        try
        {
            Interface &i = providerMap.at(interface);
            auto it = std::find_if(i.methods.begin(), i.methods.end(), [&method, usercb](const Method &m)
                                   { return m.name == method && m.usercb == usercb; });
            if (it != i.methods.end())
            {
                i.methods.erase(it);
            }
        }
        catch (const std::out_of_range &e)
        {
        }
        return Firebolt::Error::None;
    }
#else
    void Request(unsigned id, const std::string &method, const nlohmann::json &parameters) {}
#endif
};

class GatewayImpl : public IGateway,
                    private IClientTransport,
                    private IServerTransport
{
private:
    ConnectionChangeCallback connectionChangeListener;
    Transport transport;
    Client client;
    Server server;

public:
    GatewayImpl() : client(*this), server(*this) {}

    ~GatewayImpl() = default;

    virtual Firebolt::Error Connect(const FireboltSDK::Config &cfg, ConnectionChangeCallback onConnectionChange) override
    {
        assert(onConnectionChange != nullptr);

        FireboltSDK::JSON::EnumType<Logger::LogLevel> logLevels = {
            { "Error", Logger::LogLevel::Error },
            { "Warning", Logger::LogLevel::Warning },
            { "Info", Logger::LogLevel::Info },
            { "Debug", Logger::LogLevel::Debug }
        };

        std::string level = cfg.log.level;
        if (logLevels.find(level) == logLevels.end())
        {
            level = "Info";
        }

        FireboltSDK::Logger::SetLogLevel(logLevels[level]);
        FireboltSDK::Logger::SetFormat(cfg.log.format.ts, cfg.log.format.location, cfg.log.format.function,
                                       cfg.log.format.thread);

        connectionChangeListener = onConnectionChange;

        runtime_waitTime_ms = cfg.waitTime_ms;
        std::string url = cfg.wsUrl;
        if (url.find("?") == std::string::npos)
        {
            url += "?";
        }
        else
        {
            url += "&";
        }
        url += "RPCv2=true";

        std::optional<unsigned> transportLoggingInclude = cfg.log.transportInclude;
        std::optional<unsigned> transportLoggingExclude = cfg.log.transportExclude;

        FIREBOLT_LOG_INFO("Gateway", "Connecting to url = %s", url.c_str());
        Firebolt::Error status = transport.Connect(
            url, [this](const nlohmann::json &message) { this->onMessage(message); },
            [this](const bool connected, Firebolt::Error error) { this->onConnectionChange(connected, error); },
            transportLoggingInclude, transportLoggingExclude);

        if (status != Firebolt::Error::None)
        {
            return status;
        }

        client.Start();
        server.Start();
        return status;
    }

    virtual Firebolt::Error Disconnect() override { return transport.Disconnect(); }

    Firebolt::Error Request(const std::string &method, const nlohmann::json &parameters, nlohmann::json &response) override
    {
        return client.Request(method, parameters, response);
    }

    Firebolt::Error Subscribe(const std::string &event, EventCallback callback, void *usercb) override
    {
        bool alreadySubscribed = server.IsAnySubscriber(event);
        Firebolt::Error status = server.Subscribe(event, callback, usercb);
        if (status != Firebolt::Error::None)
        {
            return status;
        }

        if (alreadySubscribed)
        {
            return Firebolt::Error::None;
        }

        nlohmann::json params;
        params["listen"] = true;
        nlohmann::json result;
        status = client.Request(event, params, result);
        if (status == Firebolt::Error::None && (!result.contains("listening") || !result["listening"].get<bool>()))
        {
            status == Firebolt::Error::General;
        }
        if (status != Firebolt::Error::None)
        {
            server.Unsubscribe(event, usercb);
        }
        return status;
    }

    Firebolt::Error Unsubscribe(const std::string &event, void *usercb) override
    {
        Firebolt::Error status = server.Unsubscribe(event, usercb);
        if (status != Firebolt::Error::None)
        {
            return status;
        }

        if (server.IsAnySubscriber(event))
        {
            return Firebolt::Error::None;
        }

        nlohmann::json params;
        params["listen"] = false;
        nlohmann::json result;
        status = client.Request(event, params, result);
        if (status == Firebolt::Error::None && (!result.contains("listening") || result["listening"].get<bool>()))
        {
            status == Firebolt::Error::General;
        }
        return status;
    }

#ifdef ENABLE_MANAGE_API
    Firebolt::Error RegisterProviderInterface(const std::string &method, ProviderCallback callback, void *usercb) override
    {
        Firebolt::Error status = server.RegisterProviderInterface(method, callback, usercb);
        if (status != Firebolt::Error::None)
        {
            return status;
        }
        return status;
    }

    Firebolt::Error UnregisterProviderInterface(const std::string &interface, const std::string &method,
                                                void *usercb) override
    {
        return server.UnregisterProviderInterface(interface, method, usercb);
    }
#endif

private:
    void onMessage(const nlohmann::json &message)
    {
        if (message.contains("method"))
        {
            if (message.contains("id"))
            {
                server.Request(message["id"], message["method"], message["params"]);
            }
            else
            {
                server.Notify(message["method"], message["params"]);
            }
        }
        else
        {
            client.Response(message);
        }
    }

    void onConnectionChange(const bool connected, Firebolt::Error error) { connectionChangeListener(connected, error); }

    MessageID GetNextMessageID() override { return transport.GetNextMessageID(); }

    Firebolt::Error Send(const std::string &method, const nlohmann::json &parameters, MessageID id) override
    {
        return transport.Send(method, parameters, id);
    }

#ifdef ENABLE_MANAGE_API
    void SendResponse(unsigned id, const std::string &response) override { transport.SendResponse(id, response); }
#endif
};

IGateway &GetGatewayInstance()
{
    static GatewayImpl instance;
    return instance;
}

} // namespace FireboltSDK::Transport
