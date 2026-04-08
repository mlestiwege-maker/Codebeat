#pragma once

#include "engine/tensor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace codebeat::engine::ops {

inline Tensor add(const Tensor& a, const Tensor& b) {
    if (!a.same_shape_as(b)) {
        throw std::invalid_argument("add: tensor shape mismatch");
    }

    Tensor out(a.shape());
    for (std::size_t i = 0; i < a.size(); ++i) {
        out[i] = a[i] + b[i];
    }
    return out;
}

inline Tensor scale(const Tensor& a, float s) {
    Tensor out(a.shape());
    for (std::size_t i = 0; i < a.size(); ++i) {
        out[i] = a[i] * s;
    }
    return out;
}

inline Tensor transpose_2d(const Tensor& x) {
    Tensor out({x.cols(), x.rows()}, 0.0f);
    for (std::size_t r = 0; r < x.rows(); ++r) {
        for (std::size_t c = 0; c < x.cols(); ++c) {
            out.at(c, r) = x.at(r, c);
        }
    }
    return out;
}

inline Tensor matmul_2d(const Tensor& a, const Tensor& b) {
    if (a.cols() != b.rows()) {
        throw std::invalid_argument("matmul_2d: incompatible shapes");
    }

    Tensor out({a.rows(), b.cols()}, 0.0f);
    for (std::size_t i = 0; i < a.rows(); ++i) {
        for (std::size_t k = 0; k < a.cols(); ++k) {
            const float av = a.at(i, k);
            for (std::size_t j = 0; j < b.cols(); ++j) {
                out.at(i, j) += av * b.at(k, j);
            }
        }
    }
    return out;
}

inline Tensor rowwise_softmax_2d(const Tensor& logits) {
    Tensor out(logits.shape(), 0.0f);

    for (std::size_t r = 0; r < logits.rows(); ++r) {
        float max_v = -std::numeric_limits<float>::infinity();
        for (std::size_t c = 0; c < logits.cols(); ++c) {
            max_v = std::max(max_v, logits.at(r, c));
        }

        float sum = 0.0f;
        for (std::size_t c = 0; c < logits.cols(); ++c) {
            const float ex = std::exp(logits.at(r, c) - max_v);
            out.at(r, c) = ex;
            sum += ex;
        }

        if (sum <= 0.0f) {
            throw std::runtime_error("rowwise_softmax_2d: invalid row sum");
        }

        for (std::size_t c = 0; c < logits.cols(); ++c) {
            out.at(r, c) /= sum;
        }
    }

    return out;
}

inline Tensor layer_norm_last_dim_2d(const Tensor& x,
                                     const Tensor* gamma = nullptr,
                                     const Tensor* beta = nullptr,
                                     float eps = 1e-5f) {
    if (gamma && (gamma->rank() != 1 || gamma->size() != x.cols())) {
        throw std::invalid_argument("layer_norm_last_dim_2d: gamma shape mismatch");
    }
    if (beta && (beta->rank() != 1 || beta->size() != x.cols())) {
        throw std::invalid_argument("layer_norm_last_dim_2d: beta shape mismatch");
    }

    Tensor out(x.shape(), 0.0f);
    for (std::size_t r = 0; r < x.rows(); ++r) {
        float mean = 0.0f;
        for (std::size_t c = 0; c < x.cols(); ++c) {
            mean += x.at(r, c);
        }
        mean /= static_cast<float>(x.cols());

        float var = 0.0f;
        for (std::size_t c = 0; c < x.cols(); ++c) {
            const float d = x.at(r, c) - mean;
            var += d * d;
        }
        var /= static_cast<float>(x.cols());

        const float inv_std = 1.0f / std::sqrt(var + eps);
        for (std::size_t c = 0; c < x.cols(); ++c) {
            float v = (x.at(r, c) - mean) * inv_std;
            if (gamma) {
                v *= (*gamma)[c];
            }
            if (beta) {
                v += (*beta)[c];
            }
            out.at(r, c) = v;
        }
    }
    return out;
}

inline float cross_entropy_from_logits_2d(const Tensor& logits,
                                          const std::vector<std::size_t>& target_class_idx) {
    if (target_class_idx.size() != logits.rows()) {
        throw std::invalid_argument("cross_entropy_from_logits_2d: target row count mismatch");
    }

    float total = 0.0f;
    for (std::size_t r = 0; r < logits.rows(); ++r) {
        const auto target = target_class_idx[r];
        if (target >= logits.cols()) {
            throw std::invalid_argument("cross_entropy_from_logits_2d: target index out of range");
        }

        float max_v = -std::numeric_limits<float>::infinity();
        for (std::size_t c = 0; c < logits.cols(); ++c) {
            max_v = std::max(max_v, logits.at(r, c));
        }

        float sum_exp = 0.0f;
        for (std::size_t c = 0; c < logits.cols(); ++c) {
            sum_exp += std::exp(logits.at(r, c) - max_v);
        }
        const float log_sum_exp = std::log(sum_exp) + max_v;
        const float nll = log_sum_exp - logits.at(r, target);
        total += nll;
    }

    return total / static_cast<float>(logits.rows());
}

} // namespace codebeat::engine::ops
