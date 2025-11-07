#include "response.h"
#include <iostream>
#include <chrono>
#include <thread>

void Response::speak(const std::string& output) {
    // Optional: simulate typing effect
    for (char c : output) {
        std::cout << c << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::cout << std::endl;
}
