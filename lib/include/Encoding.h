#pragma once
#include <vector>
#include <cstdint>
#include <fstream>
#include <string>
#include <stdexcept>
#include <iostream>

const size_t MAX_BLOCK_SIZE = 200 * 1024 * 1024; 

inline void write_varint(std::ofstream& out, uint64_t value) {
    while (value >= 128) {
        out.put(static_cast<char>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    out.put(static_cast<char>(value));
}

inline uint64_t read_varint(std::ifstream& in) {
    uint64_t value = 0;
    int shift = 0;
    char byte;
    while (in.get(byte)) {
        value |= (static_cast<uint64_t>(byte & 0x7F) << shift);
        if ((byte & 0x80) == 0) break;
        shift += 7;
        if (shift > 63) throw std::runtime_error("Varint corrupted");
    }
    return value;
}

inline void write_string(std::ofstream& out, const std::string& s) {
    write_varint(out, s.size());
    out.write(s.data(), s.size());
}

inline void read_string(std::ifstream& in, std::string& s) {
    size_t len = read_varint(in);
    if (len > MAX_BLOCK_SIZE) throw std::runtime_error("String too long");
    s.resize(len);
    if (len > 0) in.read(&s[0], len);
}

inline void write_delta_vector(std::ofstream& out, const std::vector<uint32_t>& vec) {
    write_varint(out, vec.size());
    uint64_t prev = 0;
    for (uint32_t val : vec) {
        if (val < prev) {
        }
        write_varint(out, static_cast<uint64_t>(val - prev));
        prev = val;
    }
}

inline std::vector<uint32_t> read_delta_vector(std::ifstream& in) {
    size_t size = read_varint(in);
    if (size > MAX_BLOCK_SIZE) throw std::runtime_error("Vector too large");
    
    std::vector<uint32_t> vec;
    vec.reserve(size);
    uint64_t prev = 0;
    for (size_t i = 0; i < size; ++i) {
        uint64_t delta = read_varint(in);
        uint64_t val = prev + delta;
        vec.push_back(static_cast<uint32_t>(val));
        prev = val;
    }
    return vec;
}