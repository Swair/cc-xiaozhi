#pragma once

#include <map>
#include <string>
#include <vector>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

#include "Log.h"

namespace cill {

class WebSocketClient {
public:
    typedef websocketpp::client<websocketpp::config::asio_tls_client> wsclient;
    typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;
    typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;

    WebSocketClient() = delete;

    WebSocketClient(const std::string& ws_url) {
        try {
            ws_client_.clear_access_channels(websocketpp::log::alevel::all);
            ws_client_.init_asio();

            SetOnTlsCallback([this]() -> context_ptr { return this->on_tls_init(); });

            ws_url_ = ws_url;

        } catch (const std::exception& e) {
            std::cerr << "WebSocket init error: " << e.what() << std::endl;
        }
    }

    ~WebSocketClient() { ws_client_.stop(); }

    void SetWsHeaders(const std::map<std::string, std::string>& ws_headers) {
        ws_headers_ = ws_headers;
    }

    void start() {
        websocketpp::lib::error_code ec;
        auto con = ws_client_.get_connection(ws_url_, ec);
        if (ec) {
            std::cerr << "Connection error: " << ec.message() << std::endl;
            return;
        }

        // 设置请求头
        for (auto header : ws_headers_) {
            con->append_header(header.first, header.second);
        }

        ws_client_.connect(con);

        // !!! run
        ws_client_.run();
    }

    void send_text(const std::string& message) {
        websocketpp::lib::error_code ec;
        ws_client_.send(hdl_, message, websocketpp::frame::opcode::text, ec);
        if (ec) {
            std::cerr << "Send error: " << ec.message() << std::endl;
        }
        INFO(">> {}", message);
    }

    void send_binary(const void* data, size_t len) {
        websocketpp::lib::error_code ec;
        ws_client_.send(hdl_, data, len, websocketpp::frame::opcode::binary, ec);
        if (ec) {
            std::cerr << "Send error: " << ec.message() << std::endl;
        }
    }

    void SetOnTlsCallback(std::function<context_ptr(void)> cb) {
        on_tls_cb_ = cb;
        ws_client_.set_tls_init_handler(
            [this](websocketpp::connection_hdl) { return this->on_tls_cb_(); });
    }

    void SetOnOpenCallback(std::function<std::string(void)> cb) {
        on_open_cb_ = cb;
        ws_client_.set_open_handler([this](websocketpp::connection_hdl hdl) {
            hdl_ = hdl;
            std::string res = this->on_open_cb_();
            send_text(res);
        });
    }

    void SetOnCloseCallback(std::function<void(void)> cb) {
        on_close_cb_ = cb;
        ws_client_.set_close_handler(
            [this](websocketpp::connection_hdl hdl) { this->on_close_cb_(); });
    }

    void SetOnFailCallback(std::function<void(void)> cb) {
        on_fail_cb_ = cb;
        ws_client_.set_fail_handler(
            [this](websocketpp::connection_hdl hdl) { this->on_fail_cb_(); });
    }

    void SetOnMessageCallback(std::function<std::string(const std::string&, bool)> cb) {
        on_message_cb_ = cb;
        ws_client_.set_message_handler([this](websocketpp::connection_hdl hdl, message_ptr msg) {
            bool binary = (msg->get_opcode() == websocketpp::frame::opcode::binary);
            std::string res = this->on_message_cb_(msg->get_payload(), binary);
            if (res.size() > 0) {
                send_text(res);
            }
        });
    }

private:
    context_ptr on_tls_init() {
        INFO("=========on_tls_init==============");
        context_ptr ctx =
            std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);

        try {
            ctx->set_options(boost::asio::ssl::context::default_workarounds |
                             boost::asio::ssl::context::no_sslv2 |
                             boost::asio::ssl::context::no_sslv3 |
                             boost::asio::ssl::context::single_dh_use);

            // 禁用证书验证（仅用于测试）
            ctx->set_verify_mode(boost::asio::ssl::verify_none);

            // 生产环境应配置证书验证
            // ctx->set_verify_mode(boost::asio::ssl::verify_peer);
            // ctx->load_verify_file("/path/to/ca.crt");

        } catch (std::exception& e) {
            std::cerr << "TLS init error: " << e.what() << std::endl;
        }
        return ctx;
    }

private:
    std::string ws_url_;
    std::map<std::string, std::string> ws_headers_;

    wsclient ws_client_;
    websocketpp::connection_hdl hdl_;

    std::function<context_ptr(void)> on_tls_cb_;
    std::function<std::string(void)> on_open_cb_;
    std::function<std::string(const std::string&, bool)> on_message_cb_;
    std::function<void()> on_close_cb_;
    std::function<void()> on_fail_cb_;
};

}  // namespace cill