#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "json.hpp"
#include "Index.h"
#include "SearchEngine.h"
#include <iostream>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

std::string utf8_truncate(const std::string& str, size_t len) {
    if (str.length() <= len) return str;
    while (len > 0 && (str[len] & 0xC0) == 0x80) len--;
    return str.substr(0, len);
}

int main() {
    Index index;
    std::cout << "Loading index..." << std::endl;
    try { index.load("index"); } 
    catch (...) { std::cerr << "Error!" << std::endl; return 1; }
    
    const auto& forward_index = index.get_forward_index();
    SearchEngine engine(index);
    httplib::Server svr;
    
    svr.set_read_timeout(5, 0);
    svr.set_write_timeout(5, 0);

    svr.Get("/", [](const auto&, auto& res) {
        std::ifstream file("web/index.html");
        if(file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), "text/html");
        } else res.set_content("No UI", "text/html");
    });

    svr.Get("/search", [&](const auto& req, auto& res) {
        if (!req.has_param("q")) return;
        std::string query = req.get_param_value("q");
        
        double k1 = 1.2;
        double b = 0.75;
        double w_title = 5.0;

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
                    std::string snip = utf8_truncate(d.plot, 300) + "...";
                    j.push_back({{"id", id}, {"title", d.title}, {"plot_snippet", snip}});
                }
            }
            res.set_content(j.dump(-1, ' ', false, json::error_handler_t::replace), "application/json");
        } catch (...) { res.status = 500; }
    });

    svr.listen("0.0.0.0", 8080);
    return 0;
}

