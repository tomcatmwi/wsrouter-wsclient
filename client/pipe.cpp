// pipe.cpp
#include <string>
#include <cstring>
#include <array>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <memory>
#include <fcntl.h>

#include "../core/utils.hpp"

#include "constants.hpp"
#include "asio_ws.hpp"

//	The FIFO pipeline allows other applications to send Websocket messages through this program
//	Anything sent to the FIFO pipeline (ie.: /tmp/wspipe) will be forwarded to the router

//  Pipeline "mailbox" - when changes, the content is sent to Websocket --------------------------------------------------------------------------------------------

std::shared_ptr<std::string> pipe_content = nullptr;

const std::shared_ptr<std::string>& get_pipe_content() {
    return pipe_content;
}

//  Watches the output pipe and sends new values to Websocket -----------------------------------------------------------------------------------------------------

void watch_pipe() {

    char buffer[1024];
    int fd;
    bool pipe_success = false;
    
    //  Pipe (re)opener
    auto open_pipe =[&]() -> bool {
	fd = open(pipe_out.c_str(), O_RDONLY | O_NONBLOCK);
        
        if (fd == -1) {
                log("ERROR", "Error opening FIFO pipeline: " + std::string(strerror(errno)));
                pipe_success = false;
                return false;
        }

        pipe_success = true;        
        return true;
    };
    
    if (!open_pipe())
        return;

    //	Run event loop to allow sending to the FIFO pipe
    log("LOG", "Initializing FIFO pipe watcher loop");

    while (pipe_success) {
    
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
        
        if (bytes_read >= 0) {
            if (bytes_read > 0) {
                std::string incoming = std::string(buffer, bytes_read);
                send(incoming);
            }
        }

        else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }            
            else {
                log("ERROR", "Error reading from FIFO pipeline: " + std::string(strerror(errno)));
                    pipe_success = false;
                close(fd);
            }
        }   
    }

}

//  Writes the incoming messages received from outside, meant for external programs
void write_pipe(const std::string& message) {
			
    int fd = open(pipe_in.c_str(), O_WRONLY | O_NONBLOCK);
    int err = errno;

    if (fd == -1) {

        //  Silently fail if nothing is reading the pipe
        if (err == ENXIO)
            return;

        log("ERROR", "Cannot open input pipe " + pipe_in + " for writing: " + std::string(strerror(err)));
        return;
    }
    
	ssize_t bytes_written = write(fd, (message + "\n").c_str(), message.size() + 1);

    if (bytes_written == -1) {
        log("ERROR", "Failed to write to pipe_in: " + std::string(strerror(err)));
    }

    close(fd);
}

//  ---  Pipeline creator ----------------------------------------------------------------------------------------------

bool create_pipe(std::string pipe_path) {

    struct stat st;

    //  Create pipeline
    if (access(pipe_path.c_str(), F_OK) == 0)
        log("LOG", "Input pipeline " + pipe_path+ " already exists");

    else if (stat(pipe_path.c_str(), &st) != 0) {

        if (mkfifo(pipe_path.c_str(), 0666) != 0) {
            log("ERROR", "Failed to create pipeline " + pipe_path);
            return false;
        } else
            log("LOG", "Pipeline " + pipe_path + " created.");

    } else if (!S_ISFIFO(st.st_mode)) {

	log("ERROR", "Pipeline " + pipe_path + " exists but it's not FIFO.");
        return false;
    }

    return true;
}

//  ---  Initializes FIFO pipelines ----------------------------------------------------------------------------------------------

bool init_pipe() {

    if (!create_pipe(pipe_in) || !create_pipe(pipe_out))
	return false;

    //  Initialize output pipeline watcher on separate thread
    std::thread(watch_pipe).detach();

    return true;
}
