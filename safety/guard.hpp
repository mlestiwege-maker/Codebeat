#pragma once

#include <string>
#include <unordered_set>

namespace codebeat::safety {

class Guard {
public:
    Guard() : allowed_tools_{"echo", "read_note", "write_note"} {}

    [[nodiscard]] bool allow_tool(const std::string& name) const {
        return allowed_tools_.count(name) > 0;
    }

private:
    std::unordered_set<std::string> allowed_tools_;
};

} // namespace codebeat::safety
