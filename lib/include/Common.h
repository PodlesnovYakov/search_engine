#pragma once
#include <vector>
#include <string>
#include <cstdint>

// Выполняем требование: using для сокращения
using DocId = uint32_t;
using DocList = std::vector<DocId>;
using Term = std::string;
using Tokens = std::vector<Term>;
using Positions = std::vector<uint32_t>;