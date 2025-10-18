// constants.hpp
#pragma once
#include <string>

//  Version number and build time
extern const std::string version;

//  Logging flags
extern bool logging_enabled;
extern bool logging_verbose;

//  PID filename
extern std::string pid_file;

//  Server port
extern int port;

//  Maximum number of connections
extern int maxConnections;

//  Help text
extern std::string help_text;

//  Websocket host name
extern const std::string ws_id;
