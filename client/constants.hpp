// constants.hpp
#pragma once
#include <string>

//  Version number and build time
extern const std::string version;

//  Websocket server (router) host
extern std::string ws_host;

//  Websocket server port
extern std::string port;

//  Client ID
extern std::string ws_id;

//  Logging flags
extern bool logging_enabled;
extern bool logging_verbose;

//  PID filename
extern std::string pid_file;

//  FIFO pipes
extern std::string pipe_in;		//  Incoming pipe; all incoming messages will go there and the "pipe" command also writes to this
extern std::string pipe_out;	//  Other programs may write this pipe to send out something

//	Websocket connection retry constants
extern int retries;
extern int retry_interval;
extern int ws_handshake_timeout;

//  Shutdown enabled
extern bool shutdown_enabled;

//  Pipe all messages to the FIFO pipe
extern bool pipe_all;

//  Help text
extern std::string help_text;
