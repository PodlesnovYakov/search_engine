#include "Index.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <vector>

// Нормальный парсер CSV с учетом экранирования ""
std::vector<std::string> parse_csv_line(const std::string& line) {
    std::vector<std::string> result;
    std::string cell;
    bool in_quotes = false;
    
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    cell += '"'; // Экранированная кавычка
                    i++;
                } else {
                    in_quotes = false; // Конец кавычек
                }
            } else {
                cell += c;
            }
        } else {
            if (c == '"') {
                in_quotes = true;
            } else if (c == ',') {
                result.push_back(cell);
                cell.clear();
            } else {
                cell += c;
            }
        }
    }
    result.push_back(cell);
    return result;
}

std::vector<Document> parse_csv(const std::string& filename) {
    std::vector<Document> docs;
    std::ifstream file(filename);
    if (!file.is_open()) return docs;

    std::string line;
    uint32_t id = 0;
    
    // Пропускаем заголовок
    if (!std::getline(file, line)) return docs;

    std::string record_buffer;
    bool in_quotes = false;
    
    // Чтение с учетом многострочных полей
    while (std::getline(file, line)) {
        if (!record_buffer.empty()) record_buffer += "\n";
        record_buffer += line;

        int quote_count = 0;
        for (char c : record_buffer) if (c == '"') quote_count++;
        
        if (quote_count % 2 == 0) {
            auto row = parse_csv_line(record_buffer);
            if (row.size() >= 8) {
                docs.push_back({id++, row[1], row[7]});
            }
            record_buffer.clear();
        }
    }
    return docs;
}

int main(int argc, char* argv[]) {
    // Включаем синхронизацию для вывода, в индексаторе скорость cout не так важна
    std::cout << "Parsing CSV..." << std::endl;
    auto docs = parse_csv("data/wiki_movie_plots_deduped.csv");
    
    Index index;
    std::cout << "Indexing " << docs.size() << " docs..." << std::endl;
    for (const auto& doc : docs) index.add_document(doc);
    
    std::cout << "Building Skip Pointers..." << std::endl;
    index.build_skip_pointers();

    std::cout << "Saving index..." << std::endl;
    std::remove("index.docs");
    std::remove("index.inv");

    try {
        index.save("index");
        std::cout << "Done." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}