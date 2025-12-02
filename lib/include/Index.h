#pragma once
#include "Common.h"
#include "Document.h"
#include "Tokenizer.h"
#include <unordered_map>
#include <map>
#include <vector>
#include <string>

struct PostingsList {
    DocList docs;                        // Список ID документов
    std::vector<Positions> positions;    // Список списков позиций (positions[i] для docs[i])
    std::vector<size_t> skips;           // Скипы
    size_t skip_step = 0;
};

// Теперь у нас ТОЛЬКО ОДИН индекс. Позиционный индекс (мапа) УДАЛЕН.
using InvertedIndex = std::unordered_map<Term, std::unordered_map<std::string, PostingsList>>;

class Index {
public:
    void add_document(const Document& doc);
    void build_skip_pointers();
    void save(const std::string& filename) const;
    void load(const std::string& filename);

    const InvertedIndex& get_inverted_index() const { return inverted_index_; }
    // get_positional_index() БОЛЬШЕ НЕТ
    const std::map<DocId, Document>& get_documents() const { return documents_; }

private:
    void add_field_to_index(DocId doc_id, const std::string& field_name, const std::string& text);

    InvertedIndex inverted_index_;
    std::map<DocId, Document> documents_;
    Tokenizer tokenizer_;
};