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

#include "Portability.h"
#include "Transport.h"
#include "Transport_NEW.h"

#include <functional>
#include <string>

#include "gateway/common.h"
#include "gateway/gateway_impl.h"

#include <nlohmann/json.hpp>

namespace FireboltSDK::Transport
{
class FIREBOLTSDK_EXPORT Gateway {
    static Gateway *instance;

    std::unique_ptr<GatewayImpl> implementation;

private:
    Gateway(std::unique_ptr<GatewayImpl> implementation);

public:
    Gateway(const Gateway&) = delete;
    Gateway& operator=(const Gateway&) = delete;
    virtual ~Gateway();

    static Gateway& Instance();
    static void Dispose();

    void TransportUpdated(Transport_PP* transportPP);

    template <typename RESPONSE>
    Firebolt::Error Request(const std::string &method, const nlohmann::json &parameters, RESPONSE &response)
    {
        return implementation->Request(method, parameters, response);
    }

    template <typename RESULT, typename CALLBACK>
    Firebolt::Error Subscribe(const string& event, JsonObject& parameters, const CALLBACK& callback, void* usercb, const void* userdata, bool prioritize = false)
    {
        return implementation->Subscribe<RESULT>(event, parameters, callback, usercb, userdata, prioritize);
    }

    Firebolt::Error Unsubscribe(const std::string& event)
    {
        return implementation->Unsubscribe(event);
    }

    template <typename RESPONSE, typename PARAMETERS, typename CALLBACK>
    Firebolt::Error RegisterProviderInterface(const std::string &method, const PARAMETERS &parameters, const CALLBACK& callback, void* usercb)
    {
        return implementation->RegisterProviderInterface<RESPONSE>(method, parameters, callback, usercb);
    }

    Firebolt::Error UnregisterProviderInterface(const std::string &interface, const std::string &method, void* usercb)
    {
        return implementation->UnregisterProviderInterface(interface, method, usercb);
    }
};
} // namespace Firebolt::Transport
