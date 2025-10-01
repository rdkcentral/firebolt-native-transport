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

#include "helpers.h"
#include "Gateway.h"

namespace Firebolt::Helpers
{

Result<void> setNL(const string& methodName, const nlohmann::json& parameters)
{
    // JsonObject responseType;
    WPEFramework::Core::JSON::VariantContainer responseType;
    return Result<void>{FireboltSDK::Transport::Gateway::Instance().Request(methodName, parameters, responseType)};
}

Result<void> invokeNL(const string& methodName, const nlohmann::json& parameters)
{
    WPEFramework::Core::JSON::VariantContainer result;
    return Result<void>{FireboltSDK::Transport::Gateway::Instance().Request(methodName, parameters, result)};
}

SubscriptionHelper::~SubscriptionHelper()
{
    unsubscribeAll();
}

void SubscriptionHelper::unsubscribeAll()
{
    for (auto& subscription : subscriptions_)
    {
        FireboltSDK::Transport::Gateway::Instance().Unsubscribe(subscription.second.eventName);
    }
    subscriptions_.clear();
}

Result<void> SubscriptionHelper::unsubscribe(SubscriptionId id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subscriptions_.find(id);
    if (it == subscriptions_.end())
    {
        return Result<void>{Error::General};
    }
    auto errorStatus{FireboltSDK::Transport::Gateway::Instance().Unsubscribe(it->second.eventName)};
    subscriptions_.erase(it);
    return Result<void>{errorStatus};
}
} // namespace Firebolt::Transport
