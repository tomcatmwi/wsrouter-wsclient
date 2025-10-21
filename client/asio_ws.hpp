//  asio_ws.hpp
#pragma once

#include <functional>

#define ASIO_STANDALONE
#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#include <websocketpp/client.hpp>

asio::io_context& get_io_service();
bool init_websocket(std::function<void(std::string)> on_message);
void send(const std::string& data);
void shutdown();
void close_websocket();
