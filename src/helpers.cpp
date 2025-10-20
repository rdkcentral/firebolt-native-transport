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
#include "gateway.h"

namespace Firebolt::Helpers
{
class HelperImpl : public IHelper
{
public:
    ~HelperImpl() override { unsubscribeAll(); }

    Result<void> set(const std::string& methodName, const nlohmann::json& parameters) override
    {
        nlohmann::json result;
        nlohmann::json p;
        if (parameters.is_object())
        {
            p = parameters;
        }
        else
        {
            p["value"] = parameters;
        }
        return Result<void>{FireboltSDK::Transport::GetGatewayInstance().Request(methodName, p, result)};
    }

    Result<void> invoke(const std::string& methodName, const nlohmann::json& parameters) override
    {
        nlohmann::json result;
        return Result<void>{FireboltSDK::Transport::GetGatewayInstance().Request(methodName, parameters, result)};
    }

    Result<void> unsubscribe(SubscriptionId id) override
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

    void unsubscribeAll() override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &subscription : subscriptions_)
        {
            FireboltSDK::Transport::GetGatewayInstance().Unsubscribe(subscription.second.eventName);
        }
        subscriptions_.clear();
    }

private:
    Result<nlohmann::json> getJson(const std::string &methodName, const nlohmann::json &parameters) override
    {
        nlohmann::json result;
        Error status = FireboltSDK::Transport::GetGatewayInstance().Request(methodName, parameters, result);
        if (status != Error::None)
        {
            return Result<nlohmann::json>{status};
        }
        return Result<nlohmann::json>{result};
    }

    Result<SubscriptionId> subscribeImpl(const std::string &eventName, std::any &&notification,
                                         void (*callback)(void *, const nlohmann::json &)) override
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

IHelper& GetHelperInstance()
{
    static HelperImpl instance;
    return instance;
}

} // namespace Firebolt::Helpers
