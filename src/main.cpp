#include <iostream>
#include <string>
#include "voice.h"
#include "brain.h"
#include "response.h"

int main() {
    std::cout << "🎵 Welcome to Codebeat!\n";
    std::cout << "Type 'exit' to quit.\n\n";

    Voice voice;
    Brain brain;
    Response response;

    while (true) {
        // Prompt user
        std::cout << "You: ";

        // Get user input
        std::string user_input = voice.listen();

        // Process input
        std::string reply = brain.process(user_input);

        // Check for exit
        if (reply == "exit") {
            std::cout << "\n👋 Goodbye!\n";
            break;
        }

        // Output response
        response.speak(reply);
    }

    return 0;
}
