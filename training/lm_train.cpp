#include "engine/ops.hpp"
#include "engine/tensor.hpp"
#include "models/transformer.hpp"
#include "training/dataset.hpp"
#include "training/trainer.hpp"

#include <iostream>
#include <sstream>
#include <vector>

int main() {
    using namespace codebeat;

    // Numeric smoke test for foundational engine ops.
    engine::Tensor a({2, 3}, 0.0f);
    engine::Tensor b({3, 2}, 0.0f);
    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = static_cast<float>(i + 1);
    }
    for (std::size_t i = 0; i < b.size(); ++i) {
        b[i] = static_cast<float>(i + 1) * 0.5f;
    }

    const auto mm = engine::ops::matmul_2d(a, b);
    const auto prob = engine::ops::rowwise_softmax_2d(mm);
    const auto loss = engine::ops::cross_entropy_from_logits_2d(mm, {0, 1});
    std::cout << "[engine] matmul shape=" << mm.rows() << "x" << mm.cols()
              << " first_prob=" << prob.at(0, 0)
              << " ce_loss=" << loss << "\n";

    models::TinyDecoderTransformer model(models::TransformerConfig{});
    training::LineDataset dataset("data/raw/corpus.txt");

    if (!dataset.open()) {
        std::cerr << "Could not open data/raw/corpus.txt. Create it with one sample per line.\n";
        return 1;
    }

    training::LMTrainer trainer(model, dataset);
    std::vector<training::LMTrainer::EpochStats> stats;
    constexpr int kEpochs = 3;
    for (int epoch = 0; epoch < kEpochs; ++epoch) {
        std::cout << "[train] starting epoch " << (epoch + 1) << "/" << kEpochs << "\n";
        stats.push_back(trainer.run_epoch(0.01f, true));

        std::ostringstream ckpt_prefix;
        ckpt_prefix << "data/processed/codebeat_epoch_" << (epoch + 1);
        if (model.save_checkpoint(ckpt_prefix.str())) {
            std::cout << "[train] checkpoint saved: " << ckpt_prefix.str() << "(.emb.bin/.out.bin)\n";
        } else {
            std::cout << "[train] warning: failed to save checkpoint for epoch " << (epoch + 1) << "\n";
        }
    }

    if (!stats.empty()) {
        std::cout << "[train] loss trend: first=" << stats.front().avg_loss
                  << " last=" << stats.back().avg_loss << "\n";
    }

    if (model.save_checkpoint("data/processed/codebeat_latest")) {
        std::cout << "[train] latest checkpoint saved to data/processed/codebeat_latest(.emb.bin/.out.bin)\n";
    }

    return 0;
}
