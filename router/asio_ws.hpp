//  asio_ws.hpp
#pragma once

#include <functional>
#include <asio/steady_timer.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

asio::io_service& get_io_service();
extern asio::io_context io;

bool init_websocket(std::function<void(websocketpp::connection_hdl, std::string)> on_message);
void close_websocket();
void send_message(websocketpp::connection_hdl hdl, const std::string& data);
void send_error(websocketpp::connection_hdl hdl, const std::string& sender, const int code, const std::string& error);
int get_client_count();
void disconnect_client(const std::string& id, websocketpp::connection_hdl hdl);

//  Unconfirmed and confirmed Websocket clients
struct Client {
    websocketpp::connection_hdl hdl;
    std::string id;
    Client(websocketpp::connection_hdl h = websocketpp::connection_hdl(), const std::string& i = "")
        : hdl(h), id(i) {}
};

extern std::vector<Client> unconfirmed_clients;
extern std::unordered_map<std::string, Client> clients;
extern websocketpp::connection_hdl hdl;
extern websocketpp::server<websocketpp::config::asio> wsrouter;