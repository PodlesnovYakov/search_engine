#pragma once
#include "Common.h"
#include "Document.h"
#include "Encoding.h"
#include <vector>
#include <string>

class ForwardIndex {
public:
    void add_document(const Document& doc) {
        docs_.push_back(doc);
        // Считаем длину (кол-во слов) для BM25. Грубо по пробелам.
        size_t len = 0;
        for (char c : doc.title + doc.plot) { if (std::isspace(c)) len++; }
        doc_lengths_.push_back(len + 1);
        total_length_ += (len + 1);
    }

    const Document& get_document(DocId id) const { return docs_.at(id); }
    uint32_t get_doc_length(DocId id) const { return (id < doc_lengths_.size()) ? doc_lengths_[id] : 0; }
    double get_avg_dl() const { return docs_.empty() ? 0.0 : static_cast<double>(total_length_) / docs_.size(); }
    size_t size() const { return docs_.size(); }

    void save(const std::string& filename) const {
        std::ofstream out(filename, std::ios::binary);
        write_varint(out, docs_.size());
        for (const auto& doc : docs_) {
            write_varint(out, doc.id);
            write_string(out, doc.title);
            write_string(out, doc.plot);
        }
        write_delta_vector(out, doc_lengths_); // Сжимаем длины
    }

    void load(const std::string& filename) {
        std::ifstream in(filename, std::ios::binary);
        if(!in.is_open()) return; // Обработка ошибки в вызывающем коде
        size_t count = read_varint(in);
        docs_.clear(); docs_.reserve(count);
        total_length_ = 0;
        for (size_t i = 0; i < count; ++i) {
            Document doc;
            doc.id = read_varint(in);
            read_string(in, doc.title);
            read_string(in, doc.plot);
            docs_.push_back(doc);
        }
        doc_lengths_ = read_delta_vector(in);
        for(auto l : doc_lengths_) total_length_ += l;
    }

private:
    std::vector<Document> docs_;
    std::vector<uint32_t> doc_lengths_;
    uint64_t total_length_ = 0;
};