//  asio_ws.hpp
#pragma once

#include <functional>

#include <asio/steady_timer.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

asio::io_service& get_io_service();
bool init_websocket(std::function<void(std::string)> on_message);
void send(const std::string& data);
void close_websocket();
