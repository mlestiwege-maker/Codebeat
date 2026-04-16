#include "engine/ops.hpp"
#include "engine/tensor.hpp"
#include "models/transformer.hpp"
#include "training/dataset.hpp"
#include "training/trainer.hpp"

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    using namespace codebeat;

    int epochs = 6;
    float learning_rate = 0.01f;
    std::string corpus_path = "data/raw/corpus.txt";

    if (const char* env_epochs = std::getenv("CODEBEAT_TRAIN_EPOCHS"); env_epochs && *env_epochs) {
        try {
            epochs = std::max(1, std::stoi(env_epochs));
        } catch (...) {
            // Keep default if parsing fails.
        }
    }
    if (const char* env_lr = std::getenv("CODEBEAT_TRAIN_LR"); env_lr && *env_lr) {
        try {
            learning_rate = std::max(1e-5f, std::stof(env_lr));
        } catch (...) {
            // Keep default if parsing fails.
        }
    }
    if (const char* env_corpus = std::getenv("CODEBEAT_TRAIN_CORPUS"); env_corpus && *env_corpus) {
        corpus_path = env_corpus;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--epochs" || arg == "-e") && i + 1 < argc) {
            epochs = std::max(1, std::stoi(argv[++i]));
        } else if ((arg == "--lr" || arg == "-l") && i + 1 < argc) {
            learning_rate = std::max(1e-5f, std::stof(argv[++i]));
        } else if ((arg == "--corpus" || arg == "-c") && i + 1 < argc) {
            corpus_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: codebeat_train [--epochs N] [--lr RATE] [--corpus PATH]\n";
            std::cout << "Env overrides: CODEBEAT_TRAIN_EPOCHS, CODEBEAT_TRAIN_LR, CODEBEAT_TRAIN_CORPUS\n";
            return 0;
        }
    }

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
    training::LineDataset dataset(corpus_path);

    if (!dataset.open()) {
        std::cerr << "Could not open corpus file: " << corpus_path << "\n";
        return 1;
    }

    std::cout << "[train] config epochs=" << epochs
              << " lr=" << learning_rate
              << " corpus=" << corpus_path << "\n";

    training::LMTrainer trainer(model, dataset);
    std::vector<training::LMTrainer::EpochStats> stats;
    for (int epoch = 0; epoch < epochs; ++epoch) {
        std::cout << "[train] starting epoch " << (epoch + 1) << "/" << epochs << "\n";
        stats.push_back(trainer.run_epoch(learning_rate, true));

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
