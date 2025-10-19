//  Compatibility wrapper to force ASIO and Websocket++ play nice

#ifndef ASIO_WRAPPER_HPP
#define ASIO_WRAPPER_HPP
#define ASIO_STANDALONE
#include <asio/io_context.hpp>
namespace websocketpp::lib::asio {
    using io_service = ::asio::io_context;
}
#endif
