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

#include "gateway.h"
#include "types/fb-errors.h"
#include "types/json_types.h"
#include "types/types.h"
#include "firebolttransport_export.h"
#include <any>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <type_traits>

namespace Firebolt::Helpers
{

struct SubscriptionData
{
    std::string eventName;
    std::any notification;
};

template <typename JsonType, typename PropertyType>
void onPropertyChangedCallback(void *subscriptionDataPtr, const nlohmann::json &jsonResponse)
{
    SubscriptionData *subscriptionData = reinterpret_cast<SubscriptionData *>(subscriptionDataPtr);
    std::function<void(PropertyType)> notifier =
        std::any_cast<std::function<void(PropertyType)>>(subscriptionData->notification);
    JsonType jsonType;
    jsonType.FromJson(jsonResponse);
    notifier(jsonType.Value());
}

class SubscriptionHelper
{
public:
    ~SubscriptionHelper() { unsubscribeAll(); }

    template <typename JsonType, typename PropertyType>
    Result<SubscriptionId> subscribe(const std::string &eventName, std::function<void(PropertyType)> &&notification)
    {
        return subscribeImpl(eventName, std::move(notification), onPropertyChangedCallback<JsonType, PropertyType>);
    }

    Result<void> unsubscribe(SubscriptionId id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscriptions_.find(id);
        if (it == subscriptions_.end())
        {
            return Result<void>{Error::General};
        }
        auto errorStatus{FireboltSDK::Transport::GetGatewayInstance().Unsubscribe(it->second.eventName)};
        subscriptions_.erase(it);
        return Result<void>{errorStatus};
    }

    void unsubscribeAll()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &subscription : subscriptions_)
        {
            FireboltSDK::Transport::GetGatewayInstance().Unsubscribe(subscription.second.eventName);
        }
        subscriptions_.clear();
    }

private:
    Result<SubscriptionId> subscribeImpl(const std::string &eventName, std::any &&notification,
                                         void (*callback)(void *, const nlohmann::json &))
    {
        std::lock_guard<std::mutex> lock(mutex_);
        uint64_t newId = currentId_++;
        subscriptions_[newId] = SubscriptionData{eventName, std::move(notification)};
        void *notificationPtr = reinterpret_cast<void *>(&subscriptions_[newId]);

        Error status = FireboltSDK::Transport::GetGatewayInstance().Subscribe(eventName, callback, notificationPtr);

        if (Error::None != status)
        {
            subscriptions_.erase(newId);
            return Result<SubscriptionId>{status};
        }
        return Result<SubscriptionId>{newId};
    }

    std::mutex mutex_;
    std::map<uint64_t, SubscriptionData> subscriptions_;
    uint64_t currentId_{0};
};

class IHelper
{
public:
    virtual ~IHelper() = default;

    virtual Result<void> set(const std::string &methodName, const nlohmann::json &parameters) = 0;
    virtual Result<void> invoke(const std::string &methodName, const nlohmann::json &parameters) = 0;

    template <typename JsonType, typename PropertyType>
    Result<PropertyType> get(const std::string &methodName, const nlohmann::json &parameters = nlohmann::json({}))
    {
        Result<nlohmann::json> result = getJson(methodName, parameters);
        if (!result)
        {
            return Result<PropertyType>{result.error()};
        }
        JsonType jsonResult;
        jsonResult.FromJson(*result);
        return Result<PropertyType>{jsonResult.Value()};
    }

private:
    virtual Result<nlohmann::json> getJson(const std::string &methodName, const nlohmann::json &parameters) = 0;
};

FIREBOLTTRANSPORT_EXPORT IHelper& GetHelperInstance();

} // namespace Firebolt::Helpers
