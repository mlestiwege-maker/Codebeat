#pragma once

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

namespace codebeat::engine {

class Tensor {
public:
    Tensor() = default;
    Tensor(std::vector<std::size_t> shape, float value = 0.0f)
        : shape_(std::move(shape)), data_(numel(shape_), value) {}

    [[nodiscard]] const std::vector<std::size_t>& shape() const noexcept { return shape_; }
    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }
    [[nodiscard]] std::size_t rank() const noexcept { return shape_.size(); }

    [[nodiscard]] bool same_shape_as(const Tensor& other) const noexcept {
        return shape_ == other.shape_;
    }

    [[nodiscard]] std::size_t rows() const {
        ensure_rank_2("rows");
        return shape_[0];
    }

    [[nodiscard]] std::size_t cols() const {
        ensure_rank_2("cols");
        return shape_[1];
    }

    float& operator[](std::size_t i) { return data_.at(i); }
    const float& operator[](std::size_t i) const { return data_.at(i); }

    float& at(std::size_t r, std::size_t c) {
        ensure_rank_2("at");
        return data_.at(r * cols() + c);
    }

    const float& at(std::size_t r, std::size_t c) const {
        ensure_rank_2("at");
        return data_.at(r * cols() + c);
    }

    void fill(float value) {
        std::fill(data_.begin(), data_.end(), value);
    }

    [[nodiscard]] std::vector<float>& data() noexcept { return data_; }
    [[nodiscard]] const std::vector<float>& data() const noexcept { return data_; }

    static std::size_t numel(const std::vector<std::size_t>& s) {
        if (s.empty()) {
            return 0;
        }
        return std::accumulate(s.begin(), s.end(), static_cast<std::size_t>(1),
                               [](std::size_t acc, std::size_t d) { return acc * d; });
    }

private:
    void ensure_rank_2(const char* op) const {
        if (shape_.size() != 2) {
            throw std::logic_error(std::string(op) + ": expected rank-2 tensor");
        }
    }

    std::vector<std::size_t> shape_{};
    std::vector<float> data_{};
};

} // namespace codebeat::engine
