#include <array>
#include <chrono>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <sys/time.h>
#include <unordered_map>
#include <filesystem>
#include <unistd.h>
#include <optional>

#ifdef ROUTER
  #include "../router/constants.hpp"
#elif defined(CLIENT)
  #include "../client/constants.hpp"
#else
  #error "No build target defined ('ROUTER' or 'CLIENT')"
#endif

#include "utils.hpp"

//  Reads the contents of a directory -----------------------------------------------------------------------------------------------------------------------------
namespace fs = std::filesystem;

std::vector<fs::directory_entry> list_files(const std::string& path) {

    if (!fs::exists(path) || !fs::is_directory(path)) {
	std::cerr << "Invalid path: " << path << std::endl;
	return {};
    }

    std::vector<fs::directory_entry> files;
    for (const auto& entry : fs::directory_iterator(path)) {
        if (fs::is_regular_file(entry.status())) {
            files.push_back(entry);
        }
    }
    return files;
}

//  Returns a formatted timestamp ----------------------------------------------------------------------------------------------------------------------------------
std::string get_timestamp(const bool withTime) {
        const char* frm = withTime ? "%Y-%m-%d %H:%M:%S" : "%Y-%m-%d";
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        char buf[20];
        std::strftime(buf, sizeof(buf), frm, std::localtime(&tt));
        return std::string(buf);
}

//  String splitter -----------------------------------------------------------------------------------------------------------------------------------------------
std::vector<std::string> split(const std::string& s, const std::string& delim) {
        std::vector<std::string> parts;
        size_t pos = 0, prev = 0;
        while ((pos = s.find(delim, prev)) != std::string::npos) {
            parts.push_back(s.substr(prev, pos - prev));
            prev = pos + delim.length();
        }
        parts.push_back(s.substr(prev));
        return parts;
}

//  String joiner -------------------------------------------------------------------------------------------------------------------------------------------------
std::string join(const std::vector<std::string>& parts, const std::string& delim, size_t start) {
        std::string result;
        for (size_t i = start; i < parts.size(); ++i) {
            if (i > start) result += delim;
            result += parts[i];
        }
        return result;
} 

//  Logger -------------------------------------------------------------------------------------------------------------------------------------------------------
//  Prints a log line (if logging is enabled)
void log(const std::string& type, const std::string& msg) {
    if (logging_enabled) {
        std::cout << get_timestamp(true) << " [" << type << "] " << msg << std::endl;
    }
}

//  Sets the system date and time --------------------------------------------------------------------------------------------------------------------------------
bool set_datetime(std::vector<std::string> date_parts) {

    try {
            if (date_parts.size() < 6)
                    throw 0;
            
            struct tm timeinfo = {};
            timeinfo.tm_year = std::stoi(date_parts[0]) - 1900; // years since 1900
            timeinfo.tm_mon  = std::stoi(date_parts[1]) - 1;   // 0-11
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

            if (logging_enabled)
                log("RECV", "Date changed to " + get_timestamp(true));

            //  Set time zone
            if (date_parts.size() > 6) {

                std::string cmd =
                    "uci set system.@system[0].timezone='" + date_parts[6] + "' && "
                    "uci commit system && "
                    "/etc/init.d/system reload";

                system(cmd.c_str());

                if (logging_enabled)
                    log("RECV", "Time zone changed to " + date_parts[6]);
            }

            return true;

    } catch(int err) {
        if (logging_enabled)
            log("ERROR", "Incorrect date/time received");
        return false;
    }
}

//  General shutdown handler -------------------------------------------------------------------------------------------------------------------------------------
void shutdown_handler(int signum) {

    std::vector<std::string> signals = {
        "HUP", "INT", "QUIT", "ILL", "TRAP", "ABRT/SIGIOT", "BUS", "FPE", "KILL", "USR1", "SEGV", "USR2", "PIPE",
        "ALRM", "TERM", "STKFLT", "CHLD", "CONT", "STOP", "TSTP", "TTIN", "TTOU", "URG", "XCPU", "XFSZ", "VTALRM",
        "PROF", "WINCH", "IO/SIGPOLL", "PWR/SIGLOST", "UNUSED/SIGSYS"
    };

    if (signum >= 1 && signum <= static_cast<int>(signals.size())) {
	std::cout << std::endl;
        log("SHUTDOWN", "Signal caught: SIG" + signals[signum - 1]);
    }
    else
        log("SHUTDOWN", "Signal caught: " + std::to_string(signum) + " (unknown)");
}

//  Checks if the program is already running (by PID) -----------------------------------------------------------------------------------------------------------
bool single_instance() {

    std::ifstream in(pid_file);
    pid_t old_pid = 0;

    if (in) {
        in >> old_pid;
        in.close();

        if (old_pid > 0 && (kill(old_pid, 0) == 0 || errno == EPERM)) {
            log("ERROR", "Program is already running. PID: " + std::to_string(old_pid));
            return false;
        }
    }

    std::ofstream out(pid_file);
    if (!out) {
        try {
            std::remove(pid_file.c_str());
        } catch(const std::exception& e) {
            log("ERROR", "PID file already exists and cannot be deleted: " + std::string(e.what()));
            return false;
        }
    }

    out << getpid() << std::endl;
    return true;
}

//  String conversion to uppercase  ----------------------------------------------------------------------------------------------------------
std::string to_upper(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return std::toupper(c); });
    return out;
}

//	String conversion to integer ----------------------------------------------------------------------------------------------------------------
std::optional<int> string_to_int(
    const std::string& s,
    std::optional<int> min = std::nullopt,
    std::optional<int> max = std::nullopt
) {
    try {
        size_t idx = 0;
        int val = std::stoi(s, &idx);

        // Check if it's a number
        if (idx != s.size()) return std::nullopt;

        // Check minimum
        if (min && val < *min) return std::nullopt;

        // Check maximum
        if (max && val > *max) return std::nullopt;

        return val;
        
    } catch(const std::exception& e) {
        return std::nullopt;
    }
}

//  Validates a client ID -----------------------------------------------------------------------------------------------------------------------------
bool is_valid_id(const std::string& id) {
    return !id.empty() && std::regex_match(id, std::regex("^[a-zA-Z0-9]+$"));
}
