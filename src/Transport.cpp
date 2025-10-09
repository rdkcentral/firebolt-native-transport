#include "Transport.h"

namespace FireboltSDK::Transport
{

using client = websocketpp::client<websocketpp::config::asio_client>;
using message_ptr = websocketpp::config::asio_client::message_type::ptr;

enum class Transport::TransportState {
    NotStarted,
    Connected,
    Disconnected,
};

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

Transport::~Transport()
{
    if (m_status == TransportState::NotStarted) {
        return;
    }
    client_.stop_perpetual();

    if (m_status != TransportState::Connected) {
        return;
    }

    websocketpp::lib::error_code ec;
    client_.close(m_hdl, websocketpp::close::status::going_away, "", ec);
    if (ec) {
        std::cout << "> Error closing connection " << ec.message() << std::endl;
    }

    m_thread->join();
}

void Transport::start()
{
    client_.clear_access_channels(websocketpp::log::alevel::all);
    client_.clear_error_channels(websocketpp::log::elevel::all);

    client_.init_asio();
    client_.start_perpetual();

    m_thread.reset(new websocketpp::lib::thread(&client::run, &client_));
    m_status = TransportState::Disconnected;
}

Firebolt::Error Transport::Connect(std::string url)
{
    if (m_status == TransportState::NotStarted) {
        start();
    }

    SetLogging(
        websocketpp::log::alevel::all,
        (websocketpp::log::alevel::frame_header | websocketpp::log::alevel::frame_payload | websocketpp::log::alevel::control));

    websocketpp::lib::error_code ec;
    client::connection_ptr con = client_.get_connection(url, ec);

    if (ec) {
        std::cout << "Connect initialization error: " << ec.message() << std::endl;
        return Firebolt::Error::NotConnected;
    }

    m_hdl = con->get_handle();

    con->set_open_handler(websocketpp::lib::bind(
        &Transport::on_open,
        this,
        &client_,
        websocketpp::lib::placeholders::_1
    ));
    con->set_fail_handler(websocketpp::lib::bind(
        &Transport::on_fail,
        this,
        &client_,
        websocketpp::lib::placeholders::_1
    ));
    con->set_close_handler(websocketpp::lib::bind(
        &Transport::on_close,
        this,
        &client_,
        websocketpp::lib::placeholders::_1
    ));

    con->set_message_handler(websocketpp::lib::bind(
        &Transport::on_message,
        this,
        websocketpp::lib::placeholders::_1,
        websocketpp::lib::placeholders::_2
    ));

    client_.connect(con);

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
    if (m_status != TransportState::Connected) {
        return Firebolt::Error::NotConnected;
    }

    nlohmann::json msg;
    msg["jsonrpc"] = "2.0";
    msg["id"] = id;
    msg["method"] = method;
    msg["params"] = params;
#ifdef DEBUG_TRANSPORT
    std::cout << "send: " << "msg: " << msg.dump() << std::endl;
#endif

    websocketpp::lib::error_code ec;

    client_.send(m_hdl, to_string(msg), websocketpp::frame::opcode::text, ec);
    if (ec) {
        std::cout << "> Error sending message: " << ec.message() << std::endl;
        return mapError(ec);
    }

    return Firebolt::Error::None;
}

Firebolt::Error Transport::SendResponse(const unsigned id, const std::string &response)
{
    if (m_status != TransportState::Connected) {
        return Firebolt::Error::NotConnected;
    }

    websocketpp::lib::error_code ec;

    nlohmann::json msg;
    msg["jsonrpc"] = "2.0";
    msg["id"] = id;
    msg["result"] = nlohmann::json::parse(response);
    client_.send(m_hdl, to_string(msg), websocketpp::frame::opcode::text, ec);
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

void Transport::on_message(websocketpp::connection_hdl hdl, websocketpp::client<websocketpp::config::asio_client>::message_ptr msg)
{
    if (msg->get_opcode() != websocketpp::frame::opcode::text) {
        return;
    }
    if (transportReceiver_ == nullptr) {
        return;
    }
    nlohmann::json jsonMsg = nlohmann::json::parse(msg->get_payload());
#ifdef DEBUG_TRANSPORT
    std::cout << "on_message: " << "msg: " << jsonMsg.dump() << std::endl;
#endif
    transportReceiver_->Receive(jsonMsg);
}

void Transport::on_open(websocketpp::client<websocketpp::config::asio_client>* c, websocketpp::connection_hdl hdl)
{
    m_status = TransportState::Connected;

    client::connection_ptr con = c->get_con_from_hdl(hdl);
    m_server = con->get_response_header("Server");
}

void Transport::on_close(websocketpp::client<websocketpp::config::asio_client>* c, websocketpp::connection_hdl hdl)
{
    m_status = TransportState::Disconnected;
    client::connection_ptr con = c->get_con_from_hdl(hdl);
    std::stringstream s;
    s << "close code: " << con->get_remote_close_code() << " ("
        << websocketpp::close::status::get_string(con->get_remote_close_code())
        << "), close reason: " << con->get_remote_close_reason();
    m_error_reason = s.str();
}

void Transport::on_fail(websocketpp::client<websocketpp::config::asio_client>* c, websocketpp::connection_hdl hdl)
{
    m_status = TransportState::Disconnected;

    client::connection_ptr con = c->get_con_from_hdl(hdl);
    m_server = con->get_response_header("Server");
    m_error_reason = con->get_ec().message();
}

}