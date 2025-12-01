#pragma once
#include <vector>
#include <random>
#include <limits>
#include <memory>

struct SkipNode {
    uint32_t value; // doc_id 
    std::vector<std::shared_ptr<SkipNode>> forward; // Вектор указателей на следующие узлы на разных уровнях

    SkipNode(uint32_t val, int level) : value(val), forward(level + 1, nullptr) {}
};

class SkipList {
public:
    SkipList(int max_level = 16) 
        : max_level_(max_level), current_level_(0) {
        header_ = std::make_shared<SkipNode>(0, max_level_);
    }

    void insert(uint32_t value) {
        std::vector<std::shared_ptr<SkipNode>> update(max_level_ + 1, nullptr);
        auto current = header_;

        // 1. Находим правильное место для вставки на всех уровнях
        for (int i = current_level_; i >= 0; i--) {
            while (current->forward[i] && current->forward[i]->value < value) {
                current = current->forward[i];
            }
            update[i] = current;
        }

        current = current->forward[0];

        // Если элемент уже существует, ничего не делаем
        if (current && current->value == value) {
            return;
        }

        // 2. Генерируем случайный уровень для нового узла
        int new_level = random_level();

        if (new_level > current_level_) {
            for (int i = current_level_ + 1; i <= new_level; i++) {
                update[i] = header_;
            }
            current_level_ = new_level;
        }

        // 3. Создаем новый узел и встраиваем его в списки
        auto new_node = std::make_shared<SkipNode>(value, new_level);
        for (int i = 0; i <= new_level; i++) {
            new_node->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = new_node;
        }
    }

    // Получаем указатель на начало списка для итерации
    std::shared_ptr<const SkipNode> get_head() const {
        return header_;
    }

private:
    int max_level_;
    int current_level_;
    std::shared_ptr<SkipNode> header_;

    int random_level() {
        int level = 0;
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<> distrib(1, 2);
        while (distrib(gen) == 1 && level < max_level_) {
            level++;
        }
        return level;
    }
};
/*
[15, 8, 25, 3, 18]
3 → уровень 2
8 → уровень 0
15 → уровень 1
18 → уровень 0
25 → уровень 3
Уровень 0: [header] → null
Уровень 1: [header] ────────→ [15] → null
Уровень 0: [header] ────────→ [15] → null
Находим позицию: 8 < 15, поэтому update[0] = header, update[1] = header
Создаем узел 8 с уровнем 0
Вставляем только на уровень 0:
Уровень 1: [header] ────────→ [15] → null
Уровень 0: [header] → [8] → [15] → null
Уровень 3: [header] ──────────────────────────────────────→ [25] → null
             │
Уровень 2: [header] → [3] ────────────────────────────────→ [25] → null
             │         │
Уровень 1: [header] → [3] ────────────→ [15] ─────────────→ [25] → null
             │         │                 │
Уровень 0: [header] → [3] → [8] → [15] → [18] → [25] → null

// Уровень 3: header → 25 (25 > 18) → спускаемся на уровень 2
// Уровень 2: header → 3 → 25 (3 < 18 < 25) → спускаемся на уровень 1  
// Уровень 1: 3 → 15 → 25 (15 < 18 < 25) → спускаемся на уровень 0
// Уровень 0: 15 → 18 (найден!) → возвращаем узел 18
*/