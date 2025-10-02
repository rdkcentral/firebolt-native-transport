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

#include "error.h"

#include "TypesPriv.h"
#include "Transport.h"

#include "gateway/common.h"
#include "gateway/client.h"
#include "gateway/server.h"

#include <string>
#include <nlohmann/json.hpp>

namespace FireboltSDK::Transport
{
class GatewayImpl : public ITransportReceiver
{
    Config config;
    Client client;
    Server server;
    Transport* transport_;

public:
    GatewayImpl()
      : client(config)
      , server(config)
    {
    }

    void TransportUpdated(Transport* transport)
    {
        this->transport_ = transport;
        client.SetTransport(transport);
        if (transport != nullptr) {
            transport->SetTransportReceiver(this);
        }
    }

    virtual void Receive(const nlohmann::json& message) override
    {
        if (message.contains("method")) {
            if (message.contains("id")) {
                server.Request(transport_, message["id"], message["method"], message["params"]);
            } else {
                server.Notify(message["method"], message["params"]);
            }
        } else {
            client.Response(message);
        }
    }

    Firebolt::Error Request(const std::string &method, const nlohmann::json &parameters, nlohmann::json &response)
    {
        if (transport_ == nullptr) {
            return Firebolt::Error::NotConnected;
        }
        return client.Request(method, parameters, response);
    }

    Firebolt::Error Subscribe(const std::string& event, std::function<void(void*, const nlohmann::json&)> callback, void* usercb)
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

    Firebolt::Error Unsubscribe(const std::string& event)
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

    template <typename CALLBACK>
    Firebolt::Error RegisterProviderInterface(const std::string &method, const CALLBACK& callback, void* usercb)
    {
        Firebolt::Error status = server.RegisterProviderInterface(method, callback, usercb);
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
