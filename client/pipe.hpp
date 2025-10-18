// pipe.hpp
#pragma once

#include <memory>
#include <string>

const std::shared_ptr<std::string>& get_pipe_content();
bool create_pipe(std::string pipe_path);
void write_pipe(const std::string& message);
void watch_pipe();
bool init_pipe();
