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

#include "error.h"

#include <string>
#include <iostream>
#include <nlohmann/json.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

namespace FireboltSDK::Transport
{
class ITransportReceiver {
public:
    virtual void Receive(const nlohmann::json& message) = 0;
};

class Transport
{
public:
    Transport() = default;
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;
    Transport(Transport&&) = delete;
    Transport& operator=(Transport&&) = delete;
    virtual ~Transport();

    Firebolt::Error Connect(std::string url);
    void SetTransportReceiver(ITransportReceiver *transportReceiver);
    unsigned GetNextMessageID();
    Firebolt::Error Send(const std::string &method, const nlohmann::json &params, const unsigned id);
    Firebolt::Error SendResponse(const unsigned id, const std::string &response);
    void SetLogging(websocketpp::log::level include, websocketpp::log::level exclude = 0);

private:
    void on_message(websocketpp::client<websocketpp::config::asio_client>* client_, websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg);
    void runner();

private:
    class TransportImpl;

    unsigned id_counter_ = 0;
    websocketpp::client<websocketpp::config::asio_client> client_;
    websocketpp::client<websocketpp::config::asio_client>::connection_ptr connection_;
    std::atomic<bool> connected_ = false;
    std::thread runner_thread_;
    ITransportReceiver *transportReceiver_ = nullptr;
    std::atomic<bool> stop_ = false;
};
}
