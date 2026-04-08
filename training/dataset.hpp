#pragma once

#include <fstream>
#include <string>

namespace codebeat::training {

class LineDataset {
public:
    explicit LineDataset(std::string path) : path_(std::move(path)) {}

    bool open() {
        in_.close();
        in_.open(path_);
        return static_cast<bool>(in_);
    }

    bool next(std::string& line) {
        return static_cast<bool>(std::getline(in_, line));
    }

private:
    std::string path_;
    std::ifstream in_;
};

} // namespace codebeat::training
