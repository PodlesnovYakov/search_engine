#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "json.hpp"
#include "Index.h"
#include "SearchEngine.h"
#include <iostream>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

// Функция для очистки строки от битого UTF-8
std::string clean_utf8(const std::string& str) {
    std::string res;
    res.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (c < 0x80) {
            res += c; // ASCII
        } else {
            // Простейшая защита: пропускаем явно битые байты, если это не начало символа
            // В идеале тут нужна полноценная библиотека типа ICU, но для домашки хватит
            // простого копирования. Библиотека nlohmann::json с replace сама справится,
            // если строка не обрезана посередине символа.
            res += c;
        }
    }
    return res;
}

// Безопасная обрезка
std::string utf8_truncate(const std::string& str, size_t len) {
    if (str.length() <= len) return str;
    while (len > 0 && (str[len] & 0xC0) == 0x80) len--;
    return str.substr(0, len);
}

int main() {
    Index index;
    std::cout << "Loading index..." << std::endl;
    try {
        index.load("index.bin");
    } catch (...) {
        std::cerr << "Error loading index!" << std::endl;
        return 1;
    }
    std::cout << "Loaded." << std::endl;
    
    SearchEngine engine(index);
    httplib::Server svr;
    
    // Таймауты
    svr.set_read_timeout(5, 0);
    svr.set_write_timeout(5, 0);

    svr.Get("/", [](const auto&, auto& res) {
        std::ifstream file("web/index.html");
        if(file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), "text/html");
        } else res.set_content("No UI", "text/plain");
    });

    svr.Get("/search", [&](const auto& req, auto& res) {
        if (!req.has_param("q")) return;
        auto q = req.get_param_value("q");
        std::cout << "Query: " << q << std::endl;

        try {
            auto ids = engine.search(q);
            std::cout << "Found: " << ids.size() << std::endl;

            json j = json::array();
            size_t cnt = 0;
            for (auto id : ids) {
                if (cnt++ >= 20) break;
                // БЕЗОПАСНЫЙ ПОИСК ДОКУМЕНТА
                auto it = index.get_documents().find(id);
                if (it != index.get_documents().end()) {
                    const auto& d = it->second;
                    
                    std::string safe_title = utf8_truncate(d.title, 100);
                    std::string safe_plot = utf8_truncate(d.plot, 300);
                    if (d.plot.length() > 300) safe_plot += "...";

                    j.push_back({
                        {"title", safe_title}, 
                        {"plot_snippet", safe_plot}
                    });
                }
            }
            // ВАЖНО: replace заменяет битые символы на 
            res.set_content(j.dump(-1, ' ', false, json::error_handler_t::replace), "application/json");
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            res.status = 500;
        }
    });

    svr.listen("0.0.0.0", 8080);
    return 0;
}