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

#include "Portability.h"
#include "Gateway.h"
#include "FireboltSDK.h"
#include "common/types.h"
#include "error.h"
#include <any>
#include <map>
#include <mutex>
#include <optional>
#include <type_traits>

namespace Firebolt::Helpers
{

template <typename JsonType, typename PropertyType>
FIREBOLTSDK_EXPORT Result<PropertyType> get(const std::string& methodName, const nlohmann::json& parameters = nlohmann::json({})) 
{
    nlohmann::json result;
    Error status = FireboltSDK::Transport::Gateway::Instance().Request(methodName, parameters, result);
    if (status == Error::None)
    {
        JsonType jsonResult;
        jsonResult.FromJson(result);
        return Result<PropertyType>{jsonResult.Value()};
    }
    return Result<PropertyType>{status};
}

FIREBOLTSDK_EXPORT Result<void> invokeNL(const std::string& methodName, const nlohmann::json& parameters);
FIREBOLTSDK_EXPORT Result<void> setNL(const std::string& methodName, const nlohmann::json& parameters);

template <typename JsonType, typename PropertyType>
FIREBOLTSDK_EXPORT inline Result<PropertyType> invokeNL(const std::string& methodName, const nlohmann::json& parameters = {})
{
    nlohmann::json params;
    nlohmann::json result;
    Error status = FireboltSDK::Transport::Gateway::Instance().Request(methodName, params, result);
    if (status == Error::None)
    {
        JsonType jsonResult;
        jsonResult.FromJson(result);
        return Result<PropertyType>{jsonResult.Value()};
    }
    return Result<PropertyType>{status};
}

struct SubscriptionData
{
    std::string eventName;
    std::any notification;
};

template <typename JsonType, typename PropertyType>
void onPropertyChangedCallback(void* subscriptionDataPtr, const nlohmann::json& jsonResponse)
{
    SubscriptionData* subscriptionData = reinterpret_cast<SubscriptionData*>(subscriptionDataPtr);
    std::function<void(PropertyType)> notifier =
        std::any_cast<std::function<void(PropertyType)>>(subscriptionData->notification);
    JsonType jsonType;
    jsonType.FromJson(jsonResponse);
    notifier(jsonType.Value());
}

class FIREBOLTSDK_EXPORT SubscriptionHelper
{
public:
    void unsubscribeAll();

protected:
    SubscriptionHelper() = default;
    virtual ~SubscriptionHelper();

    Result<void> unsubscribe(SubscriptionId id);
    template <typename JsonType, typename PropertyType>
    Result<SubscriptionId> subscribe(const std::string& eventName, std::function<void(PropertyType)>&& notification)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        subscriptions_[currentId_] = SubscriptionData{eventName, std::move(notification)};
        void* notificationPtr = reinterpret_cast<void*>(&subscriptions_[currentId_]);
        Error status = FireboltSDK::Transport::Gateway::Instance().Subscribe(eventName,
                                                               onPropertyChangedCallback<JsonType, PropertyType>,
                                                               notificationPtr);
        if (Error::None == status)
        {
            return Result<SubscriptionId>{currentId_++};
        }
        subscriptions_.erase(currentId_);
        return Result<SubscriptionId>{status};
    }

private:
    std::mutex mutex_;
    std::map<uint64_t, SubscriptionData> subscriptions_;
    uint64_t currentId_{0};
};
} // namespace Firebolt::Transport
