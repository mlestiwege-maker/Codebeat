#pragma once

#include "engine/tensor.hpp"

#include <fstream>
#include <string>

namespace codebeat::engine {

inline bool save_tensor_binary(const Tensor& t, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }

    const auto sz = t.size();
    out.write(reinterpret_cast<const char*>(&sz), sizeof(sz));
    out.write(reinterpret_cast<const char*>(t.data().data()), static_cast<std::streamsize>(sz * sizeof(float)));
    return static_cast<bool>(out);
}

} // namespace codebeat::engine
