#include "httplib.h"
#include "json.hpp"
#include "Index.h"
#include "SearchEngine.h"
#include <iostream>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

// Безопасная функция обрезки UTF-8
std::string utf8_truncate(const std::string& str, size_t max_len) {
    if (str.size() <= max_len) return str;
    std::string res = str.substr(0, max_len);
    while (!res.empty() && (res.back() & 0xC0) == 0x80) {
        res.pop_back();
    }
    return res;
}

int main() {
    // Отключаем синхронизацию для скорости
    std::ios::sync_with_stdio(false);

    Index index;
    std::cout << "Loading index..." << std::endl;
    try { 
        index.load("index"); 
        std::cout << "Index loaded. Docs: " << index.get_forward_index().size() << std::endl;
    } 
    catch (const std::exception& e) { 
        std::cerr << "FATAL ERROR loading index: " << e.what() << std::endl; 
        return 1; 
    }
    
    const auto& forward_index = index.get_forward_index();
    SearchEngine engine(index);
    httplib::Server svr;
    
    // ВАЖНО: Принудительно ставим 1 поток, чтобы исключить Race Conditions
    svr.new_task_queue = [] { return new httplib::ThreadPool(1); };

    svr.set_read_timeout(10, 0);
    svr.set_write_timeout(10, 0);

    svr.Get("/", [](const auto&, auto& res) {
        std::ifstream file("web/index.html");
        if(file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), "text/html");
        } else res.set_content("No UI found", "text/html");
    });

    svr.Get("/search", [&](const auto& req, auto& res) {
        if (!req.has_param("q")) return;
        std::string query = req.get_param_value("q");
        
        double k1 = 1.2, b = 0.75, w_title = 5.0;
        if (req.has_param("k1")) try { k1 = std::stod(req.get_param_value("k1")); } catch(...) {}
        if (req.has_param("b")) try { b = std::stod(req.get_param_value("b")); } catch(...) {}
        if (req.has_param("w_title")) try { w_title = std::stod(req.get_param_value("w_title")); } catch(...) {}

        try {
            auto ids = engine.search(query, k1, b, w_title);
            
            json j = json::array();
            size_t cnt = 0;
            for (auto id : ids) {
                if (cnt++ >= 20) break;
                if (id < forward_index.size()) {
                    const auto& d = forward_index.get_document(id);
                    // Копируем строки с защитой
                    std::string title_safe = d.title; 
                    std::string snippet_safe = utf8_truncate(d.plot, 300) + "...";
                    
                    j.push_back({
                        {"id", id}, 
                        {"title", title_safe}, 
                        {"plot_snippet", snippet_safe}
                    });
                }
            }
            res.set_content(j.dump(-1, ' ', false, json::error_handler_t::replace), "application/json");
        } catch (const std::exception& e) { 
            std::cerr << "Search error: " << e.what() << std::endl;
            res.status = 500; 
        }
    });

    std::cout << "Server starting on 0.0.0.0:8080 (Single Threaded)..." << std::endl;
    svr.listen("0.0.0.0", 8080);
    return 0;
}

