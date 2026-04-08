#pragma once

#include "engine/tensor.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace codebeat::engine {

struct Variable {
    Tensor value{};
    Tensor grad{};
    std::string name{};
};

class ComputeNode {
public:
    virtual ~ComputeNode() = default;
    virtual void backward(Tensor grad) = 0;
    [[nodiscard]] virtual std::string op_name() const = 0;
};

class AutodiffTape {
public:
    using NodePtr = std::shared_ptr<ComputeNode>;

    explicit AutodiffTape(bool enabled = true) : enabled_(enabled) {}

    void record(NodePtr node) {
        if (enabled_) {
            nodes_.push_back(node);
        }
    }

    void clear() {
        nodes_.clear();
    }

    void backward(Variable& root_var, float scale = 1.0f) {
        if (!enabled_ || nodes_.empty()) {
            return;
        }

        root_var.grad = Tensor(root_var.value.shape(), scale);

        for (auto it = nodes_.rbegin(); it != nodes_.rend(); ++it) {
            (*it)->backward(root_var.grad);
        }
    }

    [[nodiscard]] bool is_enabled() const noexcept { return enabled_; }

private:
    bool enabled_{true};
    std::vector<NodePtr> nodes_{};
};

} // namespace codebeat::engine
