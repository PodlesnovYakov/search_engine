#pragma once
#include "Index.h"
#include <cmath>
#include <vector>
#include <algorithm>

class Ranker {
public:
    explicit Ranker(const Index& index) : index_(index) {
        avgdl_ = index_.get_forward_index().get_avg_dl();
        total_docs_ = index_.get_forward_index().size();
    }

    double score(DocId doc_id, const std::vector<Term>& query_terms) const {
        double score = 0.0;
        double k1 = 1.2;
        double b = 0.75;
        double dl = index_.get_forward_index().get_doc_length(doc_id);

        for (const auto& term : query_terms) {
            const auto& inv_idx = index_.get_inverted_index();
            auto it = inv_idx.find(term);
            if (it == inv_idx.end()) continue;

            // Считаем IDF (упрощенно по полю plot)
            double doc_freq = 0;
            const auto& fields = it->second;
            if (fields.count("plot")) doc_freq = fields.at("plot").docs.size();
            else if (!fields.empty()) doc_freq = fields.begin()->second.docs.size();
            
            if (doc_freq == 0) continue;
            double idf = std::log((total_docs_ - doc_freq + 0.5) / (doc_freq + 0.5) + 1.0);

            // Считаем TF (сколько раз слово в документе)
            double tf = 0;
            // Проверяем все поля
            for (const auto& [field, postings] : fields) {
                // Ищем документ в списке
                auto it_doc = std::lower_bound(postings.docs.begin(), postings.docs.end(), doc_id);
                if (it_doc != postings.docs.end() && *it_doc == doc_id) {
                    size_t idx = std::distance(postings.docs.begin(), it_doc);
                    tf += postings.positions[idx].size();
                    if (field == "title") tf += 2.0; // Бонус за тайтл
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