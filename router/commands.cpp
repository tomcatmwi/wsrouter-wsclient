//  commands.cpp
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include <cctype>
#include <sys/time.h>
#include <unistd.h>
#include <ctime>

#include "./constants.hpp"
#include "./asio_ws.hpp"
#include "../core/utils.hpp"

//  Analyze command line ---------------------------------------------------------------------------------------------------------------
bool process_args(int argc, char* argv[]) {
	
	for (int i = 1; i < argc; ++i) {

      	//  Help
      	if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
          	  std::cout << help_text << std::endl;                            
          	  return false;
      	}

          //  Version
      	if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-v") == 0) {
          	  std::cout << "wsclient " << version << std::endl; 
          	  return false;
      	}        

      	//  Logging on/off
      	if (std::strcmp(argv[i], "--log") == 0 || std::strcmp(argv[i], "-l") == 0)
          	  logging_enabled = true;

      	//  Verbose logging on/off
      	if (std::strcmp(argv[i], "--verbose") == 0) {
          	  logging_enabled = true;
          	  logging_verbose = true;
      	}

      	//  Maximum number of connections
      	if (i > 0 && (std::strcmp(argv[i-1], "--connections") == 0 || std::strcmp(argv[i-1], "-c") == 0) && argv[i] && *argv[i]) { 
      	  auto value = string_to_int(argv[i], 1, 64);
      	  if (!value || !value.value()) {
      	    std::cout << "Invalid --connections value" << std::endl;
      	    return false;
      	  }
          
      	  maxConnections = value.value();
      	}		

      	//	Websocket port
      	if (i > 0 && (std::strcmp(argv[i-1], "--port") == 0 || std::strcmp(argv[i-1], "-p") == 0) && argv[i] && *argv[i]) { 
          auto value = string_to_int(argv[i], 0, 65535);

      	  if (!value || !value.value()) {
      	    std::cout << "Invalid --port value" << std::endl;
      	    return false;
      	  }
      	  
      	  port = value.value();
      	}
	}

  return true;
}

//  Registers a client as confirmed upon receiving "hello" or other command with ID
void handle_hello(websocketpp::connection_hdl hdl, const std::string& id) {
  if (!is_valid_id(id)) {
      send_error(hdl, id, 4, "Invalid sender id: \"" + id + "\"");
      return;
  }

  //  Enforce connection limit
  if (get_client_count() >= maxConnections) {
      send_error(hdl, id, 7, "Router is full");
      log("ERROR", "Can't confirm client \"" + id + "\": Maximum number of connections (" + std::to_string(maxConnections) + ") reached.");
      return;
  }

  for (auto it = unconfirmed_clients.begin(); it != unconfirmed_clients.end(); ++it) {
      if (!it->hdl.expired() && it->hdl.lock() == hdl.lock()) {
          if (clients.count(id)) {
              disconnect_client(id, clients[id].hdl);
          }
          clients[id] = std::move(*it);
          unconfirmed_clients.erase(it);
          return;
      }
  }

  send_message(hdl, "router::0::::hello " + id);
}

//  Implementations of router commands
void handle_command(websocketpp::connection_hdl hdl, const std::string& sender, const std::vector<std::string>& parts) {

  if (parts.size() < 3) {
      send_error(hdl, sender, 1, "Message could not be parsed");
      return;
  }
  std::string command = parts[2];

  //  -------------------------------------------------------------------------------------------------------------------
  //  "hello"
  //  Identifies a new client
  //  -------------------------------------------------------------------------------------------------------------------
  if (command == "hello") {
      if (parts.size() < 3) {
          send_error(hdl, sender, 2, "Message is incomplete");
          return;
      }
      handle_hello(hdl, sender);            
  } else 
  
  //  -------------------------------------------------------------------------------------------------------------------
  //  "ping"
  //  -------------------------------------------------------------------------------------------------------------------
  if (command == "ping") {
      send_message(hdl, "router::0::::pong");
      log("LOG", "Ping by " + sender);
      return;
  } else

  //  -------------------------------------------------------------------------------------------------------------------
  //  "version"
  //  Returns version string
  //  -------------------------------------------------------------------------------------------------------------------
  if (command == "version") {
      send_message(hdl, "router::0::::" + version);
  } else
  
  //  -------------------------------------------------------------------------------------------------------------------
  //  "clients"
  //  Gets list of connected clients
  //  -------------------------------------------------------------------------------------------------------------------
  if (command == "clients") {
      if (parts.size() < 4) {
          send_error(hdl, sender, 2, "Message is incomplete");
          return;
      }
      std::string target = parts[3];

      //  Get all clients
      if (target == "*") {
          std::string list;
          for (const auto& [id, _] : clients) {
              if (!list.empty()) list += ",";
              list += id;
          }
          send_message(hdl, "router::0::::" + (list.empty() ? "None" : list));
      } else 
      
      //  Get number of confirmed and unconfirmed clients
      if (target == "") {
          send_message(hdl, "router::0::::" + std::to_string(clients.size()) + "," + std::to_string(unconfirmed_clients.size()));
      } else 
      
      //  Get if specific client is connected
      if (clients.count(target)) {
          send_message(hdl, "router::0::::" + target);
      } else {
          send_error(hdl, sender, 3, "Client \"" + target + "\" is not connected to server");
      }
  } else

  //  -------------------------------------------------------------------------------------------------------------------
  //  "disconnect"
  //  Forces the router to drop a connected client
  //  -------------------------------------------------------------------------------------------------------------------
  if (command == "disconnect") {
      if (parts.size() < 4) {
          send_error(hdl, sender, 2, "Message is incomplete");
          return;
      }
      
      std::string target = parts[3];

      if (target == "*" || target == "") {

          if (target == "*") {
              for (auto& [id, client] : clients) {
                  if (!client.hdl.expired()) {
                      wsrouter.close(client.hdl, websocketpp::close::status::normal, "Disconnected by router");
                  }
              }
              clients.clear();
          }

          for (auto& uc : unconfirmed_clients) {
              if (!uc.hdl.expired()) {
                  wsrouter.close(uc.hdl, websocketpp::close::status::normal, "Disconnected by router");
              }
              uc = Client();
          }
          unconfirmed_clients.clear();

      } else if (!is_valid_id(target)) {
          send_error(hdl, sender, 4, "Invalid recipient id: \"" + target + "\"");
      } else if (clients.count(target)) {
          disconnect_client(target, clients[target].hdl);
          send_message(hdl, "router::0::::Client " + target + " disconnected.");
      } else {
          send_error(hdl, sender, 3, "Client \"" + target + "\" is not connected to server");
      }
  } else   
  
  //  -------------------------------------------------------------------------------------------------------------------
  //  Final error handler - command was not recognized
  //  -------------------------------------------------------------------------------------------------------------------
  {
      send_error(hdl, sender, 3, "Invalid command: \"" + command + "\"");
  }  
}

//	Commands processor -----------------------------------------------------------------------------------------------------------------
void process_commands(websocketpp::connection_hdl hdl, std::string payload) {

  log("RECV", payload);

  //  Get message parts
  std::vector<std::string> parts = split(payload, "::");

  if (parts.size() < 3) {
    send_error(hdl, parts.size() > 1 ? parts[1] : "", 2, "Message is incomplete");
    return;
  }

  //  Get message parts
  std::string recipient = parts[0];
  std::string sender_id = parts[1];

  //  Auto-register previously unconfirmed client
  bool is_unconfirmed = true;
  for (const auto& c : clients) {
      if (c.second.hdl.lock() == hdl.lock()) {
          is_unconfirmed = false;
          break;
      }
  }

  if (is_unconfirmed && !sender_id.empty())
      handle_hello(hdl, sender_id);

  // Validate client IDs retrieved from the message
  if (!is_valid_id(sender_id)) {
      send_error(hdl, sender_id, 4, "Invalid sender id: \"" + sender_id + "\"");
      return;
  }
  
  if (!is_valid_id(recipient)) {
      send_error(hdl, sender_id, 5, "Invalid recipient id: \"" + sender_id + "\"");
      return;
  }

  std::string expects_reply;
  std::string reply_to;
  std::string content;  

  //  Handle router commands
  //  
  if (recipient == "router" && sender_id != "router" && reply_to != "router") {
    expects_reply = '1';
    reply_to = parts[1];
    content = join(parts, "::", 2);

    handle_command(hdl, sender_id, parts);
    return;
  }

  expects_reply = parts[2];
  reply_to = parts[3];
  content = join(parts, "::", 4);  

  if (sender_id == "router" || reply_to == "router") {
      send_error(hdl, sender_id, 6, "The router cannot be marked as sender, or be replied to.");
      return;
  }

  //  Forward message to recipient ---------------------------------------------------------------------------------

  std::string truncated_msg = join(parts, "::", 1);

  //  Send to all clients
  if (recipient == "*") {
      for (const auto& [id, client] : clients) {
          if (!client.hdl.expired() && client.id != sender_id) {
              send_message(client.hdl, truncated_msg);
          }
      }
  } else 

  //  Send to single client
  if (clients.count(recipient)) {
      if (!clients[recipient].hdl.expired()) {
          send_message(clients[recipient].hdl, truncated_msg);
      }
  } 
  
  //  Client not found, send error
  else {
      send_error(hdl, sender_id, 3, "Client \"" + recipient + "\" is not connected to server");
  }

}
