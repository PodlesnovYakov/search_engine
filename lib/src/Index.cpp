#include "Index.h"
#include <fstream>
#include <iostream>
#include <set>       // <--- ДОБАВЛЕНО (исправляет ошибку компиляции)
#include <cmath>
#include <algorithm> // <--- ДОБАВЛЕНО (для sort/unique)

// Хелперы для бинарной записи
template<typename T> void write_binary(std::ofstream& out, const T& value) { out.write(reinterpret_cast<const char*>(&value), sizeof(T)); }
template<typename T> void read_binary(std::ifstream& in, T& value) { in.read(reinterpret_cast<char*>(&value), sizeof(T)); }
void write_string(std::ofstream& out, const std::string& s) { size_t len = s.size(); write_binary(out, len); out.write(s.data(), len); }
void read_string(std::ifstream& in, std::string& s) { size_t len; read_binary(in, len); s.resize(len); in.read(&s[0], len); }

void Index::add_document(const Document& doc) {
    documents_[doc.id] = doc;
    add_field_to_index(doc.id, "title", doc.title);
    add_field_to_index(doc.id, "plot", doc.plot);
}

void Index::add_field_to_index(DocId doc_id, const std::string& field_name, const std::string& text) {
    auto tokens = tokenizer_.tokenize(text);
    
    // 1. Инвертированный индекс
    for (const auto& token : tokens) {
        inverted_index_[token][field_name].docs.push_back(doc_id);
    }
    // 2. Позиционный индекс
    for (size_t i = 0; i < tokens.size(); ++i) {
        positional_index_[tokens[i]][field_name][doc_id].push_back(static_cast<uint32_t>(i));
    }
}

void Index::build_skip_pointers() {
    for (auto& [term, fields] : inverted_index_) {
        for (auto& [field, postings] : fields) {
            // Сортировка и удаление дубликатов
            std::sort(postings.docs.begin(), postings.docs.end());
            auto last = std::unique(postings.docs.begin(), postings.docs.end());
            postings.docs.erase(last, postings.docs.end());

            // Скипы
            if (postings.docs.size() > 4) {
                postings.skip_step = static_cast<size_t>(std::sqrt(postings.docs.size()));
                postings.skips.clear();
                for (size_t i = postings.skip_step; i < postings.docs.size(); i += postings.skip_step) {
                    postings.skips.push_back(i);
                }
            }
        }
    }
}

void Index::save(const std::string& filename) const {
    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open()) return;

    // 1. Документы
    size_t doc_count = documents_.size();
    write_binary(out, doc_count);
    for (const auto& [id, doc] : documents_) {
        write_binary(out, id);
        write_string(out, doc.title);
        write_string(out, doc.plot);
    }

    // 2. Инвертированный индекс
    size_t inv_size = inverted_index_.size();
    write_binary(out, inv_size);
    for (const auto& [term, fields_map] : inverted_index_) {
        write_string(out, term);
        size_t fields_count = fields_map.size();
        write_binary(out, fields_count);
        for (const auto& [field, postings] : fields_map) {
            write_string(out, field);
            size_t docs_size = postings.docs.size();
            write_binary(out, docs_size);
            out.write(reinterpret_cast<const char*>(postings.docs.data()), docs_size * sizeof(DocId));
            
            size_t skips_size = postings.skips.size();
            write_binary(out, skips_size);
            if (skips_size > 0) out.write(reinterpret_cast<const char*>(postings.skips.data()), skips_size * sizeof(size_t));
            write_binary(out, postings.skip_step);
        }
    }

    // 3. Позиционный индекс
    size_t pos_size = positional_index_.size();
    write_binary(out, pos_size);
    for (const auto& [term, fields_map] : positional_index_) {
        write_string(out, term);
        size_t fields_count = fields_map.size();
        write_binary(out, fields_count);
        for (const auto& [field, doc_map] : fields_map) {
            write_string(out, field);
            size_t map_size = doc_map.size();
            write_binary(out, map_size);
            for (const auto& [docid, positions] : doc_map) {
                write_binary(out, docid);
                size_t pos_count = positions.size();
                write_binary(out, pos_count);
                out.write(reinterpret_cast<const char*>(positions.data()), pos_count * sizeof(uint32_t));
            }
        }
    }
}

void Index::load(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) throw std::runtime_error("Cannot open index file");

    // 1. Документы
    size_t doc_count;
    read_binary(in, doc_count);
    for (size_t i = 0; i < doc_count; ++i) {
        Document doc;
        read_binary(in, doc.id);
        read_string(in, doc.title);
        read_string(in, doc.plot);
        documents_[doc.id] = doc;
    }

    // 2. Инвертированный индекс
    size_t inv_size;
    read_binary(in, inv_size);
    for (size_t i = 0; i < inv_size; ++i) {
        Term term;
        read_string(in, term);
        size_t fields_count;
        read_binary(in, fields_count);
        for (size_t j = 0; j < fields_count; ++j) {
            std::string field;
            read_string(in, field);
            PostingsList postings;
            
            size_t docs_size;
            read_binary(in, docs_size);
            postings.docs.resize(docs_size);
            in.read(reinterpret_cast<char*>(postings.docs.data()), docs_size * sizeof(DocId));
            
            size_t skips_size;
            read_binary(in, skips_size);
            if (skips_size > 0) {
                postings.skips.resize(skips_size);
                in.read(reinterpret_cast<char*>(postings.skips.data()), skips_size * sizeof(size_t));
            }
            read_binary(in, postings.skip_step);
            inverted_index_[term][field] = std::move(postings);
        }
    }

    // 3. Позиционный индекс
    if (in.peek() == EOF) return;
    size_t pos_size;
    read_binary(in, pos_size);
    for (size_t i = 0; i < pos_size; ++i) {
        Term term;
        read_string(in, term);
        size_t fields_count;
        read_binary(in, fields_count);
        for (size_t j = 0; j < fields_count; ++j) {
            std::string field;
            read_string(in, field);
            size_t map_size;
            read_binary(in, map_size);
            auto& doc_map = positional_index_[term][field];
            for (size_t k = 0; k < map_size; ++k) {
                DocId docid;
                read_binary(in, docid);
                size_t vec_size;
                read_binary(in, vec_size);
                std::vector<uint32_t> positions(vec_size);
                in.read(reinterpret_cast<char*>(positions.data()), vec_size * sizeof(uint32_t));
                doc_map[docid] = std::move(positions);
            }
        }
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