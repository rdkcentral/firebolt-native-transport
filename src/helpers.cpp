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

namespace Firebolt::Helpers
{
Parameters::Parameters(const std::vector<std::string>& value)
{
    WPEFramework::Core::JSON::ArrayType<WPEFramework::Core::JSON::Variant> valueArray;
    for (auto& element : value)
    {
        valueArray.Add() = element;
    }
    WPEFramework::Core::JSON::Variant valueVariant;
    valueVariant.Array(valueArray);
    object_.Set(_T("value"), valueVariant);
}

Parameters& Parameters::add(const char* paramName, const WPEFramework::Core::JSON::Variant& param)
{
    object_.Set(paramName, param);
    return *this;
}

JsonObject Parameters::operator()() const
{
    return object_;
}

Result<void> setNL(const string& methodName, const nlohmann::json& parameters)
{
    return Result<void>{FireboltSDK::Transport::Properties::SetNL(methodName, parameters)};
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
        FireboltSDK::Transport::Event::Instance().Unsubscribe(subscription.second.eventName, &subscription.second);
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
    auto errorStatus{FireboltSDK::Transport::Event::Instance().Unsubscribe(it->second.eventName, &it->second)};
    subscriptions_.erase(it);
    return Result<void>{errorStatus};
}
} // namespace Firebolt::Transport
