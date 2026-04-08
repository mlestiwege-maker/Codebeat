#pragma once

#include <string>

namespace codebeat::runtime {

class Planner {
public:
    [[nodiscard]] std::string plan(const std::string& user_goal) const {
        return "[plan] break goal into: understand -> propose -> execute (stub). Goal: " + user_goal;
    }
};

} // namespace codebeat::runtime
