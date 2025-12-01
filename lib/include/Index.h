#pragma once
#include "Document.h"
#include "Tokenizer.h"
#include "SkipList.h" 
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <set>

using InvertedIndex = std::unordered_map<std::string, std::unordered_map<std::string, SkipList>>;
using PositionalIndex = std::unordered_map<std::string, std::unordered_map<std::string, std::map<uint32_t, std::vector<uint32_t>>>>;

class Index {
public:
    void add_document(const Document& doc);

    const InvertedIndex& get_inverted_index() const { return inverted_index_; }
    const PositionalIndex& get_positional_index() const { return positional_index_; }
    const std::map<uint32_t, Document>& get_documents() const { return documents_; }

private:
    void add_field_to_index(uint32_t doc_id, const std::string& field_name, const std::string& text);

    InvertedIndex inverted_index_;
    PositionalIndex positional_index_;
    std::map<uint32_t, Document> documents_;
    Tokenizer tokenizer_;
};