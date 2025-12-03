#include "Index.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio> // remove

std::vector<Document> parse_csv(const std::string& filename) {
    std::vector<Document> docs;
    std::ifstream file(filename);
    if (!file.is_open()) return docs;

    std::string line;
    uint32_t id = 0;
    std::getline(file, line); 

    std::string record_buffer;
    bool in_quotes = false;
    char c;

    while (file.get(c)) {
        if (c == '"') in_quotes = !in_quotes;
        if (c == '\n' && !in_quotes) {
            std::stringstream ss(record_buffer);
            std::vector<std::string> row;
            std::string field;
            bool fq = false;
            for(char ch : record_buffer) {
                if (ch == '"') fq = !fq;
                else if (ch == ',' && !fq) { row.push_back(field); field.clear(); }
                else field += ch;
            }
            row.push_back(field);
            if (row.size() >= 8) docs.push_back({id++, row[1], row[7]});
            record_buffer.clear();
        } else {
            record_buffer += c;
        }
    }
    // Хвост
    if (!record_buffer.empty()) {
         std::stringstream ss(record_buffer);
            std::vector<std::string> row;
            std::string field;
            bool fq = false;
            for(char ch : record_buffer) {
                if (ch == '"') fq = !fq;
                else if (ch == ',' && !fq) { row.push_back(field); field.clear(); }
                else field += ch;
            }
            row.push_back(field);
            if (row.size() >= 8) docs.push_back({id++, row[1], row[7]});
    }
    return docs;
}

int main(int argc, char* argv[]) {
    std::cout << "Parsing CSV..." << std::endl;
    auto docs = parse_csv("data/wiki_movie_plots_deduped.csv");
    
    Index index;
    std::cout << "Indexing " << docs.size() << " docs..." << std::endl;
    for (const auto& doc : docs) index.add_document(doc);
    
    std::cout << "Building Skip Pointers & Sorting..." << std::endl;
    index.build_skip_pointers();

    std::cout << "Saving index..." << std::endl;
    
    // Удаляем старые файлы, чтобы не было конфликтов
    std::remove("index.docs");
    std::remove("index.inv");

    try {
        index.save("index");
        std::cout << "Done. Index saved successfully." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR SAVING INDEX: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}