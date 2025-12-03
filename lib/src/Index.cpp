#include "Index.h"
#include "Encoding.h" // <-- Наши функции сжатия
#include <algorithm>
#include <cmath>
#include <map>

void Index::add_document(const Document& doc) {
    forward_index_.add_document(doc);
    add_field_to_index(doc.id, "title", doc.title);
    add_field_to_index(doc.id, "plot", doc.plot);
}

void Index::add_field_to_index(DocId doc_id, const std::string& field_name, const std::string& text) {
    auto tokens = tokenizer_.tokenize(text);
    
    // Группируем позиции по словам
    std::map<std::string, std::vector<uint32_t>> term_positions;
    for (size_t i = 0; i < tokens.size(); ++i) {
        term_positions[tokens[i]].push_back(i);
    }

    for (const auto& [term, positions] : term_positions) {
        auto& list = inverted_index_[term][field_name];
        list.docs.push_back(doc_id);
        list.positions.push_back(positions); // Сохраняем позиции
    }
}

void Index::build_skip_pointers() {
    for (auto& [term, fields] : inverted_index_) {
        for (auto& [field, postings] : fields) {
            // Docs уже отсортированы (добавляем по порядку), просто строим скипы
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

void Index::save(const std::string& base_name) const {
    forward_index_.save(base_name + ".docs"); // Прямой индекс

    std::ofstream out(base_name + ".inv", std::ios::binary);
    write_varint(out, inverted_index_.size());
    
    for (const auto& [term, fields_map] : inverted_index_) {
        write_string(out, term);
        write_varint(out, fields_map.size());
        
        for (const auto& [field, postings] : fields_map) {
            write_string(out, field);
            
            // Сжимаем документы (Delta)
            write_delta_vector(out, postings.docs);

            // Сжимаем позиции (Delta)
            write_varint(out, postings.positions.size());
            for (const auto& pos_vec : postings.positions) {
                write_delta_vector(out, pos_vec);
            }

            // Скипы
            write_delta_vector(out, std::vector<uint32_t>(postings.skips.begin(), postings.skips.end()));
            write_varint(out, postings.skip_step);
        }
    }
}

void Index::load(const std::string& base_name) {
    forward_index_.load(base_name + ".docs");

    std::ifstream in(base_name + ".inv", std::ios::binary);
    if (!in.is_open()) throw std::runtime_error("Cannot open .inv file");

    size_t inv_size = read_varint(in);
    for (size_t i = 0; i < inv_size; ++i) {
        Term term;
        read_string(in, term);
        size_t fields_count = read_varint(in);
        
        for (size_t j = 0; j < fields_count; ++j) {
            std::string field;
            read_string(in, field);
            
            PostingsList postings;
            postings.docs = read_delta_vector(in);

            size_t pos_vec_count = read_varint(in);
            postings.positions.reserve(pos_vec_count);
            for (size_t k = 0; k < pos_vec_count; ++k) {
                postings.positions.push_back(read_delta_vector(in));
            }

            auto skips_vec = read_delta_vector(in);
            postings.skips.assign(skips_vec.begin(), skips_vec.end());
            postings.skip_step = read_varint(in);

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