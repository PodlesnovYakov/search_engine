#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "json.hpp"
#include "Index.h"
#include "SearchEngine.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>

using json = nlohmann::json;

// здесь тупо читаем файл, где 1 столбец это тайтл и 7 столбец плот
// посимвольно читаем т.к. могут встречаться символы перевода строк, знаки препинания и т.д.
std::vector<Document> parse_csv(const std::string& filename, int limit) {
    std::vector<Document> docs;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return docs;
    }

    std::string line;
    uint32_t id = 0;
    std::getline(file, line);

    std::string record_buffer;
    bool in_quotes = false;
    char c;

    while (file.get(c)) {
        if (c == '"') {
            in_quotes = !in_quotes;
        }

        if (c == '\n' && !in_quotes) {
            std::stringstream ss(record_buffer);
            std::vector<std::string> row;
            std::string field;
            bool field_in_quotes = false;
            
            for(char ch : record_buffer) {
                if (ch == '"') {
                    field_in_quotes = !field_in_quotes;
                } else if (ch == ',' && !field_in_quotes) {
                    row.push_back(field);
                    field.clear();
                } else {
                    field += ch;
                }
            }
            row.push_back(field);

            if (row.size() >= 8) {
                docs.push_back({id++, row[1], row[7]});
            }

            record_buffer.clear(); 
            if (limit != 0 && id >= limit) {
                break;
            }

        } else {
            record_buffer += c;
        }
    }
    
    if (!record_buffer.empty() && (limit == 0 || id < limit)) {
        std::stringstream ss(record_buffer);
        std::vector<std::string> row;
        std::string field;
        bool field_in_quotes = false;
        
        for(char ch : record_buffer) {
            if (ch == '"') {
                field_in_quotes = !field_in_quotes;
            } else if (ch == ',' && !field_in_quotes) {
                row.push_back(field);
                field.clear();
            } else {
                field += ch;
            }
        }
        row.push_back(field);

        if (row.size() >= 8) {
            docs.push_back({id++, row[1], row[7]});
        }
    }

    return docs;
}


int main(void) {
    Index index;
    std::cout << "Loading and indexing documents..." << std::endl;
    const std::string csv_path = "data/wiki_movie_plots_deduped.csv";
    
    auto documents = parse_csv(csv_path, 0);

    if (documents.empty()) {
        std::cerr << "Warning: No documents were loaded. Please check the file path and format." << std::endl;
    }

    for (const auto& doc : documents) {
        index.add_document(doc);
    }
    std::cout << "Indexing complete. " << index.get_documents().size() << " documents indexed." << std::endl;
    
    SearchEngine engine(index);
    httplib::Server svr;

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        std::ifstream file("web/index.html");
        std::stringstream buffer;
        buffer << file.rdbuf();
        res.set_content(buffer.str(), "text/html");
    });

    svr.Get("/search", [&](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("q")) {
            res.status = 400;
            res.set_content("Missing 'q' parameter", "text/plain");
            return;
        }
        
        std::string query = req.get_param_value("q");
        std::cout << "Received query: " << query << std::endl;
        
        json response_json = json::array();
        try {
            auto result_ids = engine.search(query);
            size_t count = 0;
            for (uint32_t id : result_ids) {
                if(count++ >= 50) break;
                const auto& doc = index.get_documents().at(id);
                json doc_json;
                doc_json["title"] = doc.title;
                doc_json["plot_snippet"] = doc.plot.substr(0, 300) + "...";
                response_json.push_back(doc_json);
            }
        } catch (const std::exception& e) {
            res.status = 500;
            json err_json;
            err_json["error"] = e.what();
            res.set_content(err_json.dump(4), "application/json");
            return;
        }
        
        res.set_content(response_json.dump(4), "application/json");
    });

    std::cout << "Server started at http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);
    
    return 0;
}