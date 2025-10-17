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
    ~HelperImpl() = default;

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
};

IHelper& GetHelperInstance()
{
    static HelperImpl instance;
    return instance;
}

} // namespace Firebolt::Helpers
