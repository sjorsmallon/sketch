#pragma once 

#include <atomic>
#include <queue>
#include <iostream>
#include <fmt/core.h>
#include <fmt/color.h>
#include "concurrent_queue.hpp"



static void logger_thread(moodycamel::ConcurrentQueue<std::string>& log_queue) {
    std::string message;
    while (true) {
    if (log_queue.try_dequeue(message)) {
        fmt::print("{}\n", message); // Logging using fmt (you can replace it with your logging function)
    }
    // Add any additional logging functionality here
    }
}

void vlog(moodycamel::ConcurrentQueue<std::string>& log_queue, const char* format, fmt::format_args args);

template<typename... Args>
void log(moodycamel::ConcurrentQueue<std::string>& log_queue, const char* format, const Args& ...args)
{
    vlog(log_queue, format, fmt::make_format_args(args...));
}


inline void vlog(moodycamel::ConcurrentQueue<std::string>& log_queue, const char* format, fmt::format_args args)
{
    std::string prelude = fmt::format(fg(fmt::color::white), "[INFO]: ");
    std::string actual = fmt::vformat(format, args);      
    std::string full = prelude + actual + '\n';


    log_queue.enqueue(prelude + actual + "\n");
}


// void log_message(const std::string& message) {
//     log_queue.push(message);
// }

// int main() {
//     std::thread logger(logger_thread);

//     // Example: Logging messages from main thread
//     log_message("This is a log message.");
//     // Add more log messages as needed

//     logger.join();
//     return 0;
// }
