#include "voice.h"
#include <iostream>
#include <algorithm>
#include <cctype>

// Helper function to trim whitespace from both ends
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

std::string Voice::listen() {
    std::string input;

    while (true) {
        std::cout << "You: ";
        std::getline(std::cin, input);

        input = trim(input);
        if (!input.empty()) break;  // Ignore empty input
    }

    return input;
}
