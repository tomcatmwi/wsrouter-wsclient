// test_ws.cpp
#define ASIO_STANDALONE
#include <asio.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

int main() {
    websocketpp::client<websocketpp::config::asio_client> client;
    client.init_asio();
    return 0;
}