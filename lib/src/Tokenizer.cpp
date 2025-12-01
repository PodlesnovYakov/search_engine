#include "Tokenizer.h"
#include <algorithm>
#include <cctype>

Tokenizer::Tokenizer() {
    stop_words_ = {
        "a", "an", "the", "in", "on", "of", "for", "with", "is", "are", "was",
        "were", "it", "he", "she", "they", "i", "you", "and", "or", "but"
    };
}

std::string Tokenizer::to_lower(const std::string& str) const {
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return lower_str;
}

std::vector<std::string> Tokenizer::tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    std::string current_token;
    
    for (char ch : text) {
        if (std::isalnum(ch)) {
            current_token += ch;
        } else {
            if (!current_token.empty()) {
                std::string lower_token = to_lower(current_token);
                if (stop_words_.find(lower_token) == stop_words_.end()) {
                    tokens.push_back(lower_token);
                }
                current_token.clear();
            }
        }
    }
    
    if (!current_token.empty()) {
        std::string lower_token = to_lower(current_token);
        if (stop_words_.find(lower_token) == stop_words_.end()) {
            tokens.push_back(lower_token);
        }
    }
    
    return tokens;
}

// "A Man's Life: The Great War (1918)" для примера перейдёт в  ["man", "s", "life", "great", "war", "1918"]