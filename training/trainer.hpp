#pragma once

#include "models/transformer.hpp"
#include "models/tokenizer.hpp"
#include "training/dataset.hpp"
#include "engine/autodiff.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

namespace codebeat::training {

class LMTrainer {
public:
    LMTrainer(models::TinyDecoderTransformer& model, LineDataset& ds)
        : model_(model), ds_(ds) {}

    class NextTokenUpdateNode final : public engine::ComputeNode {
    public:
        NextTokenUpdateNode(engine::Tensor& emb,
                            engine::Tensor& out,
                            std::size_t input_id,
                            std::vector<float> hidden,
                            std::vector<float> dlogits,
                            float learning_rate)
            : emb_(emb),
              out_(out),
              input_id_(input_id),
              hidden_(std::move(hidden)),
              dlogits_(std::move(dlogits)),
              learning_rate_(learning_rate) {}

        void backward(engine::Tensor /*grad*/) override {
            const std::size_t d_model = hidden_.size();
            const std::size_t vocab = dlogits_.size();

            std::vector<float> emb_grad(d_model, 0.0f);
            for (std::size_t j = 0; j < d_model; ++j) {
                float g = 0.0f;
                for (std::size_t c = 0; c < vocab; ++c) {
                    g += out_.at(j, c) * dlogits_[c];
                }
                emb_grad[j] = g;
            }

            // Update output projection.
            for (std::size_t j = 0; j < d_model; ++j) {
                const float hj = hidden_[j];
                for (std::size_t c = 0; c < vocab; ++c) {
                    out_.at(j, c) -= learning_rate_ * hj * dlogits_[c];
                }
            }

            // Update token embedding row.
            for (std::size_t j = 0; j < d_model; ++j) {
                emb_.at(input_id_, j) -= learning_rate_ * emb_grad[j];
            }
        }

        [[nodiscard]] std::string op_name() const override {
            return "next_token_linear_update";
        }

    private:
        engine::Tensor& emb_;
        engine::Tensor& out_;
        std::size_t input_id_;
        std::vector<float> hidden_;
        std::vector<float> dlogits_;
        float learning_rate_;
    };

    struct EpochStats {
        std::size_t lines{0};
        std::size_t token_pairs{0};
        float avg_loss{0.0f};
    };

    EpochStats run_epoch(float learning_rate = 0.01f, bool use_autodiff = true) {
        if (!ds_.open()) {
            throw std::runtime_error("LMTrainer::run_epoch: failed to open dataset");
        }

        std::string line;
        std::size_t n = 0;
        std::size_t pair_count = 0;
        double total_loss = 0.0;

        auto& emb = model_.get_embedding().token_embedding();     // [vocab, d_model]
        auto& out = model_.get_output_projection();               // [d_model, vocab]
        const auto d_model = model_.config().d_model;
        const auto vocab = model_.config().vocab_size;

        while (ds_.next(line)) {
            ++n;
            const auto ids = tok_.encode(line);
            if (ids.size() < 2) {
                continue;
            }

            for (std::size_t t = 0; t + 1 < ids.size(); ++t) {
                const std::size_t input_id = ids[t] % vocab;
                const std::size_t target_id = ids[t + 1] % vocab;

                // Build hidden representation h = embedding(token) + positional_encoding(pos)
                const auto hidden = model_.encode_token(input_id, t);

                // logits = h * W_out
                auto logits = model_.logits_from_hidden(hidden); // [1, vocab]

                // Softmax (stable)
                float max_logit = -std::numeric_limits<float>::infinity();
                for (std::size_t c = 0; c < vocab; ++c) {
                    max_logit = std::max(max_logit, logits.at(0, c));
                }

                float sum_exp = 0.0f;
                std::vector<float> probs(vocab, 0.0f);
                for (std::size_t c = 0; c < vocab; ++c) {
                    const float ex = std::exp(logits.at(0, c) - max_logit);
                    probs[c] = ex;
                    sum_exp += ex;
                }
                if (sum_exp <= 0.0f) {
                    continue;
                }
                for (std::size_t c = 0; c < vocab; ++c) {
                    probs[c] /= sum_exp;
                }

                const float p_target = std::max(probs[target_id], 1e-9f);
                const float loss = -std::log(p_target);
                total_loss += static_cast<double>(loss);
                ++pair_count;

                // Gradient wrt logits: dL/dz = p - y
                std::vector<float> dlogits = probs;
                dlogits[target_id] -= 1.0f;

                if (use_autodiff) {
                    std::vector<float> hidden_vec(d_model, 0.0f);
                    for (std::size_t j = 0; j < d_model; ++j) {
                        hidden_vec[j] = hidden.at(0, j);
                    }

                    engine::AutodiffTape tape(true);
                    auto node = std::make_shared<NextTokenUpdateNode>(
                        emb, out, input_id, std::move(hidden_vec), dlogits, learning_rate);
                    tape.record(node);

                    engine::Variable loss_var;
                    loss_var.name = "cross_entropy";
                    loss_var.value = engine::Tensor({1, 1}, loss);
                    tape.backward(loss_var, 1.0f);
                } else {
                    // Fallback manual path.
                    std::vector<float> emb_grad(d_model, 0.0f);
                    for (std::size_t j = 0; j < d_model; ++j) {
                        float g = 0.0f;
                        for (std::size_t c = 0; c < vocab; ++c) {
                            g += out.at(j, c) * dlogits[c];
                        }
                        emb_grad[j] = g;
                    }

                    for (std::size_t j = 0; j < d_model; ++j) {
                        const float hj = hidden.at(0, j);
                        for (std::size_t c = 0; c < vocab; ++c) {
                            out.at(j, c) -= learning_rate * hj * dlogits[c];
                        }
                    }

                    for (std::size_t j = 0; j < d_model; ++j) {
                        emb.at(input_id, j) -= learning_rate * emb_grad[j];
                    }
                }
            }

            if (n % 500 == 0) {
                const float running = (pair_count == 0)
                                          ? 0.0f
                                          : static_cast<float>(total_loss / static_cast<double>(pair_count));
                std::cout << "[train] processed " << n << " lines, token_pairs=" << pair_count
                          << " avg_loss=" << running << "\n";
            }
        }

        EpochStats stats;
        stats.lines = n;
        stats.token_pairs = pair_count;
        stats.avg_loss = (pair_count == 0)
                             ? 0.0f
                             : static_cast<float>(total_loss / static_cast<double>(pair_count));

        std::cout << "[train] epoch complete lines=" << stats.lines
                  << " token_pairs=" << stats.token_pairs
                  << " avg_loss=" << stats.avg_loss
                  << " using " << model_.info()
                  << " (autodiff=" << (use_autodiff ? "on" : "off") << ")\n";
        return stats;
    }

private:
    models::TinyDecoderTransformer& model_;
    LineDataset& ds_;
    models::ByteTokenizer tok_{};
};

} // namespace codebeat::training
