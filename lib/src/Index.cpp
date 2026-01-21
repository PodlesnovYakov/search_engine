#include "Index.h"
#include "Encoding.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <iostream>

void Index::add_document(const Document& doc) {
    forward_index_.add_document(doc);
    add_field_to_index(doc.id, "title", doc.title);
    add_field_to_index(doc.id, "plot", doc.plot);
}

void Index::add_field_to_index(DocId doc_id, const std::string& field_name, const std::string& text) {
    auto tokens = tokenizer_.tokenize(text);
    if (tokens.empty()) return;

    std::map<std::string, std::vector<uint32_t>> term_positions;
    for (size_t i = 0; i < tokens.size(); ++i) {
        term_positions[tokens[i]].push_back(i);
    }
    for (const auto& [term, positions] : term_positions) {
        auto& list = inverted_index_[term][field_name];
        list.docs.push_back(doc_id);
        list.positions.push_back(positions);
    }
}

void Index::build_skip_pointers() {
    for (auto& [term, fields] : inverted_index_) {
        for (auto& [field, postings] : fields) {
            if (postings.docs.size() > 16) {
                postings.skip_step = static_cast<size_t>(std::sqrt(postings.docs.size()));
                postings.skips.clear();
                for (size_t i = postings.skip_step; i < postings.docs.size(); i += postings.skip_step) {
                    postings.skips.push_back(i);
                }
            } else {
                postings.skip_step = 0;
                postings.skips.clear();
            }
        }
    }
}

void Index::save(const std::string& base_name) const {
    forward_index_.save(base_name + ".docs");

    std::ofstream out(base_name + ".inv", std::ios::binary);
    if (!out.is_open()) throw std::runtime_error("Cannot open .inv for writing");

    write_varint(out, 0xCAFEBABE);
    write_varint(out, inverted_index_.size());
    
    for (const auto& [term, fields_map] : inverted_index_) {
        write_string(out, term);
        write_varint(out, fields_map.size());
        
        for (auto [field, postings] : fields_map) {
            write_string(out, field);
            if (!std::is_sorted(postings.docs.begin(), postings.docs.end())) {
            }

            write_delta_vector(out, postings.docs);
            
            write_varint(out, postings.positions.size());
            for (auto& pos_vec : postings.positions) {
                std::sort(pos_vec.begin(), pos_vec.end()); 
                write_delta_vector(out, pos_vec);
            }
            
            std::vector<uint32_t> skip_vec;
            for(auto s : postings.skips) skip_vec.push_back(static_cast<uint32_t>(s));
            write_delta_vector(out, skip_vec);
            
            write_varint(out, postings.skip_step);
        }
    }
    write_varint(out, 0xDEADBEEF);
}

void Index::load(const std::string& base_name) {
    inverted_index_.clear();
    forward_index_.load(base_name + ".docs");

    std::ifstream in(base_name + ".inv", std::ios::binary);
    if (!in.is_open()) throw std::runtime_error("Cannot open .inv file");

    if (read_varint(in) != 0xCAFEBABE) throw std::runtime_error("Invalid magic header");

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
            if (pos_vec_count != postings.docs.size()) {
                std::cerr << "CORRUPTION detected for term '" << term << "': docs=" 
                          << postings.docs.size() << ", pos=" << pos_vec_count << ". Clearing." << std::endl;
                for(size_t k=0; k<pos_vec_count; ++k) read_delta_vector(in);
                read_delta_vector(in);
                read_varint(in);
                continue;
            }

            postings.positions.reserve(pos_vec_count);
            for (size_t k = 0; k < pos_vec_count; ++k) {
                postings.positions.push_back(read_delta_vector(in));
            }

            auto skip_vec = read_delta_vector(in);
            for(auto s : skip_vec) {
                if (s < postings.docs.size()) postings.skips.push_back(s);
            }
            
            postings.skip_step = read_varint(in);
            
            inverted_index_[term][field] = std::move(postings);
        }
    }
    if (read_varint(in) != 0xDEADBEEF) throw std::runtime_error("Invalid magic footer");
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