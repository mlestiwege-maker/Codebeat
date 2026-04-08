#pragma once

#include <deque>
#include <string>

namespace codebeat::runtime {

class ConversationMemory {
public:
    explicit ConversationMemory(std::size_t max_items = 20) : max_items_(max_items) {}

    void push(std::string msg) {
        history_.push_back(std::move(msg));
        while (history_.size() > max_items_) {
            history_.pop_front();
        }
    }

    [[nodiscard]] std::string latest() const {
        return history_.empty() ? std::string{} : history_.back();
    }

private:
    std::size_t max_items_{20};
    std::deque<std::string> history_{};
};

} // namespace codebeat::runtime
