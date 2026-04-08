#pragma once

#include "engine/tensor.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace codebeat::engine {

inline bool save_tensor_binary(const Tensor& t, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }

    const auto rank = t.rank();
    out.write(reinterpret_cast<const char*>(&rank), sizeof(rank));

    const auto& shape = t.shape();
    for (std::size_t d = 0; d < rank; ++d) {
        out.write(reinterpret_cast<const char*>(&shape[d]), sizeof(shape[d]));
    }

    const auto sz = t.size();
    out.write(reinterpret_cast<const char*>(&sz), sizeof(sz));
    out.write(reinterpret_cast<const char*>(t.data().data()),
              static_cast<std::streamsize>(sz * sizeof(float)));
    return static_cast<bool>(out);
}

inline bool load_tensor_binary(Tensor& t, const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }

    std::size_t rank = 0;
    in.read(reinterpret_cast<char*>(&rank), sizeof(rank));
    if (!in || rank == 0) {
        return false;
    }

    std::vector<std::size_t> shape(rank, 0);
    for (std::size_t d = 0; d < rank; ++d) {
        in.read(reinterpret_cast<char*>(&shape[d]), sizeof(shape[d]));
        if (!in || shape[d] == 0) {
            return false;
        }
    }

    std::size_t sz = 0;
    in.read(reinterpret_cast<char*>(&sz), sizeof(sz));
    if (!in || sz != Tensor::numel(shape)) {
        return false;
    }

    Tensor loaded(shape, 0.0f);
    in.read(reinterpret_cast<char*>(loaded.data().data()),
            static_cast<std::streamsize>(sz * sizeof(float)));
    if (!in) {
        return false;
    }

    t = std::move(loaded);
    return true;
}

} // namespace codebeat::engine
