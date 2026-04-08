#pragma once

#include <algorithm>
#include <deque>
#include <string>
#include <vector>

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

    [[nodiscard]] std::vector<std::string> recent(std::size_t max_items = 5) const {
        std::vector<std::string> out;
        if (history_.empty()) {
            return out;
        }

        const std::size_t count = std::min(max_items, history_.size());
        out.reserve(count);
        auto it = history_.end();
        for (std::size_t i = 0; i < count; ++i) {
            --it;
            out.push_back(*it);
        }
        return out;
    }

private:
    std::size_t max_items_{20};
    std::deque<std::string> history_{};
};

} // namespace codebeat::runtime
