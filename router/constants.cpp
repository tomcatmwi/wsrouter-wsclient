#include "constants.hpp"
#include <string>

//  Version number and build time
const std::string version = std::string("2.0.0 ") + __DATE__ + " " + __TIME__;

//  Logging flags
bool logging_enabled = true;
bool logging_verbose = false;

//  PID filename
std::string pid_file = "/tmp/wsrouter.pid";

//  Port
int port = 8080;

//  Maximum number of connections
int maxConnections = 8;

//  Websocket client ID - it's always "router"
const std::string ws_id = "router";

//  Help text
std::string help_text =
    "wsrouter - The Ultralight IoT Websocket Router\n"
    "Version and build date: " + version + "\n\n"

    "Command line arguments:\n\n"
    "  --port, -p <port>                    Port number. Default is " + std::to_string(port) + "\n"
    "  --connections, -c <connections>      Maximum number of Websocket clients, 1-64. Default is " + std::to_string(maxConnections) + "\n"
    "  --log, -l                            Logging on\n"
    "  --verbose                            Verbose logging (enables websocketpp messages)\n"
    "  --version, -v                        Version information\n"
    "\n";
