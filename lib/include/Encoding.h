#pragma once
#include <vector>
#include <cstdint>
#include <fstream>
#include <string>

// --- Varint (числа переменной длины) ---
inline void write_varint(std::ofstream& out, uint32_t value) {
    while (value >= 128) {
        out.put(static_cast<char>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    out.put(static_cast<char>(value));
}

inline uint32_t read_varint(std::ifstream& in) {
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

// --- Строки ---
inline void write_string(std::ofstream& out, const std::string& s) {
    write_varint(out, s.size());
    out.write(s.data(), s.size());
}

inline void read_string(std::ifstream& in, std::string& s) {
    size_t len = read_varint(in);
    s.resize(len);
    in.read(&s[0], len);
}

// --- Delta Encoding (для списков) ---
inline void write_delta_vector(std::ofstream& out, const std::vector<uint32_t>& vec) {
    write_varint(out, vec.size());
    uint32_t prev = 0;
    for (uint32_t val : vec) {
        write_varint(out, val - prev);
        prev = val;
    }
}

inline std::vector<uint32_t> read_delta_vector(std::ifstream& in) {
    size_t size = read_varint(in);
    std::vector<uint32_t> vec(size);
    uint32_t prev = 0;
    for (size_t i = 0; i < size; ++i) {
        uint32_t delta = read_varint(in);
        vec[i] = prev + delta;
        prev = vec[i];
    }
    return vec;
}