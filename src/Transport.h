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
class IMessageReceiver {
public:
    virtual void Receive(const nlohmann::json& message) = 0;
};

class IConnectionReceiver {
public:
    virtual void ConnectionChanged(const bool connected, Firebolt::Error error) = 0;
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

    Firebolt::Error Connect(std::string url, IMessageReceiver *messageReceiver, IConnectionReceiver *connectionReceiver);
    Firebolt::Error Disconnect();
    void SetLogging(websocketpp::log::level include, websocketpp::log::level exclude = 0);
    unsigned GetNextMessageID();
    Firebolt::Error Send(const std::string &method, const nlohmann::json &params, const unsigned id);
#ifdef ENABLE_MANAGE_API
    Firebolt::Error SendResponse(const unsigned id, const std::string &response);
#endif

private:
    void start();
    void on_message(websocketpp::connection_hdl hdl, websocketpp::client<websocketpp::config::asio_client>::message_ptr msg);
    void on_open(websocketpp::client<websocketpp::config::asio_client>* c, websocketpp::connection_hdl hdl);
    void on_close(websocketpp::client<websocketpp::config::asio_client>* c, websocketpp::connection_hdl hdl);
    void on_fail(websocketpp::client<websocketpp::config::asio_client>* c, websocketpp::connection_hdl hdl);

private:
    // websocketpp::client<websocketpp::config::asio_client>::connection_ptr connection_;
    enum class TransportState;

    unsigned id_counter_ = 0;

    IMessageReceiver *messageReceiver = nullptr;
    IConnectionReceiver *connectionReceiver = nullptr;

    websocketpp::client<websocketpp::config::asio_client> client_;
    websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;
    websocketpp::connection_hdl m_hdl;

    TransportState m_status;
};
}
