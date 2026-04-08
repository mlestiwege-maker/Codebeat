#pragma once

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
};

} // namespace codebeat::runtime
