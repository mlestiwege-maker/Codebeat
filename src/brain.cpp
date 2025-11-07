#include "brain.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
#include <map>

// Helper: trim leading/trailing whitespace
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

// Helper: convert string to lowercase
static std::string toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return result;
}

// Helper: check if a word exists in the input
static bool containsWord(const std::string& input, const std::string& word) {
    return input.find(word) != std::string::npos;
}

std::string Brain::process(const std::string& input) {
    std::string cleaned = toLower(trim(input));

    if (cleaned.empty()) return "Please say something.";

    // Special exit command
    if (cleaned == "exit") return "exit";

    // Keyword-based responses
    if (containsWord(cleaned, "hello") || containsWord(cleaned, "hi")) 
        return "Hello! I am Codebeat, your assistant.";

    if (containsWord(cleaned, "who are you") || containsWord(cleaned, "your name")) 
        return "I am Codebeat, designed to assist you.";

    if (containsWord(cleaned, "how are you")) 
        return "I am doing great! How about you?";

    if (containsWord(cleaned, "time")) 
        return "I cannot tell the time yet, but I am learning!";

    if (containsWord(cleaned, "help") || containsWord(cleaned, "commands")) 
        return "You can say 'hello', 'who are you', 'exit', or ask me simple questions.";

    // Default response
    return "I did not understand that. Can you rephrase?";
}
