#pragma once
#include "Index.h" 
#include <string>
#include <vector>
#include <optional>
#include <variant>

class SearchEngine {
public:
    explicit SearchEngine(const Index& index);
    std::vector<uint32_t> search(const std::string& query_str) const;

private:
    struct QueryTerm {
        std::string term;
        std::optional<std::string> field;
    };

    std::vector<std::string> tokenize_query(const std::string& s) const;
    std::vector<std::string> insert_implicit_and(const std::vector<std::string>& tokens) const;
    std::vector<std::string> to_rpn(const std::vector<std::string>& tokens) const;
    std::vector<uint32_t> evaluate_rpn(const std::vector<std::string>& rpn) const;

    std::vector<uint32_t> get_doc_ids(const QueryTerm& q_term) const;
    std::vector<uint32_t> execute_intersect(const std::vector<uint32_t>& list1, const std::vector<uint32_t>& list2) const;
    
    std::vector<uint32_t> execute_union(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) const;
    std::vector<uint32_t> execute_not(const std::vector<uint32_t>& operand) const;
    std::vector<uint32_t> execute_prox(const std::string& op_token, const std::string& left_token, const std::string& right_token) const;

    QueryTerm parse_query_token(const std::string& token) const;
    static bool is_operator(const std::string& token);
    static bool is_term_like(const std::string& token);

    const Index& index_;
    Tokenizer tokenizer_; 
};