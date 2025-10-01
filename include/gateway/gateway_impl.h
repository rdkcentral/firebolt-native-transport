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

#include "TypesPriv.h"
#include "Transport.h"
#include "Transport_NEW.h"

#include "gateway/common.h"
#include "gateway/client.h"
#include "gateway/server.h"

#include <string>
#include <nlohmann/json.hpp>

namespace FireboltSDK::Transport
{
class ListeningResponse : public FireboltSDK::JSON::NL_Json_Basic<bool>
{
public:
    void FromJson(const nlohmann::json& json) override {
        if (json.contains("listening")) {
            value_ = json["listening"].get<bool>();
            is_set_ = true;
        }
    }
    bool Value() const override { return value_; }
    bool isSet() const { return is_set_; }
    bool isListening() const { return value_; }
private:
    bool value_;
    bool is_set_ = false;
};

class GatewayImpl : public ITransportReceiver, ITransportReceiver_PP
{
    Config config;
    Client client;
    Server server;
    Transport_PP* transport_pp;

    std::string jsonObject2String(const JsonObject &obj) {
        std::string s;
        obj.ToString(s);
        return s;
    }

public:
    GatewayImpl()
      : client(config)
      , server(config)
    {
    }

    void TransportUpdated(Transport_PP* transportPP)
    {
        this->transport_pp = transportPP;
        client.SetTransport(transportPP);
        if (transportPP != nullptr) {
            transportPP->SetTransportReceiver(this);
        }
    }

    virtual void Receive(const nlohmann::json& message) override
    {
        if (message.contains("method")) {
            if (message.contains("id")) {
                server.Request(transport_pp, message["id"], message["method"], to_string(message["params"]));
            } else {
                server.Notify(message["method"], to_string(message["params"]));
            }
        } else {
            client.Response(message);
        }
    }

    virtual void Receive(const WPEFramework::Core::JSONRPC::Message& message) override
    {
    }

    template <typename RESPONSE>
    Firebolt::Error Request(const std::string &method, const nlohmann::json &parameters, RESPONSE &response)
    {
        if (transport_pp == nullptr) {
            return Firebolt::Error::NotConnected;
        }
        return client.Request(method, parameters, response);
    }

    template <typename RESULT, typename CALLBACK>
    Firebolt::Error Subscribe(const string& event, JsonObject& parameters, const CALLBACK& callback, void* usercb, const void* userdata, bool prioritize = false)
    {
        Firebolt::Error status = server.Subscribe<RESULT>(event, parameters, callback, usercb, userdata);
        if (status != Firebolt::Error::None) {
            return status;
        }

        nlohmann::json params = nlohmann::json::parse(jsonObject2String(parameters));
        params["listen"] = true;
        ListeningResponse response;
        status = client.Request(event, params, response);
        if (status == Firebolt::Error::None && (!response.isSet() || !response.isListening())) {
            status == Firebolt::Error::General;
        }
        if (status != Firebolt::Error::None) {
            server.Unsubscribe(event);
        }
        return status;
    }

    Firebolt::Error Unsubscribe(const string& event)
    {
        Firebolt::Error status = server.Unsubscribe(event);
        if (status != Firebolt::Error::None) {
            return status;
        }
        nlohmann::json params;
        params["listen"] = true;
        ListeningResponse response;
        status = client.Request(event, params, response);
        if (status == Firebolt::Error::None && (!response.isSet()|| response.isListening())) {
            status == Firebolt::Error::General;
        }
        return status;
    }

    template <typename RESPONSE, typename PARAMETERS, typename CALLBACK>
    Firebolt::Error RegisterProviderInterface(const std::string &method, const PARAMETERS &parameters, const CALLBACK& callback, void* usercb)
    {
        Firebolt::Error status = server.RegisterProviderInterface<RESPONSE>(method, parameters, callback, usercb);
        if (status != Firebolt::Error::None) {
            return status;
        }
        return status;
    }

    Firebolt::Error UnregisterProviderInterface(const std::string &interface, const std::string &method, void* usercb)
    {
        return server.UnregisterProviderInterface(interface, method, usercb);
    }
};
} // namespace Firebolt::Transport
