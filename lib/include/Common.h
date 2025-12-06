#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <fstream>

using DocId = uint32_t;
using DocList = std::vector<DocId>;
using Term = std::string;
using Tokens = std::vector<Term>;
using Positions = std::vector<uint32_t>;


inline void encode_varint(std::ofstream& out, uint32_t value) {
    while (value >= 128) {
        out.put(static_cast<char>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    out.put(static_cast<char>(value));
}

inline uint32_t decode_varint(std::ifstream& in) {
    uint32_t value = 0;
    int shift = 0;
    char byte;
    while (in.get(byte)) {
        value |= (static_cast<uint32_t>(byte & 0x7F) << shift);
        if ((byte & 0x80) == 0) break;
        shift += 7;
    }
    return value;
}