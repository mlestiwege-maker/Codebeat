#pragma once

#include <string>

namespace codebeat::models {

class CtcAsrModel {
public:
    [[nodiscard]] std::string status() const {
        return "ASR module stub: implement acoustic encoder + CTC loss in later milestone.";
    }
};

} // namespace codebeat::models
