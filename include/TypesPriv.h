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

#include <nlohmann/json.hpp>
#include <string>
#include <algorithm>
#include <cctype>
#include <map>

namespace FireboltSDK::JSON
{

struct ICaseComparator
{
    bool operator()(const std::string& a, const std::string& b) const noexcept
    {
        return ::strcasecmp(a.c_str(), b.c_str()) < 0;
    }
};

template <typename T> using EnumType = std::map<std::string, T, ICaseComparator>;
template <typename T>
inline std::string ToString(const EnumType<T>& enumType, const T& value)
{
    auto it = std::find_if(enumType.begin(), enumType.end(),
        [&value](const auto& pair) { return pair.second == value; });
    if (it != enumType.end()) {
        return it->first;
    }
    return {};
}

template <typename T>
class NL_Json_Basic {
public:
    virtual void FromJson(const nlohmann::json& json) = 0;
    T virtual Value() const = 0;

    void FromString(const std::string& str) { FromJson(nlohmann::json::parse(str)); }
};

template <typename T>
class BasicType : public NL_Json_Basic<T>
{
public:
    void FromJson(const nlohmann::json& json) override { value_ = json.get<T>(); }
    T Value() const override { return value_; }
private:
    T value_;
};

using String = BasicType<std::string>;
using Boolean = BasicType<bool>;
using Float = BasicType<float>;
using Unsigned = BasicType<uint32_t>;
using Integer = BasicType<int32_t>;

template <typename T1, typename T2>
class NL_Json_Array : public NL_Json_Basic<std::vector<T2>>
{
public:
    void FromJson(const nlohmann::json& json) override
    {
        value_.clear();
        for (const auto& item : json) {
            T1 element;
            element.FromJson(item);
            value_.push_back(element.Value());
        }
    }
    std::vector<T2> Value() const override {
        return value_;
    }
private:
    std::vector<T2> value_;
};

} // namespace FireboltSDK::JSON
