#pragma once

#include "models/transformer.hpp"
#include "training/dataset.hpp"

#include <iostream>

namespace codebeat::training {

class LMTrainer {
public:
    LMTrainer(models::TinyDecoderTransformer& model, LineDataset& ds)
        : model_(model), ds_(ds) {}

    void run_epoch() {
        std::string line;
        std::size_t n = 0;
        while (ds_.next(line)) {
            ++n;
            if (n % 1000 == 0) {
                std::cout << "[train] processed " << n << " samples\n";
            }
        }
        std::cout << "[train] epoch complete on " << n << " samples using "
                  << model_.info() << "\n";
    }

private:
    models::TinyDecoderTransformer& model_;
    LineDataset& ds_;
};

} // namespace codebeat::training
