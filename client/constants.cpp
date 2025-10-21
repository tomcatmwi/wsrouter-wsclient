#include "constants.hpp"
#include <string>

//  Version number and build time
const std::string version = std::string("1.5.1 ") + __DATE__ + " " + __TIME__;

//  Server host
std::string ws_host = "192.168.8.1";

//  Port
std::string port = "8080";

//  Websocket client ID
std::string ws_id = "noname";

//  Logging flags
bool logging_enabled = true;
bool logging_verbose = false;

//  FIFO pipes
std::string pipe_in = "/tmp/ws_in";
std::string pipe_out = "/tmp/ws_out";

//  PID filename
std::string pid_file = "/tmp/wsclient.pid";

//  Websocket connection retries - 0 means infinite
int retries = 10;

//  Connection retry interval, milliseconds
int retry_interval = 1000;

//  Timeout for a Websocket connection attempt, milliseconds
int ws_handshake_timeout = 2000;

//  Shutdown enabled
bool shutdown_enabled = true;

//  Output all incoming messages to the output FIFO pipe
bool pipe_all = true;

//  Help text
std::string help_text =
    "wsclient - The Ultralight IoT Websocket Client\n"
    "Version and build date: " + version + "\n\n"
    "Command line arguments:\n"

    "\nConnection settings:\n\n"
    "  --host, -x <host>                    Websocket remote host (server). Default: " + ws_host + "\n"
    "  --id, -i <id>                        Websocket client ID. The router will know this client by this name. Default: " + ws_id + "\n"
    "  --port, -p <port>                    Websocket remote port. Default: " + port + "\n"
    "  --retries, -r <retries>              Attempts to reconnect if Websocket connection is lost. 0 means infinite. Default: " + std::to_string(retries) + ".\n"
    "  --retry_interval, -ri <interval>     Milliseconds to wait between reconnection attempts. Default: " + std::to_string(retry_interval) + "\n"
    "  --timeout, -t <timeout>              Timeout in milliseconds for reconnection attempts. Default: " + std::to_string(ws_handshake_timeout) + "\n"

    "\nPipeline configuration:\n\n"
    "  --disable_pipe_all, -dp              Disable forwarding every incoming message to the output FIFO pipe\n"
    "  --pipe_in, -pi <pipe>                Input FIFO pipe path. Messages received with PIPE command will be written to this pipe, and other programs can read it. Default: " + pipe_in + "\n"
    "  --pipe_out, -po <pipe>               Output FIFO pipe path. Anything sent to this pipe will be sent to the WS server. Default: " + pipe_out + "\n"

    "\nOthers:\n\n"
    "  --disable_shutdown, -ds              Disable remote shutdown. The client will still disconnect upon receiving the command." + "\n"
    "  --help, -h                           This text\n"
    "  --log, -l                            Enable logging. Default: " + (logging_enabled ? "on" : "off") + "\n"
    "  --pid, -p <path>                     File to store process ID (prevents running multiple instances). Default: " + pid_file + "\n"
    "  --version, -v                        Version information\n"
    "\n";
