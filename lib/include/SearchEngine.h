#pragma once
#include "Index.h"
#include "Tokenizer.h"
#include "Common.h"
#include <optional>
#include <variant>

class SearchEngine {
public:
    explicit SearchEngine(const Index& index);
    DocList search(const std::string& query_str) const;

    // --- ИСПРАВЛЕНИЕ: Переместили в public, чтобы StackItem мог это видеть ---
    struct QueryTerm {
        Term term;
        std::optional<std::string> field;
    };
    // -----------------------------------------------------------------------

private:
    Tokens tokenize_query(const std::string& s) const;
    Tokens insert_implicit_and(const Tokens& tokens) const;
    Tokens to_rpn(const Tokens& tokens) const;
    DocList evaluate_rpn(const Tokens& rpn) const;

    // ВАЖНО: Возвращаем указатель на данные
    const PostingsList* get_postings(const QueryTerm& q_term) const;
    DocList get_doc_ids(const QueryTerm& q_term) const;

    // ОПТИМИЗИРОВАННОЕ ПЕРЕСЕЧЕНИЕ (Skip Pointers)
    DocList execute_intersect(const PostingsList* left, const PostingsList* right) const;
    DocList execute_intersect_vec(const DocList& left, const PostingsList* right) const;
    DocList execute_intersect_vec_vec(const DocList& left, const DocList& right) const;

    DocList execute_union(const DocList& a, const DocList& b) const;
    DocList execute_not(const DocList& operand) const;
    
    // НАСТОЯЩИЙ NEAR/ADJ
    DocList execute_prox(const std::string& op_token, const QueryTerm& left, const QueryTerm& right, const DocList& l_docs, const DocList& r_docs) const;

    QueryTerm parse_query_token(const std::string& token) const;
    static bool is_operator(const std::string& token);
    static bool is_term_like(const std::string& token);

    const Index& index_;
    Tokenizer tokenizer_;
};