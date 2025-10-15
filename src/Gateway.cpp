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

#include "Gateway.h"
#include "Transport.h"
#include "error.h"
#include "TypesPriv.h"
#include "Logger.h"
#include <chrono>
#include <condition_variable>
#include <list>
#include <map>
#include <map>
#include <mutex>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <string>
#include <thread>

#ifdef UNIT_TEST
#include "json_engine.h"
#endif

namespace FireboltSDK::Transport {

struct Config
{
    unsigned waitTime_ms;
    unsigned watchdogCycle_ms;
    int providerWaitTime;
};

static Config config_g =  {
    .waitTime_ms = 3000,
    .watchdogCycle_ms = 500,
    .providerWaitTime = -1,
};

static Transport transport;

using Timestamp = std::chrono::time_point<std::chrono::steady_clock>;
using MessageID = uint32_t;

Gateway::~Gateway() = default;

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
        nlohmann::json response_json;
        Firebolt::Error error = Firebolt::Error::None;
        bool ready = false;
        std::mutex mtx;
        std::condition_variable waiter;
    };

    std::map <MessageID, std::shared_ptr<Caller>> queue;
    mutable std::mutex queue_mtx;

    std::atomic<bool> running { false };
    std::thread watchdogThread;

#ifdef UNIT_TEST
    JsonEngine jsonEngine;
#endif

    void watchdog()
    {
        auto watchdogTimer = std::chrono::milliseconds(config_g.watchdogCycle_ms);
        std::vector<std::shared_ptr<Caller>> outdated;

        while (running) {
            Timestamp now = std::chrono::steady_clock::now();
            {
                std::lock_guard lck(queue_mtx);
                for (auto it = queue.begin(); it != queue.end();) {
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second->timestamp).count() > config_g.waitTime_ms) {
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
    Client()
    {
    }

    virtual ~Client()
    {
        running = false;
        if (watchdogThread.joinable()) {
            watchdogThread.join();
        }
    }

    void Start() {
        running = true;
        watchdogThread = std::thread(std::bind(&Client::watchdog, this));
    }

#ifdef UNIT_TEST
    Firebolt::Error Request(const std::string &method, const nlohmann::json &parameters, nlohmann::json &response)
    {
        nlohmann::json message;
        message["jsonrpc"] = "2.0";
        message["id"] = transport.GetNextMessageID();
        message["method"] = method;
        message["params"] = parameters;
        if (message["params"].empty()) {
            Firebolt::Error result = jsonEngine.MockResponse(message);
            if (result != Firebolt::Error::None) {
                return result;
            }
            response = message["result"];
        } else {
            jsonEngine.MockRequest(message);
        }
        return Firebolt::Error::None;
    }
#else
    Firebolt::Error Request(const std::string &method, const nlohmann::json &parameters, nlohmann::json &response)
    {
        MessageID id = transport.GetNextMessageID();
        std::shared_ptr<Caller> c = std::make_shared<Caller>(id);
        {
            std::lock_guard lck(queue_mtx);
            queue[id] = c;
        }

        Firebolt::Error result = transport.Send(method, parameters, id);
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
                response = nlohmann::json::parse(c->response);
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
                c->response_json = message["result"];
            } else {
                c->error = static_cast<Firebolt::Error>(message["error"]["code"]);
            }
            c->ready = true;
            c->waiter.notify_one();
        } catch (const std::out_of_range &e) {
            std::cout << "No receiver for message-id: " << id << std::endl;
        }
    }
};

class Server
{
    struct CallbackDataEvent {
        const EventCallback lambda;
        void* usercb;
    };

    using EventMap = std::map<std::string, CallbackDataEvent>;

    EventMap eventMap;
    mutable std::mutex eventMap_mtx;

#ifdef ENABLE_MANAGE_API
    using DispatchFunctionProvider = std::function<std::string(const nlohmann::json &parameters, void*)>;

    struct Method {
        std::string name;
        DispatchFunctionProvider lambda;
        void* usercb;
    };

    struct Interface {
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
        if (dotPos != std::string::npos && dotPos + 3 < key.size() && key.substr(dotPos + 1, 2) == "on") {
            key[dotPos + 3] = std::tolower(key[dotPos + 3]); // make lower-case the first latter after ".on"
            key.erase(dotPos + 1, 2); // erase "on"
        }
        return key;
    }

public:
    virtual ~Server()
    {
        {
            std::lock_guard lck(eventMap_mtx);
            eventMap.clear();
        }
#ifdef ENABLE_MANAGE_API
        {
            std::lock_guard lck(providers_mtx);
            providerMap.clear();
        }
#endif
    }

    void Start()
    {
    }

    Firebolt::Error Subscribe(const std::string& event, EventCallback callback, void* usercb)
    {
        Firebolt::Error status = Firebolt::Error::General;

        CallbackDataEvent callbackData = {callback, usercb};

        std::string key = getKeyFromEvent(event);

        std::lock_guard lck(eventMap_mtx);
        EventMap::iterator eventIndex = eventMap.find(key);
        if (eventIndex == eventMap.end())
        {
            eventMap.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(callbackData));
            status = Firebolt::Error::None;
        }

        return status;
    }

    Firebolt::Error Unsubscribe(const std::string& event)
    {
        std::lock_guard lck(eventMap_mtx);
        return eventMap.erase(getKeyFromEvent(event)) > 0 ? Firebolt::Error::None : Firebolt::Error::General;
    }

    void Notify(const std::string &method, const nlohmann::json &parameters)
    {
        std::string key = method;
        std::lock_guard lck(eventMap_mtx);
        EventMap::iterator eventIt = eventMap.find(method);
        if (eventIt != eventMap.end()) {
            CallbackDataEvent& callback = eventIt->second;
            callback.lambda(callback.usercb, parameters);
        }
    }

#ifdef ENABLE_MANAGE_API
    void Request(unsigned id, const std::string &method, const nlohmann::json &parameters)
    {
        size_t dotPos = method.find('.');
        if (dotPos == std::string::npos) {
            return;
        }
        std::string interface = method.substr(0, dotPos);;
        std::string methodName = method.substr(dotPos + 1);
        std::lock_guard lck(providers_mtx);
        auto provider = providerMap.find(interface);
        if (provider == providerMap.end()) {
            return;
        }
        auto& methods = provider->second.methods;
        auto it = methods.begin();
        nlohmann::json params;
        params["parameters"] = parameters;
        while (it != methods.end()) {
            it = std::find_if(it, methods.end(), [&methodName](const Method &m) { return m.name == methodName; });
            if (it != methods.end()) {
                std::string response = it->lambda(parameters.dump(), it->usercb);
                transport.SendResponse(id, response);
                break;
            }
        }
    }

    Firebolt::Error RegisterProviderInterface(const std::string &fullMethod, ProviderCallback callback, void* usercb)
    {
        uint32_t waitTime = config_g.providerWaitTime;

        size_t dotPos = fullMethod.find('.');
        std::string interface = fullMethod.substr(0, dotPos);
        std::string method = fullMethod.substr(dotPos + 1);
        if (method.size() > 2 && method.substr(0, 2) == "on") {
            method[2] = std::tolower(method[2]);
            method.erase(0, 2); // erase "on"
        }

        std::function<std::string(void* usercb, const nlohmann::json &params)> actualCallback = callback;
        DispatchFunctionProvider lambda = [actualCallback, method, waitTime](const nlohmann::json& params, void* usercb) {
            return actualCallback(usercb, params);
        };
        std::lock_guard lck(providers_mtx);
        if (providerMap.find(interface) == providerMap.end()) {
            Interface i = {
                .name = interface,
            };
            i.methods.push_back({
                .name = method,
                .lambda = lambda,
                .usercb = usercb,
            });
            providerMap[interface] = i;
        } else {
            auto &i = providerMap[interface];
            auto it = std::find_if(i.methods.begin(), i.methods.end(), [&method, usercb](const Method &m) { return m.name == method && m.usercb == usercb; });
            if (it == i.methods.end()) {
                i.methods.push_back({
                    .name = method,
                    .lambda = lambda,
                    .usercb = usercb,
                });
            }
        }
        return Firebolt::Error::None;
    }

    Firebolt::Error UnregisterProviderInterface(const std::string &interface, const std::string &method, void* usercb)
    {
        std::lock_guard lck(providers_mtx);
        try {
            Interface &i = providerMap.at(interface);
            auto it = std::find_if(i.methods.begin(), i.methods.end(), [&method, usercb](const Method &m) { return m.name == method && m.usercb == usercb; });
            if (it != i.methods.end()) {
                i.methods.erase(it);
            }
        } catch (const std::out_of_range &e) {
        }
        return Firebolt::Error::None;
    }
#else
    void Request(unsigned id, const std::string &method, const nlohmann::json &parameters)
    {
    }
#endif
};

static Client client;
static Server server;

class GatewayImpl : public Gateway, IMessageReceiver, IConnectionReceiver
{
    ConnectionChangeCallback connectionChangeListener = nullptr;

public:
    GatewayImpl() = default;
    ~GatewayImpl() = default;

    virtual Firebolt::Error Connect(const std::string& configLine, ConnectionChangeCallback listener) override
    {
        nlohmann::json userConfig = nlohmann::json::parse(configLine);
        nlohmann::json config;
        config["waitTime"] = config_g.waitTime_ms;
        config["log"] = nlohmann::json::parse(R"({ "level": "Info", "format": { "ts": true, "location": false, "function": true, "thread": true }})");
        config["wsUrl"] = "ws://127.0.0.1:9998";
        config["RPCv2"] = true;

        config.merge_patch(userConfig);
        if (userConfig.contains("logLevel")) {
            config["log"]["level"] = userConfig["logLevel"];
        }

        FireboltSDK::JSON::EnumType<Logger::LogLevel> logLevels = {
            { "Error", Logger::LogLevel::Error },
            { "Warning", Logger::LogLevel::Warning },
            { "Info", Logger::LogLevel::Info },
            { "Debug", Logger::LogLevel::Debug }
        };

        nlohmann::json logInfo = config["log"];
        nlohmann::json logFormat = logInfo["format"];
        FireboltSDK::Logger::SetLogLevel(logLevels[logInfo["level"]]);
        FireboltSDK::Logger::SetFormat(
            logFormat["ts"].get<bool>(),
            logFormat["location"].get<bool>(),
            logFormat["function"].get<bool>(),
            logFormat["thread"].get<bool>()
        );
        connectionChangeListener = listener;
        std::string urlParamsKeys[] = { "RPCv2" };
        std::string urlParams;
        for ( auto k : urlParamsKeys ) {
            if (config.contains(k)) {
                if (config[k].is_boolean() ) {
                    urlParams += (urlParams.empty() ? "?" : "&") + k + "=" + (config[k].get<bool>() ? "true" : "false");
                }
            }
        }
        config_g.waitTime_ms = config["waitTime"].get<unsigned>();
        std::string url = config["wsUrl"].get<std::string>() + urlParams;
        FIREBOLT_LOG_INFO("Gateway", "Connecting to url = %s", url.c_str());
        Firebolt::Error status = transport.Connect(url, this, this);
        client.Start();
        server.Start();
        return status;
    }

    virtual Firebolt::Error Disconnect() override
    {
        return transport.Disconnect();
    }

    virtual void Receive(const nlohmann::json& message) override
    {
        if (message.contains("method")) {
            if (message.contains("id")) {
                server.Request(message["id"], message["method"], message["params"]);
            } else {
                server.Notify(message["method"], message["params"]);
            }
        } else {
            client.Response(message);
        }
    }

    void ConnectionChanged(const bool connected, Firebolt::Error error) override
    {
        if (connectionChangeListener != nullptr) {
            connectionChangeListener(connected, error);
        }
    }

    Firebolt::Error Request(const std::string &method, const nlohmann::json &parameters, nlohmann::json &response) override
    {
        return client.Request(method, parameters, response);
    }

    Firebolt::Error Subscribe(const std::string& event, EventCallback callback, void* usercb) override
    {
        Firebolt::Error status = server.Subscribe(event, callback, usercb);
        if (status != Firebolt::Error::None) {
            return status;
        }

        nlohmann::json params;
        params["listen"] = true;
        nlohmann::json result;
        status = client.Request(event, params, result);
        if (status == Firebolt::Error::None && (!result.contains("listening") || !result["listening"].get<bool>())) {
            status == Firebolt::Error::General;
        }
        if (status != Firebolt::Error::None) {
            server.Unsubscribe(event);
        }
        return status;
    }

    Firebolt::Error Unsubscribe(const std::string& event) override
    {
        Firebolt::Error status = server.Unsubscribe(event);
        if (status != Firebolt::Error::None) {
            return status;
        }
        nlohmann::json params;
        params["listen"] = false;
        nlohmann::json result;
        status = client.Request(event, params, result);
        if (status == Firebolt::Error::None && (!result.contains("listening") || result["listening"].get<bool>())) {
            status == Firebolt::Error::General;
        }
        return status;
    }

#ifdef ENABLE_MANAGE_API
    Firebolt::Error RegisterProviderInterface(const std::string &method, ProviderCallback callback, void* usercb) override
    {
        Firebolt::Error status = server.RegisterProviderInterface(method, callback, usercb);
        if (status != Firebolt::Error::None) {
            return status;
        }
        return status;
    }

    Firebolt::Error UnregisterProviderInterface(const std::string &interface, const std::string &method, void* usercb) override
    {
        return server.UnregisterProviderInterface(interface, method, usercb);
    }
#endif
};

Gateway& GetGatewayInstance()
{
    static GatewayImpl instance;
    return instance;
}

} // namespace Firebolt::Transport
