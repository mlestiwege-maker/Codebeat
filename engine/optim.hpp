#pragma once

#include "engine/autodiff.hpp"

#include <cmath>
#include <vector>

namespace codebeat::engine {

class SGD {
public:
    explicit SGD(float lr = 0.001f) : learning_rate_(lr) {}

    void step(std::vector<Variable*>& params) const {
        for (auto* p : params) {
            if (!p) {
                continue;
            }
            for (std::size_t i = 0; i < p->value.size() && i < p->grad.size(); ++i) {
                p->value[i] -= learning_rate_ * p->grad[i];
            }
        }
    }

private:
    float learning_rate_{0.001f};
};

class AdamW {
public:
    explicit AdamW(float lr = 0.001f, float beta1 = 0.9f, float beta2 = 0.999f,
                   float eps = 1e-8f, float weight_decay = 0.01f)
        : learning_rate_(lr),
          beta1_(beta1),
          beta2_(beta2),
          eps_(eps),
          weight_decay_(weight_decay),
          t_(0) {}

    void step(std::vector<Variable*>& params) {
        ++t_;
        for (auto* p : params) {
            if (!p) {
                continue;
            }

            // Initialize momentum buffers if needed
            if (m_.size() < p->value.size()) {
                m_.resize(p->value.size(), 0.0f);
                v_.resize(p->value.size(), 0.0f);
            }

            for (std::size_t i = 0; i < p->value.size() && i < p->grad.size(); ++i) {
                const float g = p->grad[i];
                m_[i] = beta1_ * m_[i] + (1.0f - beta1_) * g;
                v_[i] = beta2_ * v_[i] + (1.0f - beta2_) * g * g;

                const float m_hat = m_[i] / (1.0f - std::pow(beta1_, static_cast<float>(t_)));
                const float v_hat = v_[i] / (1.0f - std::pow(beta2_, static_cast<float>(t_)));

                const float step = learning_rate_ * m_hat / (std::sqrt(v_hat) + eps_);
                p->value[i] -= step + weight_decay_ * learning_rate_ * p->value[i];
            }
        }
    }

    void clear_state() {
        m_.clear();
        v_.clear();
        t_ = 0;
    }

private:
    float learning_rate_;
    float beta1_;
    float beta2_;
    float eps_;
    float weight_decay_;
    std::size_t t_;
    std::vector<float> m_;
    std::vector<float> v_;
};

} // namespace codebeat::engine
