#include "Index.h"
void Index::add_document(const Document& doc) {
    documents_[doc.id] = doc;
    add_field_to_index(doc.id, "title", doc.title);
    add_field_to_index(doc.id, "plot", doc.plot);
}

void Index::add_field_to_index(uint32_t doc_id, const std::string& field_name, const std::string& text) {
    auto tokens = tokenizer_.tokenize(text);
    std::set<std::string> unique_tokens(tokens.begin(), tokens.end());

    for (const auto& token : unique_tokens) {
        inverted_index_[token][field_name].insert(doc_id);
    }
    
    for (size_t i = 0; i < tokens.size(); ++i) {
        positional_index_[tokens[i]][field_name][doc_id].push_back(i);
    }
}

/*
  Обратный индекс
  "quick": {
    "title": { docs: [42, 105, 200, ...] } // Список ID документов
    "plot": {docs: [42, 100, 13]}
  },
  "brown": {
    "title": { docs: [42, 110, ...] }
  },
  "fox": {
    "title": { docs: [42, 300, 512, ...] }
  }
}
*/
/*
    Координатный индекс
{
  "quick": {
    "title": { 42: [0], 105: [0, 15], ... } // В док. 42 на позиции 0. В док. 105 на 0 и 15.
  },
  "brown": {
    "title": { 42: [1], 110: [4], ... } // В док. 42 на позиции 1.
  },
  "fox": {
    "title": { 42: [2], 300: [8], ... }
  }
}

*/