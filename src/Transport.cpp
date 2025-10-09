#include "Transport.h"

namespace FireboltSDK::Transport
{

using client = websocketpp::client<websocketpp::config::asio_client>;
using message_ptr = websocketpp::config::asio_client::message_type::ptr;

static Firebolt::Error mapError(websocketpp::lib::error_code error)
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

Transport::~Transport() {
    stop_ = true;
    client_.stop();
    if (runner_thread_.joinable()) {
        runner_thread_.join();
    }
}

void Transport::on_message(client* client_, websocketpp::connection_hdl hdl, message_ptr msg) {
    if (transportReceiver_ != nullptr) {
        nlohmann::json jsonMsg = nlohmann::json::parse(msg->get_payload());
#ifdef DEBUG_TRANSPORT
        std::cout << "on_message: " << "msg: " << jsonMsg.dump() << std::endl;
#endif
        transportReceiver_->Receive(jsonMsg);
    }
}

void Transport::runner() {
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

Firebolt::Error Transport::Connect(std::string url)
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

void Transport::SetTransportReceiver(ITransportReceiver *transportReceiver)
{
    transportReceiver_ = transportReceiver;
}

unsigned Transport::GetNextMessageID()
{
    return ++id_counter_;
}

Firebolt::Error Transport::Send(const std::string &method, const nlohmann::json &params, const unsigned id)
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

Firebolt::Error Transport::SendResponse(const unsigned id, const std::string &response)
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

void Transport::SetLogging(websocketpp::log::level include, websocketpp::log::level exclude)
{
    client_.set_access_channels(include);
    client_.clear_access_channels(exclude);
}
}