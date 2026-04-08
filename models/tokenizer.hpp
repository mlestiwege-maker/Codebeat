#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace codebeat::models {

class ByteTokenizer {
public:
    [[nodiscard]] std::vector<std::size_t> encode(const std::string& text) const {
        std::vector<std::size_t> ids;
        ids.reserve(text.size());
        for (unsigned char c : text) {
            ids.push_back(static_cast<std::size_t>(c));
        }
        return ids;
    }

    [[nodiscard]] std::string decode(const std::vector<std::size_t>& ids) const {
        std::string out;
        out.reserve(ids.size());
        for (auto id : ids) {
            out.push_back(static_cast<char>(id & 0xFFU));
        }
        return out;
    }
};

} // namespace codebeat::models
