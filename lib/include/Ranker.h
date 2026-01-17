#pragma once
#include "Index.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <iostream>

class Ranker {
public:
    explicit Ranker(const Index& index) : index_(index) {
        avgdl_ = index_.get_forward_index().get_avg_dl();
        total_docs_ = index_.get_forward_index().size();
        if (avgdl_ <= 0.0001) avgdl_ = 1.0;
        if (total_docs_ == 0) total_docs_ = 1;
    }

    double score(DocId doc_id, const std::vector<Term>& query_terms, double k1, double b, double w_title) const {
        double score = 0.0;
        if (doc_id >= total_docs_) return 0.0;

        double dl = index_.get_forward_index().get_doc_length(doc_id);

        for (const auto& term : query_terms) {
            const auto& inv_idx = index_.get_inverted_index();
            auto it = inv_idx.find(term);
            if (it == inv_idx.end()) continue;

            const auto& fields = it->second;
            
            // Защита: doc_freq не может быть больше total_docs
            double doc_freq = 0;
            for(const auto& [field, postings] : fields) {
                if (postings.docs.size() > doc_freq) doc_freq = postings.docs.size();
            }
            if (doc_freq >= total_docs_) doc_freq = total_docs_ - 0.5; // Cap it
            
            if (doc_freq == 0) continue;
            
            // Защита от NaN в логарифме
            double idf_val = (total_docs_ - doc_freq + 0.5) / (doc_freq + 0.5) + 1.0;
            double idf = (idf_val > 0) ? std::log(idf_val) : 0;
            
            double tf = 0;
            for (const auto& [field, postings] : fields) {
                if (postings.docs.empty()) continue;

                // Если список не отсортирован (битые данные), lower_bound опасен
                // Но мы предполагаем, что после фикса Index.cpp данные отсортированы
                auto it_doc = std::lower_bound(postings.docs.begin(), postings.docs.end(), doc_id);
                
                if (it_doc != postings.docs.end() && *it_doc == doc_id) {
                    size_t idx = std::distance(postings.docs.begin(), it_doc);
                    
                    // Абсолютная защита памяти
                    if (idx < postings.positions.size()) {
                        double field_tf = postings.positions[idx].size();
                        if (field == "title") field_tf *= w_title;
                        tf += field_tf;
                    }
                }
            }

            double num = tf * (k1 + 1);
            double den = tf + k1 * (1 - b + b * (dl / avgdl_));
            score += idf * (num / den);
        }
        return score;
    }

private:
    const Index& index_;
    double avgdl_;
    size_t total_docs_;
};