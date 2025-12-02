#pragma once
#include "Common.h"
#include "Document.h"
#include "Tokenizer.h"
#include <unordered_map>
#include <map>
#include <vector>
#include <string>

// Структура для Инвертированного индекса (Docs + Skips)
struct PostingsList {
    DocList docs;              
    std::vector<size_t> skips; 
    size_t skip_step = 0;      
};

// Инвертированный: Term -> Field -> List
using InvertedIndex = std::unordered_map<Term, std::unordered_map<std::string, PostingsList>>;

// Позиционный: Term -> Field -> DocId -> Positions
// Обрати внимание: map<DocId, ...> не идеален для бинарного сжатия, 
// но для текущей задачи (сериализация) подходит.
using PositionalIndex = std::unordered_map<Term, std::unordered_map<std::string, std::map<DocId, Positions>>>;

class Index {
public:
    void add_document(const Document& doc);
    void build_skip_pointers(); // Сортировка + Скипы

    // Сериализация
    void save(const std::string& filename) const;
    void load(const std::string& filename);

    const InvertedIndex& get_inverted_index() const { return inverted_index_; }
    const PositionalIndex& get_positional_index() const { return positional_index_; }
    const std::map<DocId, Document>& get_documents() const { return documents_; }

private:
    void add_field_to_index(DocId doc_id, const std::string& field_name, const std::string& text);

    InvertedIndex inverted_index_;
    PositionalIndex positional_index_;
    std::map<DocId, Document> documents_;
    Tokenizer tokenizer_;
};