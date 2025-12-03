#pragma once
#include "Common.h"
#include "Document.h"
#include "Tokenizer.h"
#include "ForwardIndex.h" // <-- Добавили
#include <unordered_map>
#include <vector>
#include <string>

struct PostingsList {
    DocList docs;
    // Позиции теперь здесь! Вектор векторов (для сжатия)
    std::vector<std::vector<uint32_t>> positions; 
    std::vector<size_t> skips;
    size_t skip_step = 0;
};

using InvertedIndex = std::unordered_map<Term, std::unordered_map<std::string, PostingsList>>;

class Index {
public:
    void add_document(const Document& doc);
    void build_skip_pointers();
    void save(const std::string& base_name) const;
    void load(const std::string& base_name);

    const InvertedIndex& get_inverted_index() const { return inverted_index_; }
    const ForwardIndex& get_forward_index() const { return forward_index_; }

private:
    void add_field_to_index(DocId doc_id, const std::string& field_name, const std::string& text);

    InvertedIndex inverted_index_;
    ForwardIndex forward_index_;
    Tokenizer tokenizer_;
};