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

#include <functional>
#include <string>
#include <nlohmann/json.hpp>
#include "error.h"
#include "firebolttransport_export.h"

namespace FireboltSDK::Transport
{
using EventCallback = std::function<void(void* usercb, const nlohmann::json &params)>;
using ConnectionChangeCallback = std::function<void(const bool connected, const Firebolt::Error error)>;
#ifdef ENABLE_MANAGE_API
using ProviderCallback = std::function<std::string(void* usercb, const nlohmann::json &params)>;
#endif

class FIREBOLTTRANSPORT_EXPORT Gateway {
public:
    virtual ~Gateway();

    virtual Firebolt::Error Connect(const std::string& configLine, ConnectionChangeCallback onConnectionChange) = 0;
    virtual Firebolt::Error Disconnect() = 0;

    virtual Firebolt::Error Request(const std::string &method, const nlohmann::json &parameters, nlohmann::json &response) = 0;
    virtual Firebolt::Error Subscribe(const std::string& event, EventCallback callback, void* usercb) = 0;
    virtual Firebolt::Error Unsubscribe(const std::string& event) = 0;

#ifdef ENABLE_MANAGE_API
    virtual Firebolt::Error RegisterProviderInterface(const std::string &method, ProviderCallback callback, void* usercb) = 0;
    virtual Firebolt::Error UnregisterProviderInterface(const std::string &interface, const std::string &method, void* usercb) = 0;
#endif
};

FIREBOLTTRANSPORT_EXPORT Gateway& GetGatewayInstance();
}