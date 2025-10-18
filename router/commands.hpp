//  commands.hpp
#pragma once

bool process_args(int argc, char* argv[]);
void process_commands(websocketpp::connection_hdl hdl, std::string payload);
void handle_hello(websocketpp::connection_hdl hdl, const std::string& id);
void handle_command(websocketpp::connection_hdl hdl, const std::string& sender, const std::vector<std::string>& parts);

