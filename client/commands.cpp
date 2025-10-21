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

#if defined(__FreeBSD__)
	#include <sys/reboot.h>
#endif

#include "../core/utils.hpp"

#include "./asio_ws.hpp"
#include "./constants.hpp"
#include "./pipe.hpp"

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

      	//  No shutdown
      	if (geteuid() != 0 || std::strcmp(argv[i], "--disable_shutdown") == 0 || std::strcmp(argv[i], "-ds") == 0)
          	  shutdown_enabled = false;

      	//  Verbose logging on/off
      	if (std::strcmp(argv[i], "--verbose") == 0) {
          	  logging_enabled = true;
          	  logging_verbose = true;
      	}

      	//  FIFO incoming pipe path
      	if (i > 0 && (std::strcmp(argv[i-1], "--pipe_in") == 0 || std::strcmp(argv[i-1], "-pi") == 0) && argv[i] && *argv[i])
              	pipe_in = argv[i];

      	//  FIFO outgoing pipe path
      	if (i > 0 && (std::strcmp(argv[i-1], "--pipe_out") == 0 || std::strcmp(argv[i-1], "-po") == 0) && argv[i] && *argv[i])
              	pipe_out = argv[i];

      	//  Disable forwarding of messages to FIFO pipe
      	if (std::strcmp(argv[i], "--disable_pipe_all") == 0 || std::strcmp(argv[i], "-dp") == 0)
          	  pipe_all = false;

      	//  Websocket client ID
      	if (i > 0 && (std::strcmp(argv[i-1], "--id") == 0 || std::strcmp(argv[i-1], "-i") == 0) && argv[i] && *argv[i])
              	ws_id = argv[i];

      	//  Websocket host address
      	if (i > 0 && (std::strcmp(argv[i-1], "--host") == 0 || std::strcmp(argv[i-1], "-x") == 0) && argv[i] && *argv[i])
              	ws_host = argv[i];

      	//	Websocket port
      	if (i > 0 && (std::strcmp(argv[i-1], "--port") == 0 || std::strcmp(argv[i-1], "-p") == 0) && argv[i] && *argv[i]) { 
      	  auto value = string_to_int(argv[i], 0, 65535);
      	  if (!value) {
      	    std::cout << "Invalid --port value" << std::endl;
      	    return false;
      	  }
      	  
      	  port = argv[i];
      	}
		  
      	//	Reconnection
      	if (i > 0 && (std::strcmp(argv[i-1], "--retries") == 0 || std::strcmp(argv[i-1], "-r") == 0)) {
      	  
      	  auto value = string_to_int(argv[i], 0, 65535);
      	  if (!value) {
      	    std::cout << "Invalid --retries value" << std::endl;
      	    return false;
      	  }
      	  
      	  retries = *value;
      	}
          
      	//	Reconnection interval
      	if (i > 0 && (std::strcmp(argv[i-1], "--retry_interval") == 0 || std::strcmp(argv[i-1], "-ri") == 0)) {
      	  
      	  auto value = string_to_int(argv[i], 100, 60000);
      	  if (!value) {
      	    std::cout << "Invalid --retry_interval value" << std::endl;
      	    return false;
      	  }
      	  
      	  retry_interval = *value;
      	}
          
      	//	Websocket handshake timeout
      	if (i > 0 && (std::strcmp(argv[i-1], "--timeout") == 0 || std::strcmp(argv[i-1], "-t") == 0)) {
      	  
      	  auto value = string_to_int(argv[i], 100, 360000);
      	  if (!value) {
      	    std::cout << "Invalid --timeout value" << std::endl;
      	    return false;
      	  }
      	  
      	  ws_handshake_timeout = *value;
      	}  		        
        }

  return true;
}

//	Commands processor -----------------------------------------------------------------------------------------------------------------
void process_commands (std::string payload) {
	//  Command format:
	//  sender::expects_reply::reply_to::content

	std::vector<std::string> parts = split(payload, "::");

	//  Get message parts
	std::string sender_id = parts[0];

	if (parts.size() < 3) {
		send(sender_id + "::" + ws_id + "::0::ERROR::Message is incomplete");
		return;
	}

	// bool expects_reply = parts[1] == "1";
	bool error = parts[1] == "2";
	std::string reply_to = (parts.size() > 2 && !parts[2].empty()) ? parts[2] : parts[0];
	
	std::string content = join(parts, "::", 3);
	std::string timestamp = get_timestamp();
		
	//  Error
	if (error) {
		if (pipe_all)
			write_pipe(payload);
		log("ERROR", content);
		return;
	}

	std::vector<std::string> content_parts = split(content, "::");
	std::string command = to_upper(content_parts[0]);

	if (logging_enabled)
		log("LOG", content);

	//	Output incoming command unless not
	if (pipe_all && command != "PIPE")
		write_pipe(payload);

	//  ----------------------------------------------------------------------------------------------------------------
	//  PING - Sends back a ping
	//  ----------------------------------------------------------------------------------------------------------------

		if (command == "PING") {
		send(reply_to + "::" + ws_id + "::0::::PONG");
		return;
		}

	//  ----------------------------------------------------------------------------------------------------------------
	//  PIPE - Write incoming content to input FIFO pipeline
	//  Example: PIPE::content
	//  ----------------------------------------------------------------------------------------------------------------

	if (command == "PIPE") {
		//	Remove "pipe::" from the content
		write_pipe(content.substr(6));
		send(reply_to + "::" + ws_id + "::0::::Message sent to " + pipe_in);  
		return;
	}

	//  ----------------------------------------------------------------------------------------------------------------
	//  DATE - Set system date and time (latter is optional)
	//  Example: DATE::year::month::day::hour::minute::second::timezone
	//  ----------------------------------------------------------------------------------------------------------------

    if (command == "DATE" || command == "TIME") {

		if (geteuid() != 0) {
		send(reply_to + "::" + ws_id + "::0::error::1::Failed to set date/time: Client is not running as root");
		log("ERROR", "Date and time cannot be set - root privileges are required!");
		return;
		}			

      auto date_parts = split(content_parts[1], "::");

      try {
	  if (date_parts.size() != 7)
    	  throw 0;
                        
      struct tm timeinfo = {};
      timeinfo.tm_year = std::stoi(date_parts[0]) - 1900;
      timeinfo.tm_mon  = std::stoi(date_parts[1]) - 1;
      timeinfo.tm_mday = std::stoi(date_parts[2]);
      timeinfo.tm_hour = std::stoi(date_parts[3]);
      timeinfo.tm_min  = std::stoi(date_parts[4]);
      timeinfo.tm_sec  = std::stoi(date_parts[5]);

      time_t new_time = mktime(&timeinfo);
      if (new_time == -1)
        throw 0;

      timeval tv;
      tv.tv_sec = new_time;
      tv.tv_usec = 0;

      if (settimeofday(&tv, nullptr) != 0)
        throw 0;

      setenv("TZ", date_parts[6].c_str(), 1);
      tzset();
      log("LOG", "New date/time received from : " + sender_id + ": \"" + content_parts[1] + "\"");
      send(reply_to + "::" + ws_id + "::0::::New date/time set");
      
	} catch (...) {
  	  send(reply_to + "::" + ws_id + "::0::error::1::Incorrect date/time");
	  log("ERROR", "Incorrect date/time received from " + sender_id +  ": \"" + content_parts[1] + "\"");
    }
        
    return;
  }

  //  ----------------------------------------------------------------------------------------------------------------
  //  SHUTDOWN - Shuts down the device
  //  ----------------------------------------------------------------------------------------------------------------
  
  if (command == "SHUTDOWN") {

	close_websocket();

    if (shutdown_enabled) {
		log("LOG", "Shutdown!");
		send(reply_to + "::" + ws_id + "::0::::Shutdown requested");

        #if defined(__FreeBSD__)
        sync();
        reboot(RB_POWEROFF);
        #else
        system("poweroff");
        #endif

	} else {
		send(reply_to + "::" + ws_id + "::0::::Shutdown requested but not possible. Exiting wsclient");
        log("LOG", "Shutdown disabled, exiting client!");
		close_websocket();
		std::raise(SIGTERM);
		std::exit(0);
	}
	
	return;
  }
}
