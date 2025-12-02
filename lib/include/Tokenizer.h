#pragma once
#include "Common.h"
#include <unordered_set>

class Tokenizer {
public:
    Tokenizer();
    Tokens tokenize(const std::string& text) const;

private:
    std::string to_lower(const std::string& str) const;
    std::unordered_set<std::string> stop_words_;
};