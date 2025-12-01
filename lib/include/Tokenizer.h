#pragma once
#include <string>
#include <vector>
#include <unordered_set>

class Tokenizer {
public:
    Tokenizer();
    std::vector<std::string> tokenize(const std::string& text) const;

private:
    std::string to_lower(const std::string& str) const;
    std::unordered_set<std::string> stop_words_;
};