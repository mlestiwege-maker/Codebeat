#pragma once

#include "engine/autodiff.hpp"
#include "engine/ops.hpp"
#include "engine/rng.hpp"
#include "engine/tensor.hpp"

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace codebeat::models {

struct TransformerConfig {
    std::size_t vocab_size{256};
    std::size_t d_model{128};
    std::size_t n_heads{4};
    std::size_t n_layers{2};
    std::size_t max_seq_len{256};
    float dropout_p{0.1f};
};

class EmbeddingLayer {
public:
    explicit EmbeddingLayer(std::size_t vocab_size, std::size_t d_model)
        : vocab_size_(vocab_size), d_model_(d_model) {
        // Token embedding: [vocab_size, d_model]
        token_embed_ = engine::Tensor({vocab_size, d_model}, 0.0f);
        engine::RNG rng(42);
        const float scale = 1.0f / std::sqrt(static_cast<float>(d_model));
        for (std::size_t i = 0; i < token_embed_.size(); ++i) {
            token_embed_[i] = (rng.uniform() - 0.5f) * 2.0f * scale;
        }

        // Positional embedding: [max_seq_len, d_model]
        pos_embed_ = engine::Tensor({256, d_model}, 0.0f);
        for (std::size_t pos = 0; pos < 256; ++pos) {
            for (std::size_t i = 0; i < d_model; ++i) {
                const float dim = static_cast<float>(i);
                const float p = static_cast<float>(pos);
                const float freq = 1.0f / std::pow(10000.0f, 2.0f * dim / static_cast<float>(d_model));
                if (i % 2 == 0) {
                    pos_embed_.at(pos, i) = std::sin(p * freq);
                } else {
                    pos_embed_.at(pos, i) = std::cos(p * freq);
                }
            }
        }
    }

    [[nodiscard]] engine::Tensor lookup(std::size_t token_id) const {
        engine::Tensor out({1, d_model_}, 0.0f);
        if (token_id < vocab_size_) {
            for (std::size_t i = 0; i < d_model_; ++i) {
                out[i] = token_embed_.at(token_id, i);
            }
        }
        return out;
    }

    [[nodiscard]] engine::Tensor add_positional_encoding(const engine::Tensor& x, std::size_t seq_pos) const {
        engine::Tensor out(x.shape(), 0.0f);
        for (std::size_t i = 0; i < x.size(); ++i) {
            out[i] = x[i];
        }
        if (seq_pos < 256) {
            for (std::size_t i = 0; i < d_model_ && i < x.size(); ++i) {
                out[i] += pos_embed_.at(seq_pos, i);
            }
        }
        return out;
    }

    [[nodiscard]] engine::Tensor& token_embedding() { return token_embed_; }
    [[nodiscard]] const engine::Tensor& token_embedding() const { return token_embed_; }

private:
    std::size_t vocab_size_;
    std::size_t d_model_;
    engine::Tensor token_embed_;
    engine::Tensor pos_embed_;
};

class MultiHeadAttention {
public:
    explicit MultiHeadAttention(std::size_t d_model, std::size_t n_heads)
        : d_model_(d_model), n_heads_(n_heads), head_dim_(d_model / n_heads) {
        if (d_model % n_heads != 0) {
            throw std::invalid_argument("d_model must be divisible by n_heads");
        }

        engine::RNG rng(42);
        const float scale = 1.0f / std::sqrt(static_cast<float>(d_model));

        // Query, Key, Value projection weights: [d_model, d_model]
        W_q_ = engine::Tensor({d_model, d_model}, 0.0f);
        W_k_ = engine::Tensor({d_model, d_model}, 0.0f);
        W_v_ = engine::Tensor({d_model, d_model}, 0.0f);

        for (std::size_t i = 0; i < d_model * d_model; ++i) {
            W_q_[i] = (rng.uniform() - 0.5f) * 2.0f * scale;
            W_k_[i] = (rng.uniform() - 0.5f) * 2.0f * scale;
            W_v_[i] = (rng.uniform() - 0.5f) * 2.0f * scale;
        }

        // Output projection: [d_model, d_model]
        W_o_ = engine::Tensor({d_model, d_model}, 0.0f);
        for (std::size_t i = 0; i < d_model * d_model; ++i) {
            W_o_[i] = (rng.uniform() - 0.5f) * 2.0f * scale;
        }
    }

    [[nodiscard]] std::vector<engine::Variable*> parameters() {
        return {
            // Return pointers for optimizer (would need to wrap tensors in Variables in real usage)
        };
    }

private:
    std::size_t d_model_;
    std::size_t n_heads_;
    std::size_t head_dim_;
    engine::Tensor W_q_;
    engine::Tensor W_k_;
    engine::Tensor W_v_;
    engine::Tensor W_o_;
};

class FeedForward {
public:
    explicit FeedForward(std::size_t d_model, std::size_t d_ff = 0)
        : d_model_(d_model), d_ff_(d_ff > 0 ? d_ff : 4 * d_model) {
        engine::RNG rng(42);
        const float scale = 1.0f / std::sqrt(static_cast<float>(d_model));

        // First linear: [d_model, d_ff]
        W1_ = engine::Tensor({d_model_, d_ff_}, 0.0f);
        for (std::size_t i = 0; i < W1_.size(); ++i) {
            W1_[i] = (rng.uniform() - 0.5f) * 2.0f * scale;
        }

        // Second linear: [d_ff, d_model]
        W2_ = engine::Tensor({d_ff_, d_model_}, 0.0f);
        for (std::size_t i = 0; i < W2_.size(); ++i) {
            W2_[i] = (rng.uniform() - 0.5f) * 2.0f * scale;
        }
    }

private:
    std::size_t d_model_;
    std::size_t d_ff_;
    engine::Tensor W1_;
    engine::Tensor W2_;
};

class TransformerLayer {
public:
    explicit TransformerLayer(std::size_t d_model, std::size_t n_heads)
        : attention_(d_model, n_heads), ff_(d_model) {}

private:
    MultiHeadAttention attention_;
    FeedForward ff_;
};

class TinyDecoderTransformer {
public:
    explicit TinyDecoderTransformer(TransformerConfig cfg = TransformerConfig{})
        : cfg_(cfg), embed_(cfg.vocab_size, cfg.d_model) {
        // Initialize transformer layers
        for (std::size_t i = 0; i < cfg_.n_layers; ++i) {
            layers_.emplace_back(cfg_.d_model, cfg_.n_heads);
        }

        // Output projection: [d_model, vocab_size]
        engine::RNG rng(42);
        const float scale = 1.0f / std::sqrt(static_cast<float>(cfg_.d_model));
        output_proj_ = engine::Tensor({cfg_.d_model, cfg_.vocab_size}, 0.0f);
        for (std::size_t i = 0; i < output_proj_.size(); ++i) {
            output_proj_[i] = (rng.uniform() - 0.5f) * 2.0f * scale;
        }
    }

    [[nodiscard]] const TransformerConfig& config() const noexcept { return cfg_; }

    [[nodiscard]] std::string info() const {
        return "TinyDecoderTransformer(vocab=" + std::to_string(cfg_.vocab_size) +
               ", d_model=" + std::to_string(cfg_.d_model) +
               ", n_heads=" + std::to_string(cfg_.n_heads) +
               ", n_layers=" + std::to_string(cfg_.n_layers) + ")";
    }

    [[nodiscard]] std::vector<engine::Variable*> parameters() {
        std::vector<engine::Variable*> params;
        // Collect all parameters from embeddings, layers, and output projection
        // In real code, would wrap tensors in Variables and return pointers
        return params;
    }

    [[nodiscard]] engine::Tensor& get_output_projection() { return output_proj_; }
    [[nodiscard]] EmbeddingLayer& get_embedding() { return embed_; }

private:
    TransformerConfig cfg_;
    EmbeddingLayer embed_;
    std::vector<TransformerLayer> layers_;
    engine::Tensor output_proj_;
};

} // namespace codebeat::models
