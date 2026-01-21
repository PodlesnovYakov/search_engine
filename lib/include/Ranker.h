#pragma once
#include "Index.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <iostream>
#include <limits>

class Ranker {
public:
    explicit Ranker(const Index& index) : index_(index) {
        avgdl_ = index_.get_forward_index().get_avg_dl();
        total_docs_ = index_.get_forward_index().size();
        if (avgdl_ <= 0.0001) avgdl_ = 1.0;
        if (total_docs_ == 0) total_docs_ = 1;
    }
    double score(DocId doc_id, const std::vector<Term>& query_terms, double k1, double b, double w_title, double w_prox) const {
        double total_score = 0.0;
        if (doc_id >= total_docs_) return 0.0;

        double dl = index_.get_forward_index().get_doc_length(doc_id);
        std::vector<std::vector<uint32_t>> terms_positions_in_doc;
        terms_positions_in_doc.reserve(query_terms.size());
        for (const auto& term : query_terms) {
            const auto& inv_idx = index_.get_inverted_index();
            auto it = inv_idx.find(term);
            if (it == inv_idx.end()) {
                terms_positions_in_doc.push_back({});
                continue;
            }

            const auto& fields = it->second;
            
            double doc_freq = 0;
            for(const auto& [field, postings] : fields) {
                if (postings.docs.size() > doc_freq) doc_freq = postings.docs.size();
            }
            if (doc_freq >= total_docs_) doc_freq = total_docs_ - 0.5;
            
            double idf_val = (total_docs_ - doc_freq + 0.5) / (doc_freq + 0.5) + 1.0;
            double idf = (idf_val > 0) ? std::log(idf_val) : 0;
            
            double tf = 0;
            std::vector<uint32_t> current_term_positions;

            for (const auto& [field, postings] : fields) {
                if (postings.docs.empty()) continue;

                auto it_doc = std::lower_bound(postings.docs.begin(), postings.docs.end(), doc_id);
                if (it_doc != postings.docs.end() && *it_doc == doc_id) {
                    size_t idx = std::distance(postings.docs.begin(), it_doc);
                    if (idx < postings.positions.size()) {
                        double field_tf = postings.positions[idx].size();
                        if (field == "title") field_tf *= w_title;
                        tf += field_tf;
                        const auto& pos_vec = postings.positions[idx];
                        current_term_positions.insert(current_term_positions.end(), pos_vec.begin(), pos_vec.end());
                    }
                }
            }
            std::sort(current_term_positions.begin(), current_term_positions.end());
            terms_positions_in_doc.push_back(std::move(current_term_positions));

            double num = tf * (k1 + 1);
            double den = tf + k1 * (1 - b + b * (dl / avgdl_));
            double term_score = idf * (num / den);
            
            if (std::isfinite(term_score)) {
                total_score += term_score;
            }
        }
        if (w_prox > 0 && query_terms.size() > 1) {
            double prox_score = 0.0;
            for (size_t i = 0; i < terms_positions_in_doc.size() - 1; ++i) {
                const auto& pos1 = terms_positions_in_doc[i];
                const auto& pos2 = terms_positions_in_doc[i+1];
                
                if (pos1.empty() || pos2.empty()) continue;
                int min_dist = 1000000;
                
                for (uint32_t p1 : pos1) {
                    auto it = std::lower_bound(pos2.begin(), pos2.end(), p1);
                    
                    if (it != pos2.end()) {
                        int dist = static_cast<int>(*it) - static_cast<int>(p1);
                        if (dist < min_dist) min_dist = dist;
                    }
                    if (it != pos2.begin()) {
                        int dist = static_cast<int>(p1) - static_cast<int>(*(it - 1));
                        if (dist < min_dist) min_dist = dist;
                    }
                }
                if (min_dist > 0) {
                    prox_score += (1.0 / (static_cast<double>(min_dist) * min_dist)); 
                }
            }
            total_score += (prox_score * w_prox);
        }

        return total_score;
    }

private:
    const Index& index_;
    double avgdl_;
    size_t total_docs_;
};