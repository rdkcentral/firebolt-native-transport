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
#include "Transport.h"
#include "firebolttransport_export.h"

namespace FireboltSDK::Transport
{

struct Config
{
    static constexpr uint64_t watchdogThreshold_ms = 3000;
    static constexpr uint64_t watchdogCycle_ms = 500;
    static constexpr uint32_t DefaultWaitTime = -1;
};

class FIREBOLTTRANSPORT_EXPORT Gateway {
private:
    Gateway();

public:
    Gateway(const Gateway&) = delete;
    Gateway& operator=(const Gateway&) = delete;
    virtual ~Gateway();

    static Gateway& Instance();
    static void Dispose();

    void TransportUpdated(Transport* transport);

    Firebolt::Error Request(const std::string &method, const nlohmann::json &parameters, nlohmann::json &response);
    Firebolt::Error Subscribe(const std::string& event, std::function<void(void*, const nlohmann::json&)> callback, void* usercb);
    Firebolt::Error Unsubscribe(const std::string& event);

    template <typename CALLBACK>
    Firebolt::Error RegisterProviderInterface(const std::string &method, const CALLBACK& callback, void* usercb);
    Firebolt::Error UnregisterProviderInterface(const std::string &interface, const std::string &method, void* usercb);

private:
    static Gateway *instance;

    class GatewayImpl;
    std::unique_ptr<GatewayImpl> implementation;
};
} // namespace Firebolt::Transport