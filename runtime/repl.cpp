#include "runtime/memory.hpp"
#include "runtime/planner.hpp"
#include "runtime/tools.hpp"

#include <iostream>
#include <string>

int main() {
    using namespace codebeat::runtime;

    ConversationMemory mem;
    Planner planner;
    Tools tools;

    std::cout << "Codebeat REPL (type 'exit' to quit)\n";
    for (std::string line; std::cout << "> " && std::getline(std::cin, line);) {
        if (line == "exit") {
            break;
        }

        mem.push("user: " + line);
        const auto p = planner.plan(line);
        const auto r = tools.echo(line);

        std::cout << p << "\n" << r.output << "\n";
    }

    return 0;
}
