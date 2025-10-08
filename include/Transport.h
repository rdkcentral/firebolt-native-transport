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

class Transport // TODO: interface for mocking
{
private:
    using client = websocketpp::client<websocketpp::config::asio_client>;
    using message_ptr = websocketpp::config::asio_client::message_type::ptr;

    unsigned id_counter_ = 0;
    client client_;
    client::connection_ptr connection_;
    std::atomic<bool> connected_ = false;
    std::thread runner_thread_;
    ITransportReceiver *transportReceiver_ = nullptr;
    std::atomic<bool> stop_ = false;

private:
    void on_message(client* client_, websocketpp::connection_hdl hdl, message_ptr msg) {
        if (transportReceiver_ != nullptr) {
            nlohmann::json jsonMsg = nlohmann::json::parse(msg->get_payload());
#ifdef DEBUG_TRANSPORT
            std::cout << "on_message: " << "msg: " << jsonMsg.dump() << std::endl;
#endif
            transportReceiver_->Receive(jsonMsg);
        }
    }

    void runner() {
        while (!stop_) {
            try {
                connected_ = true;
                client_.run();
            } catch (websocketpp::exception const & e) {
                connected_ = false;
                std::cout << "runner, " << e.what() << std::endl;
            }
        }
    }

    Firebolt::Error mapError(websocketpp::lib::error_code error)
    {
        using EV = websocketpp::error::value;
        switch (error.value()) {
            case EV::con_creation_failed:
            case EV::unrequested_subprotocol:
            case EV::http_connection_ended:
            case EV::open_handshake_timeout:
            case EV::close_handshake_timeout:
            case EV::invalid_port:
            case EV::rejected:
                return Firebolt::Error::Timedout;
            case EV::general:
            default:
                return Firebolt::Error::General;
        }
    }

public:
    Transport() = default;
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;
    Transport(Transport&&) = delete;
    Transport& operator=(Transport&&) = delete;
    virtual ~Transport() {
        stop_ = true;
        client_.stop();
        if (runner_thread_.joinable()) {
            runner_thread_.join();
        }
    }

    Firebolt::Error Connect(std::string url)
    {
        try {
            SetLogging(
                websocketpp::log::alevel::all,
                (websocketpp::log::alevel::frame_header | websocketpp::log::alevel::frame_payload | websocketpp::log::alevel::control));

            client_.init_asio();
            client_.set_message_handler(websocketpp::lib::bind(&Transport::on_message, this, &client_, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));

            websocketpp::lib::error_code ec;
            connection_ = client_.get_connection(url, ec);
            if (ec) {
                std::cout << "could not create connection, " << ec.message() << std::endl;
                return Firebolt::Error::NotConnected;
            }

            client_.connect(connection_);

            runner_thread_ = std::thread(std::bind(&Transport::runner, this));
            connected_ = true;
        } catch (websocketpp::exception const & e) {
            std::cout << e.what() << std::endl;
            return Firebolt::Error::General;
        }
        return Firebolt::Error::None;
    }

    void SetTransportReceiver(ITransportReceiver *transportReceiver)
    {
        transportReceiver_ = transportReceiver;
    }

    unsigned GetNextMessageID()
    {
        return ++id_counter_;
    }

    Firebolt::Error Send(const std::string &method, const nlohmann::json &params, const unsigned id)
    {
        if (!connected_) {
            return Firebolt::Error::NotConnected;
        }

        websocketpp::lib::error_code ec;

        nlohmann::json msg;
        msg["jsonrpc"] = "2.0";
        msg["id"] = id;
        msg["method"] = method;
        msg["params"] = params;
#ifdef DEBUG_TRANSPORT
        std::cout << "send: " << "msg: " << msg.dump() << std::endl;
#endif
        client_.send(client_.get_con_from_hdl(connection_->get_handle()), to_string(msg), websocketpp::frame::opcode::text, ec);
        if (ec) {
            std::cout << "Send failed, " << ec.message() << std::endl;
            return mapError(ec);
        }
        return Firebolt::Error::None;
    }

    Firebolt::Error SendResponse(const unsigned id, const std::string &response)
    {
        if (!connected_) {
            return Firebolt::Error::NotConnected;
        }

        websocketpp::lib::error_code ec;

        nlohmann::json msg;
        msg["jsonrpc"] = "2.0";
        msg["id"] = id;
        msg["result"] = nlohmann::json::parse(response);
        client_.send(client_.get_con_from_hdl(connection_->get_handle()), to_string(msg), websocketpp::frame::opcode::text, ec);
        if (ec) {
            std::cout << "Send failed, " << ec.message() << std::endl;
            return mapError(ec);
        }
        return Firebolt::Error::None;
    }

    void SetLogging(websocketpp::log::level include, websocketpp::log::level exclude = 0)
    {
        client_.set_access_channels(include);
        client_.clear_access_channels(exclude);
    }
};
}
