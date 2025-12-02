#include "Index.h"
#include <iostream>
#include <algorithm>
#include <cmath>

// Хелперы для строк (для чисел используем Varint из Common.h)
void write_string(std::ofstream& out, const std::string& s) { 
    encode_varint(out, s.size()); 
    out.write(s.data(), s.size()); 
}
void read_string(std::ifstream& in, std::string& s) { 
    size_t len = decode_varint(in); 
    s.resize(len); 
    in.read(&s[0], len); 
}

void Index::add_document(const Document& doc) {
    documents_[doc.id] = doc;
    add_field_to_index(doc.id, "title", doc.title);
    add_field_to_index(doc.id, "plot", doc.plot);
}

void Index::add_field_to_index(DocId doc_id, const std::string& field_name, const std::string& text) {
    auto tokens = tokenizer_.tokenize(text);
    
    // Временная мапа для сбора позиций внутри одного документа
    std::unordered_map<std::string, std::vector<uint32_t>> doc_positions;
    for (size_t i = 0; i < tokens.size(); ++i) {
        doc_positions[tokens[i]].push_back(i);
    }

    // Записываем в основной индекс
    for (const auto& [term, positions] : doc_positions) {
        auto& postings = inverted_index_[term][field_name];
        postings.docs.push_back(doc_id);
        postings.positions.push_back(positions);
    }
}

void Index::build_skip_pointers() {
    // В этой версии мы пишем данные последовательно, они уже отсортированы по DocID,
    // так как мы добавляем документы по порядку (0, 1, 2...).
    // Но если бы порядок был нарушен, тут нужна была бы сложная сортировка двух векторов синхронно.
    // Считаем, что indexer подает документы строго по возрастанию ID.

    for (auto& [term, fields] : inverted_index_) {
        for (auto& [field, postings] : fields) {
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
    encode_varint(out, documents_.size());
    for (const auto& [id, doc] : documents_) {
        encode_varint(out, id);
        write_string(out, doc.title);
        write_string(out, doc.plot);
    }

    // 2. Индекс (СЖАТИЕ!)
    encode_varint(out, inverted_index_.size());
    
    for (const auto& [term, fields_map] : inverted_index_) {
        write_string(out, term);
        encode_varint(out, fields_map.size());
        
        for (const auto& [field, postings] : fields_map) {
            write_string(out, field);
            
            // А. Сохраняем DocID с Delta Encoding
            size_t count = postings.docs.size();
            encode_varint(out, count);
            
            DocId prev_doc = 0;
            for (DocId doc : postings.docs) {
                encode_varint(out, doc - prev_doc); // Пишем разницу!
                prev_doc = doc;
            }

            // Б. Сохраняем Позиции с Delta Encoding
            // (размер массива позиций равен размеру массива документов)
            for (const auto& pos_vec : postings.positions) {
                encode_varint(out, pos_vec.size());
                uint32_t prev_pos = 0;
                for (uint32_t pos : pos_vec) {
                    encode_varint(out, pos - prev_pos); // Пишем разницу!
                    prev_pos = pos;
                }
            }

            // В. Скипы (их можно не сжимать, их мало, или тоже дельтой)
            encode_varint(out, postings.skips.size());
            for (size_t skip : postings.skips) encode_varint(out, skip);
            encode_varint(out, postings.skip_step);
        }
    }
}

void Index::load(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) throw std::runtime_error("Cannot open index file");

    // 1. Документы
    size_t doc_count = decode_varint(in);
    for (size_t i = 0; i < doc_count; ++i) {
        Document doc;
        doc.id = decode_varint(in);
        read_string(in, doc.title);
        read_string(in, doc.plot);
        documents_[doc.id] = doc;
    }

    // 2. Индекс
    size_t inv_size = decode_varint(in);
    for (size_t i = 0; i < inv_size; ++i) {
        Term term;
        read_string(in, term);
        size_t fields_count = decode_varint(in);
        
        for (size_t j = 0; j < fields_count; ++j) {
            std::string field;
            read_string(in, field);
            PostingsList postings;
            
            // А. Читаем Docs (Delta Decoding)
            size_t count = decode_varint(in);
            postings.docs.resize(count);
            DocId prev_doc = 0;
            for (size_t k = 0; k < count; ++k) {
                DocId delta = decode_varint(in);
                postings.docs[k] = prev_doc + delta;
                prev_doc = postings.docs[k];
            }

            // Б. Читаем Позиции (Delta Decoding)
            postings.positions.resize(count);
            for (size_t k = 0; k < count; ++k) {
                size_t pos_count = decode_varint(in);
                postings.positions[k].resize(pos_count);
                uint32_t prev_pos = 0;
                for (size_t p = 0; p < pos_count; ++p) {
                    uint32_t delta = decode_varint(in);
                    postings.positions[k][p] = prev_pos + delta;
                    prev_pos = postings.positions[k][p];
                }
            }

            // В. Скипы
            size_t skips_size = decode_varint(in);
            postings.skips.resize(skips_size);
            for(size_t k=0; k<skips_size; ++k) postings.skips[k] = decode_varint(in);
            postings.skip_step = decode_varint(in);

            inverted_index_[term][field] = std::move(postings);
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