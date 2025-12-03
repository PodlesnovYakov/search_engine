#include "SearchEngine.h"
#include "Ranker.h"
#include <stack>
#include <stdexcept>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <cmath>
#include <map> // <--- ДОБАВЛЕНО (Лечит ошибку 'map' does not name a type)
#include <iostream>

SearchEngine::SearchEngine(const Index& index) : index_(index), tokenizer_() {}

DocList SearchEngine::search(const std::string& query_str) const {
    if (query_str.empty()) return {};
    auto tokens = tokenize_query(query_str);
    if (tokens.empty()) return {};
    
    // Склейка полей (title + : + king -> title:king)
    Tokens processed;
    for(size_t i = 0; i < tokens.size(); ++i) {
        if (i + 1 < tokens.size() && tokens[i+1] == ":") {
            processed.push_back(tokens[i] + tokens[i+1] + (i + 2 < tokens.size() ? tokens[i+2] : ""));
            i += 2; 
        } else {
            processed.push_back(tokens[i]);
        }
    }

    processed = insert_implicit_and(processed);
    
    // 1. Получаем кандидатов
    DocList results = evaluate_rpn(to_rpn(processed));

    // 2. РАНЖИРОВАНИЕ BM25
    if (results.empty()) return {};
    
    Tokens scoring_terms;
    for(const auto& t : tokens) {
        if (is_term_like(t)) {
            auto qt = parse_query_token(t);
            if (!qt.term.empty()) scoring_terms.push_back(qt.term);
        }
    }

    Ranker ranker(index_);
    std::sort(results.begin(), results.end(), [&](DocId a, DocId b) {
        return ranker.score(a, scoring_terms) > ranker.score(b, scoring_terms);
    });
    
    return results;
}

Tokens SearchEngine::tokenize_query(const std::string& s) const {
    Tokens tokens;
    std::string buf;
    for (char ch : s) {
        // Добавил ':' в разделители
        if (ch == '(' || ch == ')' || ch == ':' || std::isspace(ch)) {
            if (!buf.empty()) { tokens.push_back(buf); buf.clear(); }
            if (!std::isspace(ch)) { tokens.push_back(std::string(1, ch)); }
        } else if (ch != '"') {
            buf += ch;
        }
    }
    if (!buf.empty()) tokens.push_back(buf);
    return tokens;
}

Tokens SearchEngine::insert_implicit_and(const Tokens& tokens) const {
    Tokens result;
    for (size_t i = 0; i < tokens.size(); ++i) {
        result.push_back(tokens[i]);
        if (i + 1 < tokens.size()) {
            if (is_term_like(tokens[i]) && is_term_like(tokens[i+1]) && tokens[i].back() != ':') {
                result.push_back("AND");
            }
        }
    }
    return result;
}

Tokens SearchEngine::to_rpn(const Tokens& tokens) const {
    Tokens rpn;
    std::stack<std::string> op_stack;
    const std::map<std::string, int> precedence = {{"OR", 1}, {"AND", 2}, {"NEAR", 3}, {"ADJ", 3}, {"NOT", 4}};
    auto get_prec = [&](const std::string& op) { return precedence.count(op.substr(0, op.find('/'))) ? precedence.at(op.substr(0, op.find('/'))) : 0; };

    for (const auto& token : tokens) {
        std::string upper = token;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        if (is_operator(upper)) {
            while (!op_stack.empty() && op_stack.top() != "(" && get_prec(op_stack.top()) >= get_prec(upper)) {
                rpn.push_back(op_stack.top()); op_stack.pop();
            }
            op_stack.push(upper);
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
    while (!op_stack.empty()) { rpn.push_back(op_stack.top()); op_stack.pop(); }
    return rpn;
}

struct StackItem {
    DocList calculated; 
    const PostingsList* raw = nullptr;
    std::optional<SearchEngine::QueryTerm> origin_term = std::nullopt;

    const DocList& get_vector() const {
        if (raw) return raw->docs;
        return calculated;
    }
};

DocList SearchEngine::evaluate_rpn(const Tokens& rpn) const {
    std::stack<StackItem> eval_stack;

    for (const auto& token : rpn) {
        if (is_operator(token)) {
            if (token == "NOT") {
                auto op = eval_stack.top(); eval_stack.pop();
                eval_stack.push({execute_not(op.get_vector()), nullptr, std::nullopt});
            } else {
                auto right = eval_stack.top(); eval_stack.pop();
                auto left = eval_stack.top(); eval_stack.pop();

                if (token == "AND") {
                    DocList res;
                    if (left.raw && right.raw) res = execute_intersect(left.raw, right.raw);
                    else if (left.raw) res = execute_intersect_vec(right.calculated, left.raw);
                    else if (right.raw) res = execute_intersect_vec(left.calculated, right.raw);
                    else res = execute_intersect_vec_vec(left.get_vector(), right.get_vector());
                    eval_stack.push({std::move(res), nullptr, std::nullopt});
                } else if (token == "OR") {
                    eval_stack.push({execute_union(left.get_vector(), right.get_vector()), nullptr, std::nullopt});
                }
                else if (token.rfind("NEAR", 0) == 0 || token.rfind("ADJ", 0) == 0) {
                    DocList res;
                    if (left.origin_term && right.origin_term) {
                        res = execute_prox(token, *left.origin_term, *right.origin_term, left.get_vector(), right.get_vector());
                    } else {
                        res = execute_intersect_vec_vec(left.get_vector(), right.get_vector());
                    }
                    eval_stack.push({std::move(res), nullptr, std::nullopt});
                }
            }
        } else {
            auto q_term = parse_query_token(token);
            if (!q_term.term.empty()) {
                const PostingsList* pl = get_postings(q_term);
                if (pl && q_term.field) eval_stack.push({{}, pl, q_term});
                else eval_stack.push({get_doc_ids(q_term), nullptr, q_term});
            } else {
                eval_stack.push({{}, nullptr, std::nullopt});
            }
        }
    }
    if (eval_stack.empty()) return {};
    return eval_stack.top().get_vector();
}

SearchEngine::QueryTerm SearchEngine::parse_query_token(const std::string& token) const {
    std::string term = token;
    std::optional<std::string> field;
    size_t pos = token.find(':');
    if (pos != std::string::npos && pos > 0) {
        field = token.substr(0, pos);
        term = token.substr(pos + 1);
    }
    auto toks = tokenizer_.tokenize(term);
    return {toks.empty() ? "" : toks[0], field};
}

const PostingsList* SearchEngine::get_postings(const QueryTerm& q_term) const {
    const auto& idx = index_.get_inverted_index();
    auto it = idx.find(q_term.term);
    if (it == idx.end()) return nullptr;
    if (q_term.field) {
        auto fit = it->second.find(*q_term.field);
        return (fit != it->second.end()) ? &fit->second : nullptr;
    }
    return nullptr;
}

std::vector<uint32_t> SearchEngine::get_doc_ids(const QueryTerm& q_term) const {
    const auto& inv_index = index_.get_inverted_index();
    auto term_it = inv_index.find(q_term.term);
    if (term_it == inv_index.end()) return {};

    if (q_term.field) {
        auto field_it = term_it->second.find(*q_term.field);
        if (field_it != term_it->second.end()) return field_it->second.docs;
    } else {
        DocList result;
        for (const auto& [field, postings] : term_it->second) {
            DocList temp;
            temp.reserve(result.size() + postings.docs.size());
            std::set_union(result.begin(), result.end(), 
                           postings.docs.begin(), postings.docs.end(), 
                           std::back_inserter(temp));
            result = std::move(temp);
        }
        return result;
    }
    return {};
}

DocList SearchEngine::execute_prox(const std::string& op_token, const QueryTerm& left_term, const QueryTerm& right_term, const DocList& l_docs, const DocList& r_docs) const {
    size_t slash = op_token.find('/');
    int dist = 1;
    if (slash != std::string::npos) try { dist = std::stoi(op_token.substr(slash + 1)); } catch(...) {}
    bool ordered = (op_token.find("ADJ") == 0);

    const auto& idx = index_.get_inverted_index();
    if (!idx.count(left_term.term) || !idx.count(right_term.term)) return {};

    const auto& l_fields = idx.at(left_term.term);
    const auto& r_fields = idx.at(right_term.term);

    DocList result;
    DocList common_docs = execute_intersect_vec_vec(l_docs, r_docs);

    for (DocId doc_id : common_docs) {
        bool match = false;
        std::vector<std::string> fields = {"title", "plot"};
        if (left_term.field) fields = {*left_term.field};

        for (const auto& field : fields) {
            if (match) break;
            if (right_term.field && *right_term.field != field) continue;
            if (!l_fields.count(field) || !r_fields.count(field)) continue;

            const auto& pl_l = l_fields.at(field);
            const auto& pl_r = r_fields.at(field);

            auto it_l = std::lower_bound(pl_l.docs.begin(), pl_l.docs.end(), doc_id);
            auto it_r = std::lower_bound(pl_r.docs.begin(), pl_r.docs.end(), doc_id);

            if (it_l == pl_l.docs.end() || *it_l != doc_id) continue;
            if (it_r == pl_r.docs.end() || *it_r != doc_id) continue;

            size_t idx_l = std::distance(pl_l.docs.begin(), it_l);
            size_t idx_r = std::distance(pl_r.docs.begin(), it_r);

            const auto& pos_l = pl_l.positions[idx_l];
            const auto& pos_r = pl_r.positions[idx_r];

            auto pl = pos_l.begin();
            auto pr = pos_r.begin();
            while (pl != pos_l.end() && pr != pos_r.end()) {
                long long p1 = *pl;
                long long p2 = *pr;
                long long diff = p2 - p1;
                if (ordered) {
                    if (diff > 0 && diff <= dist) { match = true; break; }
                    if (p2 <= p1) ++pr; else ++pl;
                } else {
                    if (std::abs(diff) <= dist) { match = true; break; }
                    if (p1 < p2) ++pl; else ++pr;
                }
            }
        }
        if (match) result.push_back(doc_id);
    }
    return result;
}

DocList SearchEngine::execute_intersect(const PostingsList* p1, const PostingsList* p2) const {
    if (!p1 || !p2) return {};
    DocList result;
    if (p1->docs.size() > p2->docs.size()) std::swap(p1, p2);

    const auto& small = p1->docs;
    const auto& large = p2->docs;
    const auto& skips = p2->skips;
    size_t step = p2->skip_step;

    size_t i = 0, j = 0;
    while (i < small.size() && j < large.size()) {
        if (small[i] == large[j]) {
            result.push_back(small[i]); i++; j++;
        } else if (small[i] < large[j]) {
            i++;
        } else {
            if (step > 0 && !skips.empty()) {
                size_t skip_idx = j / step;
                while (skip_idx < skips.size() && large[skips[skip_idx]] <= small[i]) {
                    j = skips[skip_idx]; skip_idx++;
                }
            }
            while (j < large.size() && large[j] < small[i]) j++;
        }
    }
    return result;
}

DocList SearchEngine::execute_intersect_vec(const DocList& small, const PostingsList* large) const {
    if (!large) return {};
    return execute_intersect_vec_vec(small, large->docs);
}

DocList SearchEngine::execute_intersect_vec_vec(const DocList& l1, const DocList& l2) const {
    DocList result;
    std::set_intersection(l1.begin(), l1.end(), l2.begin(), l2.end(), std::back_inserter(result));
    return result;
}

DocList SearchEngine::execute_union(const DocList& a, const DocList& b) const {
    DocList result;
    std::set_union(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(result));
    return result;
}

// --- ИСПРАВЛЕННАЯ ФУНКЦИЯ EXECUTE_NOT ---
DocList SearchEngine::execute_not(const DocList& operand) const {
    DocList result;
    size_t total_docs = index_.get_forward_index().size(); // Используем ForwardIndex
    
    // Перебираем все возможные ID документов (они идут от 0 до total_docs-1)
    for (DocId id = 0; id < total_docs; ++id) {
        if (!std::binary_search(operand.begin(), operand.end(), id)) {
            result.push_back(id);
        }
    }
    return result;
}
// ----------------------------------------

bool SearchEngine::is_operator(const std::string& token) {
    return token == "AND" || token == "OR" || token == "NOT" || token.rfind("NEAR", 0) == 0 || token.rfind("ADJ", 0) == 0;
}
bool SearchEngine::is_term_like(const std::string& token) {
    return !is_operator(token) && token != "(" && token != ")" && token.back() != ':';
}