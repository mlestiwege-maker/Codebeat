#pragma once

#include <chrono>
#include <ctime>
#include <string>

namespace codebeat::runtime {

struct ToolResult {
    bool ok{false};
    std::string output{};
};

class Tools {
public:
    [[nodiscard]] ToolResult echo(const std::string& text) const {
        return ToolResult{true, "echo: " + text};
    }

    [[nodiscard]] ToolResult now_time() const {
        const auto now = std::chrono::system_clock::now();
        const std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::string ts = std::ctime(&t);
        if (!ts.empty() && ts.back() == '\n') {
            ts.pop_back();
        }
        return ToolResult{true, "local time: " + ts};
    }

    [[nodiscard]] ToolResult capabilities() const {
        return ToolResult{true,
                          "tools: echo, time, summarize-memory, lock, quick-actions"};
    }
};

} // namespace codebeat::runtime
