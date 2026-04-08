#pragma once

#include <random>

namespace codebeat::engine {

class RNG {
public:
    explicit RNG(unsigned int seed = 42U) : gen_(seed) {}

    float uniform(float lo = 0.0f, float hi = 1.0f) {
        std::uniform_real_distribution<float> dist(lo, hi);
        return dist(gen_);
    }

private:
    std::mt19937 gen_;
};

} // namespace codebeat::engine
