#include "bloom_filter.h"
#include <cstring>
#include <cmath>

BloomFilter::BloomFilter() : bitArray(WORD_COUNT) {
    for (size_t i = 0; i < WORD_COUNT; ++i) {
        bitArray[i] = 0;
    }
}

uint32_t BloomFilter::hash(uint32_t value, uint32_t seed) const {
    uint32_t h = seed;
    
    h ^= value;
    h *= 0x5bd1e995;
    h ^= h >> 15;
    h *= 0x27d4eb2d;
    h ^= h >> 15;
    
    return h % SIZE;
}

void BloomFilter::add(int v) {
    uint32_t value = static_cast<uint32_t>(v);
    
    for (size_t i = 0; i < HASH_COUNT; ++i) {
        uint32_t pos = hash(value, seeds[i]);
        auto [wordIdx, bitOffset] = getBitPosition(pos);
        
        uint64_t mask = 1ULL << bitOffset;
        uint64_t oldWord, newWord;
        
        do {
            oldWord = bitArray[wordIdx].load(std::memory_order_relaxed);
            newWord = oldWord | mask;
        } while (!bitArray[wordIdx].compare_exchange_weak(
            oldWord, newWord, 
            std::memory_order_release, 
            std::memory_order_relaxed));
    }
}

bool BloomFilter::contains(int v) const {
    uint32_t value = static_cast<uint32_t>(v);
    
    for (size_t i = 0; i < HASH_COUNT; ++i) {
        uint32_t pos = hash(value, seeds[i]);
        auto [wordIdx, bitOffset] = getBitPosition(pos);
        
        uint64_t word = bitArray[wordIdx].load(std::memory_order_acquire);
        uint64_t mask = 1ULL << bitOffset;
        
        if (!(word & mask)) {
            return false;
        }
    }
    
    return true;
}

void BloomFilter::print() const {
    std::cout << "Bloom Filter (size: " << SIZE << " bits, " 
              << HASH_COUNT << " hash functions)" << std::endl;
    
    size_t setBits = 0;
    for (size_t i = 0; i < WORD_COUNT; ++i) {
        uint64_t word = bitArray[i].load(std::memory_order_relaxed);
        setBits += __builtin_popcountll(word);
    }
    
    double fillRatio = static_cast<double>(setBits) / SIZE;
    double theoreticalFPP = std::pow(fillRatio, HASH_COUNT);
    
    std::cout << "Set bits: " << setBits << " / " << SIZE 
              << " (" << std::fixed << std::setprecision(4) 
              << fillRatio * 100 << "%)" << std::endl;
    std::cout << "Theoretical false positive probability: " 
              << std::setprecision(8) << theoreticalFPP << std::endl;
    
    std::cout << "First 64 bits: ";
    for (size_t i = 0; i < 64 && i < SIZE; ++i) {
        auto [wordIdx, bitOffset] = getBitPosition(i);
        uint64_t word = bitArray[wordIdx].load(std::memory_order_relaxed);
        std::cout << ((word & (1ULL << bitOffset)) ? '1' : '0');
    }
    std::cout << std::endl;
}
