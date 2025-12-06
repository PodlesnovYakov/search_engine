#pragma once
#include "Index.h"
#include "Tokenizer.h"
#include "Common.h"
#include <optional>
#include <variant>

class SearchEngine {
public:
    explicit SearchEngine(const Index& index);//добавить проксимити
    DocList search(const std::string& query_str, double k1 = 1.2, double b = 0.75, double w_title = 5.0) const;

    struct QueryTerm {
        Term term;
        std::optional<std::string> field;
    };

private:
    Tokens tokenize_query(const std::string& s) const;
    Tokens insert_implicit_and(const Tokens& tokens) const;
    Tokens to_rpn(const Tokens& tokens) const;
    DocList evaluate_rpn(const Tokens& rpn) const;

    const PostingsList* get_postings(const QueryTerm& q_term) const;
    DocList get_doc_ids(const QueryTerm& q_term) const;

    DocList execute_intersect(const PostingsList* left, const PostingsList* right) const;
    DocList execute_intersect_vec(const DocList& left, const PostingsList* right) const;
    DocList execute_intersect_vec_vec(const DocList& left, const DocList& right) const;

    DocList execute_union(const DocList& a, const DocList& b) const;
    DocList execute_not(const DocList& operand) const;
    
    DocList execute_prox(const std::string& op_token, const QueryTerm& left, const QueryTerm& right, const DocList& l_docs, const DocList& r_docs) const;

    QueryTerm parse_query_token(const std::string& token) const;
    static bool is_operator(const std::string& token);
    static bool is_term_like(const std::string& token);

    const Index& index_;
    Tokenizer tokenizer_;
};