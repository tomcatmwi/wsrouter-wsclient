//  asio_ws.cpp
#include <iostream>
#include <string>
#include <fstream>
#include <signal.h>
#include <functional>

#define ASIO_STANDALONE
#include <asio.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include "constants.hpp"
#include "commands.hpp"
#include "asio_ws.hpp"
#include "../core/utils.hpp"

//  Internal variables
websocketpp::connection_hdl hdl;
websocketpp::server<websocketpp::config::asio> wsrouter;
static websocketpp::lib::error_code ec;
asio::io_service io;

//  Flag to indicate that the program is quitting
static std::atomic<bool> quitting{false};

std::vector<Client> unconfirmed_clients;
std::unordered_map<std::string, Client> clients{};

//  ---------------------------------------------------------------------------------------------------------------------

asio::io_context& get_io_service() {
    return wsrouter.get_io_service();
}

//	Shutdown ------------------------------------------------------------------------------------------------------------
void close_websocket() {

    if (quitting.exchange(true)) 
  	  return;

    wsrouter.stop_listening();

    for (const auto& client : unconfirmed_clients) {
        websocketpp::lib::error_code ec;
        wsrouter.close(client.hdl, websocketpp::close::status::going_away, "Server shutting down", ec);
        if (ec) log("ERROR", "Failed to disconnect unconfirmed client: " + ec.message());
    }

    for (const auto& [id, client] : clients) {
        websocketpp::lib::error_code ec;
        log("LOG", "Disconnecting client: " + id);
        wsrouter.close(client.hdl, websocketpp::close::status::going_away, "Server shutting down", ec);
        if (ec) log("ERROR", "Failed to disconnect client " + id + ": " + ec.message());
    }

    wsrouter.stop_perpetual();
    wsrouter.stop();
    io.stop();
    
    websocketpp::lib::error_code e;
    wsrouter.close(hdl, websocketpp::close::status::going_away, "", e);

    if (e && e.value() != websocketpp::error::bad_connection)
      log("ERROR", "close() error: " + e.message());
    else
	  log("LOG", "Websocket router terminated.");
}

int get_client_count() {
    return static_cast<int>(unconfirmed_clients.size() + clients.size());
}

//  Removes a client
void disconnect_client(const std::string& id, websocketpp::connection_hdl hdl) {
    if (clients.erase(id)) {
        wsrouter.close(hdl, websocketpp::close::status::normal, "Disconnected by router");
        return;
    }
    for (auto& uc : unconfirmed_clients) {
        if (!uc.hdl.expired() && uc.hdl.lock() == hdl.lock()) {
            uc = Client();
            wsrouter.close(hdl, websocketpp::close::status::normal, "Disconnected by router");
            return;
        }
    }
}

//  Send Websocket message (thread safe) --------------------------------------------------------------------------------
void send_message(websocketpp::connection_hdl hdl, const std::string& data) {
    asio::post(wsrouter.get_io_service(), [hdl, data]() {
        websocketpp::lib::error_code ec;
        wsrouter.send(hdl, data, websocketpp::frame::opcode::text, ec);
        if (ec)
            log("ERROR", "Websocket send failed: " + ec.message());
        else
            log("SENT", data);

    });
}

//  Send Websocket error message (thread safe) ---------------------------------------------------------------------------
void send_error(websocketpp::connection_hdl hdl, const std::string& sender, const int code, const std::string& error) {
    asio::post(wsrouter.get_io_service(), [hdl, sender, code, error]() {

        websocketpp::lib::error_code ec;
        std::string message = "router::" + std::to_string(code) + "::::" + error;
        wsrouter.send(hdl, message, websocketpp::frame::opcode::text, ec);
        if (ec)
            log("ERROR", "Websocket send failed: " + ec.message());
        else
            log("ERROR", error);
        
    });
}

//  Start Websocket service ---------------------------------------------------------------------------------------------
//  The event handler function to process incoming messages is passed as argument

bool init_websocket(std::function<void(websocketpp::connection_hdl, std::string)> on_message) {

    log("LOG", "Opening Websocket router at port " + std::to_string(port) + "...");
    
    //	STFU - no console messages from wsrouter
    wsrouter.clear_access_channels(websocketpp::log::alevel::all);
    wsrouter.clear_error_channels(websocketpp::log::elevel::all);
    
    wsrouter.init_asio(&io);
    wsrouter.start_perpetual();

	//	Connection handler      
    wsrouter.set_open_handler([&](websocketpp::connection_hdl hdl) {
        const int conns = unconfirmed_clients.size() + clients.size();

        if (conns >= maxConnections) {
            wsrouter.close(hdl, websocketpp::close::status::normal, "Router full");
            log("ERROR", "Connection rejected: Router is full");
            return;
        }
        
        unconfirmed_clients.emplace_back(hdl);
        log("LOG", "New client connected. Current count: " + std::to_string(conns + 1));
    });
    
    //  Connection close event handler
    wsrouter.set_close_handler([&](websocketpp::connection_hdl hdl) {
        const int conns = unconfirmed_clients.size() + clients.size();

        // Remove from unconfirmed_clients
        for (auto it = unconfirmed_clients.begin(); it != unconfirmed_clients.end(); ++it) {
            if (!it->hdl.expired() && it->hdl.lock() == hdl.lock()) {
                unconfirmed_clients.erase(it);
                log("LOG", "Unconfirmed client disconnected. Current count: " + std::to_string(conns - 1));
                return;
            }
        }

        // Remove from confirmed clients
        for (auto it = clients.begin(); it != clients.end();) {
            if (!it->second.hdl.expired() && it->second.hdl.lock() == hdl.lock()) {
                log("LOG", "Client \"" + it->second.id + "\" disconnected. Current count: " + std::to_string(conns - 1));
                it = clients.erase(it);
            }
            else
                ++it;
        }
    });

    //  Message event handler
    wsrouter.set_message_handler([on_message](websocketpp::connection_hdl hdl, websocketpp::server<websocketpp::config::asio>::message_ptr msg) {
        process_commands(hdl, msg->get_payload());
    });


    // BUG: Shutdownnál dobja el a meglévő clienteket!

    //  Initialize server
    try {
        wsrouter.set_reuse_addr(true);
        wsrouter.listen(port);
        wsrouter.start_accept();
        log("LOG", "Websocket router initialized");
        wsrouter.run();
    } catch (const std::exception& e) {
        log("ERROR", std::string("Failed to start WebSocket server on port " + std::to_string(port) + ": ") + e.what());
        return false;
    }

    return true;
}
