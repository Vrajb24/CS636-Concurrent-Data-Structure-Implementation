#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <atomic>
#include <vector>
#include <cstdint>
#include <iostream>
#include <iomanip>

class BloomFilter {
private:
    static const size_t SIZE = 1 << 24;
    static const size_t HASH_COUNT = 3;
    static const size_t BITS_PER_WORD = 64;
    static const size_t WORD_COUNT = (SIZE + BITS_PER_WORD - 1) / BITS_PER_WORD;
    
    std::vector<std::atomic<uint64_t>> bitArray;
    
    const uint32_t seeds[HASH_COUNT] = {0x1234ABCD, 0xF0F0F0F0, 0xAAAA5555};
    
    uint32_t hash(uint32_t value, uint32_t seed) const;
    
    std::pair<size_t, size_t> getBitPosition(size_t bitIndex) const {
        return {bitIndex / BITS_PER_WORD, bitIndex % BITS_PER_WORD};
    }
    
public:
    BloomFilter();
    
    void add(int v);
    bool contains(int v) const;
    
    void print() const;
};

#endif
