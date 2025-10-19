#include <csignal>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <sys/time.h>
#include <unistd.h>

//  General utility functions
#include "./core/utils.hpp"

//  Program-specific
#include "./client/constants.hpp"
#include "./client/commands.hpp"
#include "./client/asio_ws.hpp"
#include "./client/pipe.hpp"

//  Shutdown handlers ---------------------------------------------------------------------------------------------------------------------------------------------

void shutdown(int signum) {
    shutdown_handler(signum);
    close_websocket();
    log("SHUTDOWN", "Bye!");
}

//  ================================================================================================================================================================

int main(int argc, char* argv[]) {
        
    //  Start!
    std::signal(SIGINT, shutdown);
    std::signal(SIGTERM, shutdown);
        
    //  Check if program is already running, then process arguments
    if (!single_instance() || !process_args(argc, argv))
        return 1;

    //  Initialize FIFO pipe watcher
    else if (!init_pipe())
        return 1;
    
    //	Are we root?
    if (geteuid() != 0) {
      log("WARNING", "This program should be run as root! Some features will not work without root privileges.");
      shutdown_enabled = false;
    }

    //  Initialize Websocket service
    if (!init_websocket(process_commands))
      return 1;

    return 0;
}
