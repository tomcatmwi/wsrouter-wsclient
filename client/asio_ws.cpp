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

typedef websocketpp::client<websocketpp::config::asio_client> client;

#include "constants.hpp"
#include "../core/utils.hpp"

//  Internal variables
static websocketpp::connection_hdl hdl;
static websocketpp::lib::error_code ec;
static client wsclient;
static std::atomic<bool> quitting{false};

//  ---------------------------------------------------------------------------------------------------------------------

asio::io_context& get_io_service() {
    return wsclient.get_io_service();
}

//	Shutdown ------------------------------------------------------------------------------------------------------------
void close_websocket() {

    if (quitting.exchange(true)) 
  	  return;
  	  
    wsclient.stop_perpetual();
    wsclient.stop();

    websocketpp::lib::error_code e;
    wsclient.close(hdl, websocketpp::close::status::going_away, "", e);

    if (e && e.value() != websocketpp::error::bad_connection)
      log("ERROR", "close() error: " + e.message());
    else
	  log("LOG", "Websocket client terminated.");
}

//  Send Websocket message (thread safe) --------------------------------------------------------------------------------

void send(const std::string& data) {
    asio::post(wsclient.get_io_service(), [data]() {
        websocketpp::lib::error_code ec;
        wsclient.send(hdl, data, websocketpp::frame::opcode::text, ec);
        if (ec)
            log("ERROR", "Websocket send failed: " + ec.message());
    });
}

//  Start Websocket service ---------------------------------------------------------------------------------------------
//  The event handler function to process incoming messages is passed as argument

std::string ws_fullhost;
bool init_websocket(std::function<void(std::string)> on_message) {

	static int retry_counter = 0;
	std::function<void()> schedule_reconnect;

	ws_fullhost = "ws://" + ws_host + ":" + port;
    log("LOG", "Connecting to Websocket router at " + ws_fullhost + "...");
    
    //	STFU - no console messages from wsclient
    wsclient.clear_access_channels(websocketpp::log::alevel::all);
    wsclient.clear_error_channels(websocketpp::log::elevel::all);
    
    wsclient.init_asio();
    wsclient.start_perpetual();

	//	Connection handler      
    wsclient.set_open_handler([](websocketpp::connection_hdl h) {
        hdl = h;
        log("LOG", "Connected to: " + ws_fullhost + " as " + ws_id);
        send("router::" + ws_id + "::hello::" + ws_id + "::");
        retry_counter = 0;
    });        
        
    //	Automatic reconnection routine
	schedule_reconnect = [&](){
        if (quitting)
        return;

        if (retries != 0 && retry_counter >= retries) {
            log("ERROR", "Connection retry limit (" + std::to_string(retries) + ") reached. Giving up!");
            close_websocket();
            return;
        }

        ++retry_counter;

        //  Deprecated: legacy code for old Websocket++ version using set_timer
        wsclient.set_timer(retry_interval, [&](websocketpp::lib::error_code const& tec) {

        // For later upgrade after wsocketpp is fixed: Use ASIO steady_timer
        
        // auto timer = std::make_shared<asio::steady_timer>(wsclient.get_io_service());
        // timer->expires_after(std::chrono::milliseconds(retry_interval));
        // timer->async_wait([timer, &schedule_reconnect](const std::error_code& tec) {

            if (tec || quitting) return;
            
            websocketpp::lib::error_code err;
            auto con = wsclient.get_connection(ws_fullhost, err);

            if (err) {
                log("ERROR", "Reconnect get_connection failed: " + err.message());
                schedule_reconnect();
                return;
            }
            
            log("LOG", "Attempting to reconnect (" + std::to_string(retry_counter) + (retries ? "/" + std::to_string(retries) : "") + ")...");

            con->set_open_handshake_timeout(ws_handshake_timeout);
            wsclient.connect(con);
        });

	};
    
    //	Connection closure handler
	wsclient.set_close_handler([&](websocketpp::connection_hdl h){
	  if (quitting)
		return;
	
  	  auto c  = wsclient.get_con_from_hdl(h);
  	  auto ec = c->get_ec();
  	  int code = c->get_remote_close_code();
  	  
  	  if (ec || code != websocketpp::close::status::normal)
        log("ERROR", "Connection lost: " + (ec ? ec.message() : "abnormal close"));
  	  else
        log("LOG", "Connection closed cleanly by peer");
        
      schedule_reconnect();
	});
	
	//	Connection failure handler
    wsclient.set_fail_handler([&](websocketpp::connection_hdl h){
	  if (quitting)
		return;
		
  	  auto c  = wsclient.get_con_from_hdl(h);
  	  auto ec = c->get_ec();
  	  log("ERROR", "Connection failure: " + (ec ? ec.message() : "unknown"));
  	  schedule_reconnect();
	});

    //	Incoming message handler
    wsclient.set_message_handler([on_message](websocketpp::connection_hdl, client::message_ptr msg) {
    	on_message(msg->get_payload());
    });  

	//	Initialize Websocket connection
    try {
        client::connection_ptr con = wsclient.get_connection(ws_fullhost, ec);
        if (ec) {
            log("ERROR", "Connection error: " + ec.message());
            return false;
        }
        wsclient.connect(con);

    }
    catch (const std::exception& e) {
        log("ERROR", "Hostname \"" + ws_host + "\" cannot be resolved: " + std::string(e.what()));
        log("ERROR", "Please install glibc 2.41 or use IP address instead of a hostname.");
        log("ERROR", "You can check your current glibc version with 'ldd --version'.)");
        return false;
    }

    try {
        wsclient.run();
    } 
    catch (const std::exception& e) {
        log("ERROR", "Unable to launch Websocket client: " + std::string(e.what()));
        return false;
    }

    log("LOG", "Websocket client initialized");
    return true;
}
