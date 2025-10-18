// utils.hpp
#ifndef UTILS_HPP
#define UTILS_HPP

#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include <optional>

namespace fs = std::filesystem;

std::string get_timestamp(const bool withTime = false);
std::string join(const std::vector<std::string>& parts, const std::string& delim, size_t start);
std::vector<fs::directory_entry> list_files(const std::string& path);
void log(const std::string& type, const std::string& msg);
bool set_datetime(std::vector<std::string> date_parts);
bool single_instance();
void shutdown_handler(int signum);
std::vector<std::string> split(const std::string& s, const std::string& delim);
std::string to_upper(const std::string& s);
std::optional<int> string_to_int(const std::string& s, std::optional<int> min, std::optional<int> max);
bool is_valid_id(const std::string& id);

#endif
