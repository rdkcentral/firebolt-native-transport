#include "transport.h"
#include <chrono>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

using namespace Firebolt::Transport;

class TransportTest : public ::testing::Test
{
protected:
    Transport transport;
};

TEST_F(TransportTest, GetNextMessageID)
{
    unsigned firstId = transport.getNextMessageID();
    unsigned secondId = transport.getNextMessageID();
    EXPECT_EQ(firstId, 1);
    EXPECT_EQ(secondId, 2);
}

TEST_F(TransportTest, SendWithoutConnection)
{
    nlohmann::json params;
    Firebolt::Error err = transport.send("test.method", params, 1);
    EXPECT_EQ(err, Firebolt::Error::NotConnected);
}

TEST_F(TransportTest, DisconnectWithoutStart)
{
    Firebolt::Error err = transport.disconnect();
    EXPECT_EQ(err, Firebolt::Error::None);
}

class TransportIntegrationTest : public ::testing::Test
{
protected:
    using server = websocketpp::server<websocketpp::config::asio>;
    using connection_hdl = websocketpp::connection_hdl;

    server m_server;
    std::unique_ptr<std::thread> m_serverThread;
    const std::string m_uri = "ws://localhost:9002";
    bool m_serverStarted = false;

    void SetUp() override
    {
        try
        {
            m_server.init_asio();
            m_server.set_reuse_addr(true);

            m_server.set_message_handler(
                [this](connection_hdl hdl, server::message_ptr msg)
                {
                    try
                    {
                        m_server.send(hdl, msg->get_payload(), msg->get_opcode());
                    }
                    catch (const websocketpp::exception& e)
                    {
                        FAIL() << "Server failed to send message: " << e.what();
                    }
                });

            websocketpp::log::level include = websocketpp::log::alevel::all;
            websocketpp::log::level exclude = (websocketpp::log::alevel::frame_header |
                                               websocketpp::log::alevel::frame_payload |
                                               websocketpp::log::alevel::control);
            m_server.set_access_channels(include);
            m_server.clear_access_channels(exclude);
            m_server.listen(9002);
            m_server.start_accept();
            m_serverThread = std::make_unique<std::thread>([this]() { m_server.run(); });
            m_serverStarted = true;
        }
        catch (const websocketpp::exception& e)
        {
            FAIL() << "Failed to start websocket server: " << e.what();
        }
    }

    void TearDown() override
    {
        if (m_serverStarted)
        {
            m_server.stop_listening();
            m_server.stop();
            if (m_serverThread && m_serverThread->joinable())
            {
                m_serverThread->join();
            }
        }
    }
};

TEST_F(TransportIntegrationTest, ConnectAndDisconnect)
{
    Transport transport;
    std::promise<bool> connectionPromise;
    auto connectionFuture = connectionPromise.get_future();

    auto onConnectionChange = [&](bool connected, const Firebolt::Error& /*err*/)
    {
        if (connected)
        {
            connectionPromise.set_value(true);
        }
    };

    auto onMessage = [&](const nlohmann::json& /*msg*/) {};

    Firebolt::Error err = transport.connect(m_uri, onMessage, onConnectionChange);
    ASSERT_EQ(err, Firebolt::Error::None);

    auto status = connectionFuture.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(status, std::future_status::ready) << "Connection timed out";
    EXPECT_TRUE(connectionFuture.get());

    err = transport.disconnect();
    EXPECT_EQ(err, Firebolt::Error::None);
}

TEST_F(TransportIntegrationTest, SendAndReceiveMessage)
{
    Transport transport;
    std::promise<bool> connectionPromise;
    auto connectionFuture = connectionPromise.get_future();
    std::promise<nlohmann::json> messagePromise;
    auto messageFuture = messagePromise.get_future();

    auto onConnectionChange = [&](bool connected, const Firebolt::Error& /*err*/)
    {
        if (connected)
        {
            connectionPromise.set_value(true);
        }
    };

    auto onMessage = [&](const nlohmann::json& msg) { messagePromise.set_value(msg); };

    Firebolt::Error err = transport.connect(m_uri, onMessage, onConnectionChange);
    ASSERT_EQ(err, Firebolt::Error::None);

    auto status = connectionFuture.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(status, std::future_status::ready) << "Connection timed out";
    ASSERT_TRUE(connectionFuture.get());

    nlohmann::json params = {{"key", "value"}};
    unsigned msgId = transport.getNextMessageID();
    err = transport.send("test.method", params, msgId);
    EXPECT_EQ(err, Firebolt::Error::None);

    auto msgStatus = messageFuture.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(msgStatus, std::future_status::ready) << "Message response timed out";

    nlohmann::json receivedMsg = messageFuture.get();
    EXPECT_EQ(receivedMsg["id"], msgId);
    EXPECT_EQ(receivedMsg["method"], "test.method");
    EXPECT_EQ(receivedMsg["params"]["key"], "value");

    err = transport.disconnect();
    EXPECT_EQ(err, Firebolt::Error::None);
}
