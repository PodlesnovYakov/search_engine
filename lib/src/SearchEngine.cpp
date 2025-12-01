#include "SearchEngine.h"
#include <stack>
#include <stdexcept>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <map>
#include <set>

SearchEngine::SearchEngine(const Index& index) : index_(index), tokenizer_() {}

std::vector<uint32_t> SearchEngine::search(const std::string& query_str) const {
    if (query_str.empty()) return {};
    auto tokens = tokenize_query(query_str);
    if (tokens.empty()) return {};
    tokens = insert_implicit_and(tokens);
    auto rpn = to_rpn(tokens);
    return evaluate_rpn(rpn);
}

std::vector<std::string> SearchEngine::tokenize_query(const std::string& s) const {
    std::vector<std::string> tokens;
    std::string buf;
    for (char ch : s) {
        if (ch == '(' || ch == ')') {
            if (!buf.empty()) { tokens.push_back(buf); buf.clear(); }
            tokens.push_back(std::string(1, ch));
        } else if (std::isspace(ch)) {
            if (!buf.empty()) { tokens.push_back(buf); buf.clear(); }
        } else if (ch != '"') {
            buf += ch;
        }
    }
    if (!buf.empty()) tokens.push_back(buf);
    return tokens;
}

std::vector<std::string> SearchEngine::insert_implicit_and(const std::vector<std::string>& tokens) const {
    std::vector<std::string> result;
    for (size_t i = 0; i < tokens.size(); ++i) {
        result.push_back(tokens[i]);
        if (i + 1 < tokens.size()) {
            if (is_term_like(tokens[i]) && is_term_like(tokens[i + 1])) {
                result.push_back("AND");
            }
        }
    }
    return result;
}

std::vector<std::string> SearchEngine::to_rpn(const std::vector<std::string>& tokens) const {
    std::vector<std::string> rpn;
    std::stack<std::string> op_stack;
    const std::map<std::string, int> precedence = {{"OR", 1}, {"AND", 2}, {"NEAR", 3}, {"ADJ", 3}, {"NOT", 4}};

    auto get_prec = [&](const std::string& op) {
        std::string base_op = op.substr(0, op.find('/'));
        return precedence.count(base_op) ? precedence.at(base_op) : 0;
    };

    for (const auto& token : tokens) {
        std::string upper_token = token;
        std::transform(upper_token.begin(), upper_token.end(), upper_token.begin(), ::toupper);
        if (is_operator(upper_token)) {
            while (!op_stack.empty() && op_stack.top() != "(" && get_prec(op_stack.top()) >= get_prec(upper_token)) {
                rpn.push_back(op_stack.top()); op_stack.pop();
            }
            op_stack.push(upper_token);
        } else if (token == "(") {
            op_stack.push(token);
        } else if (token == ")") {
            while (!op_stack.empty() && op_stack.top() != "(") {
                rpn.push_back(op_stack.top()); op_stack.pop();
            }
            if (!op_stack.empty()) op_stack.pop();
        } else {
            rpn.push_back(token);
        }
    }
    while (!op_stack.empty()) {
        rpn.push_back(op_stack.top()); op_stack.pop();
    }
    return rpn;
}

std::vector<uint32_t> SearchEngine::evaluate_rpn(const std::vector<std::string>& rpn) const {
    std::stack<std::variant<std::string, std::vector<uint32_t>>> eval_stack;

    for (const auto& token : rpn) {
        if (is_operator(token)) {
            if (token == "NOT") {
                if (eval_stack.empty()) throw std::runtime_error("Invalid query: NOT needs an operand.");
                auto& top_var = eval_stack.top();
                std::vector<uint32_t> operand_docs;
                if (auto* term_str = std::get_if<std::string>(&top_var)) {
                    operand_docs = get_doc_ids(parse_query_token(*term_str));
                } else {
                    operand_docs = std::get<std::vector<uint32_t>>(top_var);
                }
                eval_stack.pop();
                eval_stack.push(execute_not(operand_docs));
            } else {
                if (eval_stack.size() < 2) throw std::runtime_error("Invalid query for binary operator.");
                auto right_var = eval_stack.top(); eval_stack.pop();
                auto left_var = eval_stack.top(); eval_stack.pop();

                if (token.rfind("NEAR", 0) == 0 || token.rfind("ADJ", 0) == 0) {
                    auto* left_str = std::get_if<std::string>(&left_var);
                    auto* right_str = std::get_if<std::string>(&right_var);
                    if (!left_str || !right_str) {
                        throw std::runtime_error("Proximity operators can only be used with simple terms.");
                    }
                    eval_stack.push(execute_prox(token, *left_str, *right_str));
                } else { 
                    std::vector<uint32_t> left_docs, right_docs;
                    if (auto* s = std::get_if<std::string>(&left_var)) left_docs = get_doc_ids(parse_query_token(*s));
                    else left_docs = std::get<std::vector<uint32_t>>(left_var);

                    if (auto* s = std::get_if<std::string>(&right_var)) right_docs = get_doc_ids(parse_query_token(*s));
                    else right_docs = std::get<std::vector<uint32_t>>(right_var);

                    if (token == "AND") {
                        eval_stack.push(execute_intersect(left_docs, right_docs));
                    } else if (token == "OR") {
                        eval_stack.push(execute_union(left_docs, right_docs));
                    }
                }
            }
        } else {
            eval_stack.push(token);
        }
    }

    if (eval_stack.empty()) return {};
    if (eval_stack.size() != 1) throw std::runtime_error("Invalid query structure.");
    
    if (auto* term_str = std::get_if<std::string>(&eval_stack.top())) {
        return get_doc_ids(parse_query_token(*term_str));
    }
    return std::get<std::vector<uint32_t>>(eval_stack.top());
}

SearchEngine::QueryTerm SearchEngine::parse_query_token(const std::string& token) const {
    std::string term_part = token;
    std::optional<std::string> field_part;

    size_t colon_pos = token.find(':');
    if (colon_pos != std::string::npos && colon_pos > 0) {
        field_part = token.substr(0, colon_pos);
        term_part = token.substr(colon_pos + 1);
    }

    auto processed_tokens = tokenizer_.tokenize(term_part);
    std::string final_term = processed_tokens.empty() ? "" : processed_tokens[0];
    return {final_term, field_part};
}

std::vector<uint32_t> SearchEngine::get_doc_ids(const QueryTerm& q_term) const {
    const auto& inv_index = index_.get_inverted_index();
    auto term_it = inv_index.find(q_term.term);
    if (term_it == inv_index.end()) return {};

    std::vector<uint32_t> doc_ids;
    
    if (q_term.field) {
        auto field_it = term_it->second.find(*q_term.field);
        if (field_it != term_it->second.end()) {
            auto current = field_it->second.get_head()->forward[0];
            while (current) {
                doc_ids.push_back(current->value);
                current = current->forward[0];
            }
        }
    } else {
        std::set<uint32_t> unique_ids;
        for (const auto& field_pair : term_it->second) {
            auto current = field_pair.second.get_head()->forward[0];
            while (current) {
                unique_ids.insert(current->value);
                current = current->forward[0];
            }
        }
        doc_ids.assign(unique_ids.begin(), unique_ids.end());
    }
    return doc_ids;
}

std::vector<uint32_t> SearchEngine::execute_intersect(const std::vector<uint32_t>& list1, const std::vector<uint32_t>& list2) const {
    std::vector<uint32_t> result;
    std::set_intersection(list1.begin(), list1.end(), list2.begin(), list2.end(), std::back_inserter(result));
    return result;
}

std::vector<uint32_t> SearchEngine::execute_union(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) const {
    std::vector<uint32_t> result;
    std::set_union(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(result));
    return result;
}

std::vector<uint32_t> SearchEngine::execute_not(const std::vector<uint32_t>& operand) const {
    std::vector<uint32_t> universe, result;
    for(const auto& pair : index_.get_documents()) {
        universe.push_back(pair.first);
    }
    std::sort(universe.begin(), universe.end());
    std::set_difference(universe.begin(), universe.end(), operand.begin(), operand.end(), std::back_inserter(result));
    return result;
}

std::vector<uint32_t> SearchEngine::execute_prox(const std::string& op_token, const std::string& left_token, const std::string& right_token) const {
    QueryTerm left_term = parse_query_token(left_token);
    QueryTerm right_term = parse_query_token(right_token);

    if (left_term.field != right_term.field) return {};

    const auto& pos_index = index_.get_positional_index();
    auto left_it = pos_index.find(left_term.term);
    auto right_it = pos_index.find(right_term.term);

    if (left_it == pos_index.end() || right_it == pos_index.end()) return {};

    size_t slash_pos = op_token.find('/');
    if (slash_pos == std::string::npos) return {}; 
    int distance = std::stoi(op_token.substr(slash_pos + 1));
    bool is_ordered = op_token.rfind("ADJ", 0) == 0;

    std::vector<uint32_t> result_docs;

    std::vector<uint32_t> left_docs = get_doc_ids(left_term);
    std::vector<uint32_t> right_docs = get_doc_ids(right_term);
    std::vector<uint32_t> common_docs;
    std::set_intersection(left_docs.begin(), left_docs.end(), right_docs.begin(), right_docs.end(), std::back_inserter(common_docs));

    for (uint32_t doc_id : common_docs) {
        const auto& left_postings = left_it->second;
        const auto& right_postings = right_it->second;

        std::string field_to_check = left_term.field ? *left_term.field : left_postings.begin()->first;

        auto l_field_it = left_postings.find(field_to_check);
        auto r_field_it = right_postings.find(field_to_check);

        if (l_field_it != left_postings.end() && r_field_it != right_postings.end() &&
            l_field_it->second.count(doc_id) && r_field_it->second.count(doc_id)) {
            
            const auto& pos1 = l_field_it->second.at(doc_id);
            const auto& pos2 = r_field_it->second.at(doc_id);
            bool found = false;

            for (int p1 : pos1) {
                for (int p2 : pos2) {
                    int diff = p2 - p1;
                    if (is_ordered) {
                        if (diff > 0 && diff <= distance) { found = true; break; }
                    } else {
                        if (std::abs(diff) <= distance && diff != 0) { found = true; break; }
                    }
                }
                if (found) break;
            }
            if (found) result_docs.push_back(doc_id);
        }
    }
    return result_docs;
}

bool SearchEngine::is_operator(const std::string& token) {
    return token == "AND" || token == "OR" || token == "NOT" || token.rfind("NEAR", 0) == 0 || token.rfind("ADJ", 0) == 0;
}

bool SearchEngine::is_term_like(const std::string& token) {
    return !is_operator(token) && token != "(" && token != ")";
}