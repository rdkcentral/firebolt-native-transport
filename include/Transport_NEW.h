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

#include <string>
#include <iostream>
#include <nlohmann/json.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

namespace FireboltSDK::Transport
{
    class ITransportReceiver_PP {
    public:
        virtual void Receive(const nlohmann::json& message) = 0;
    };

    class Transport_PP
    {

    private:
        using client = websocketpp::client<websocketpp::config::asio_client>;
        using message_ptr = websocketpp::config::asio_client::message_type::ptr;

        unsigned id_counter;

        client c;
        client::connection_ptr con;
        std::thread runner_thread;
        ITransportReceiver_PP *_transportReceiver = nullptr;

    private:
        void on_message(client* c, websocketpp::connection_hdl hdl, message_ptr msg) {
            std::cout << "on_message: " << "msg: " << msg->get_payload() << std::endl;
            if (_transportReceiver != nullptr) {
                _transportReceiver->Receive(nlohmann::json::parse(msg->get_payload()));
            }
        }

        void runner() {
            try {
                c.run();
            } catch (websocketpp::exception const & e) {
                std::cout << "runner, " << e.what() << std::endl;
            }
        }

        Firebolt::Error mapError(websocketpp::lib::error_code error)
        {
            using err = websocketpp::error::value;
            switch (error.value()) {
                case err::con_creation_failed:
                case err::unrequested_subprotocol:
                case err::http_connection_ended:
                case err::open_handshake_timeout:
                case err::close_handshake_timeout:
                case err::invalid_port:
                case err::rejected:
                    return Firebolt::Error::Timedout;
                case err::general:
                default:
                    return Firebolt::Error::General;
            }
        }

    public:
        virtual ~Transport_PP() {
            c.stop();
            if (runner_thread.joinable()) {
                runner_thread.join();
            }
        }

        unsigned GetNextMessageID()
        {
            return ++id_counter;
        }

        Firebolt::Error Send(const string &method, const nlohmann::json &params, const unsigned id)
        {
            websocketpp::lib::error_code ec;

            nlohmann::json msg;
            msg["jsonrpc"] = "2.0";
            msg["id"] = id;
            msg["method"] = method;
            msg["params"] = params;
            // printf("TB] to send: '%s'\n", to_string(msg).c_str());
            c.send(c.get_con_from_hdl(con->get_handle()), to_string(msg), websocketpp::frame::opcode::text, ec);
            if (ec) {
                std::cout << "Send failed, " << ec.message() << std::endl;
                return mapError(ec);
            }
            return Firebolt::Error::None;
        }

        Firebolt::Error SendResponse(const unsigned id, const std::string &response)
        {
            websocketpp::lib::error_code ec;

            nlohmann::json msg;
            msg["jsonrpc"] = "2.0";
            msg["id"] = id;
            msg["result"] = nlohmann::json::parse(response);
            // printf("TB] to send(resp): '%s'\n", to_string(msg).c_str());
            c.send(c.get_con_from_hdl(con->get_handle()), to_string(msg), websocketpp::frame::opcode::text, ec);
            if (ec) {
                std::cout << "Send failed, " << ec.message() << std::endl;
                return mapError(ec);
            }
            return Firebolt::Error::None;
        }

        void SetLogging(websocketpp::log::level include, websocketpp::log::level exclude = 0)
        {
            c.set_access_channels(include);
            c.clear_access_channels(exclude);
        }

        void Connect(std::string url)
        {
            try {
                SetLogging(
                    websocketpp::log::alevel::all,
                    (websocketpp::log::alevel::frame_header | websocketpp::log::alevel::frame_payload | websocketpp::log::alevel::control));

                c.init_asio();
                c.set_message_handler(websocketpp::lib::bind(&Transport_PP::on_message, this, &c, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));

                websocketpp::lib::error_code ec;
                con = c.get_connection(url, ec);
                if (ec) {
                    std::cout << "could not create connection, " << ec.message() << std::endl;
                    return;
                }

                c.connect(con);

                runner_thread = std::thread(std::bind(&Transport_PP::runner, this));
            } catch (websocketpp::exception const & e) {
                std::cout << e.what() << std::endl;
            }
        }

        void SetTransportReceiver(ITransportReceiver_PP *transportReceiver)
        {
            _transportReceiver = transportReceiver;
        }
    };
}
