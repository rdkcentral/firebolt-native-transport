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
class NL_Json_Basic {
public:
    virtual ~NL_Json_Basic() = default;
    virtual void FromJson(const nlohmann::json& json) = 0;
    T virtual Value() const = 0;

    void FromString(const std::string& str) { FromJson(nlohmann::json::parse(str)); }
};

class String : public NL_Json_Basic<std::string>
{
public:
    void FromJson(const nlohmann::json& json) override { value_ = json.get<std::string>(); }
    std::string Value() const override { return value_; }

private:
    std::string value_;
};

class WPE_String : public WPEFramework::Core::JSON::String
{
    using Base = WPEFramework::Core::JSON::String;

public:
    WPE_String() : Base(), value_() {}
    WPE_String(const char value[]) : Base(value), value_(value) {}
    WPE_String& operator=(const char rhs[])
    {
        Base::operator=(rhs);
        value_ = rhs;
        return (*this);
    }
    WPE_String& operator=(const WPE_String rhs)
    {
        Base::operator=(rhs);
        value_ = rhs;
        return (*this);
    }

public:
    const std::string& Value() const
    {
        value_ = Base::Value();
        return value_;
    }

private:
    mutable std::string value_;
};
} // namespace FireboltSDK::JSON
