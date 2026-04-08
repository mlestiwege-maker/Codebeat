#include "engine/ops.hpp"
#include "engine/tensor.hpp"
#include "models/transformer.hpp"
#include "training/dataset.hpp"
#include "training/trainer.hpp"

#include <iostream>

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
    trainer.run_epoch();

    return 0;
}
