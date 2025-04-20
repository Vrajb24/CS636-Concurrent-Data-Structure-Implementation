#include "hash_table.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <random>
#include <functional>

std::vector<uint32_t> read_binary_file(const std::string& filename, size_t limit = 0) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return {};
    }
    
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    size_t numInts = fileSize / sizeof(uint32_t);
    if (limit > 0 && limit < numInts) {
        numInts = limit;
    }
    
    std::vector<uint32_t> data(numInts);
    file.read(reinterpret_cast<char*>(data.data()), numInts * sizeof(uint32_t));
    
    return data;
}

void run_benchmark(HashTableInterface* ht, int num_threads, const std::vector<size_t>& input_sizes) {
    std::cout << "\n========= Benchmark ==========" << std::endl;
    std::cout << "Implementation: " << 
    #ifdef USE_TBB
        "Intel TBB"
    #else
        "Pthread"
    #endif
        << " with " << num_threads << " threads" << std::endl;
    
    std::vector<uint32_t> insert_keys = read_binary_file("bin/random_keys_insert.bin");
    std::vector<uint32_t> insert_values = read_binary_file("bin/random_values_insert.bin");
    std::vector<uint32_t> delete_keys = read_binary_file("bin/random_keys_delete.bin");
    std::vector<uint32_t> search_keys = read_binary_file("bin/random_keys_search.bin");
        
    if (insert_keys.empty() || insert_values.empty() || delete_keys.empty() || search_keys.empty()) {
        std::cerr << "Failed to load test data. Using randomly generated data instead." << std::endl;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dist(1, std::numeric_limits<uint32_t>::max());
        
        size_t max_size = *std::max_element(input_sizes.begin(), input_sizes.end());
        
        insert_keys.resize(max_size);
        insert_values.resize(max_size);
        delete_keys.resize(max_size);
        search_keys.resize(max_size);
        
        for (size_t i = 0; i < max_size; ++i) {
            insert_keys[i] = dist(gen);
            insert_values[i] = dist(gen);
            delete_keys[i] = dist(gen);
            search_keys[i] = dist(gen);
        }
    }
    
    std::cout << "\n| Input Size | Operation | Time (ms) | Throughput (ops/sec) |" << std::endl;
    std::cout << "|------------|-----------|-----------|----------------------|" << std::endl;
    
    for (size_t n : input_sizes) {
        if (n > insert_keys.size()) {
            std::cerr << "Warning: Requested input size " << n 
                      << " exceeds available data size " << insert_keys.size() << std::endl;
            n = insert_keys.size();
        }
        
        std::vector<uint8_t> insert_results(n, 0);
        std::vector<uint32_t> lookup_results(n, 0);
        std::vector<uint8_t> delete_results(n, 0);
        
        auto start = std::chrono::high_resolution_clock::now();
        ht->batch_insert(insert_keys.data(), insert_values.data(), n, insert_results.data(), num_threads);
        auto end = std::chrono::high_resolution_clock::now();
        auto insert_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        double insert_throughput = (insert_time_ms > 0) ? (n * 1000.0 / insert_time_ms) : 0;
        
        std::cout << "| " << std::setw(10) << n << " | Insert    | " 
                  << std::setw(9) << insert_time_ms << " | " 
                  << std::setw(20) << std::fixed << std::setprecision(2) << insert_throughput << " |" << std::endl;
        
        start = std::chrono::high_resolution_clock::now();
        ht->batch_lookup(search_keys.data(), n, lookup_results.data(), num_threads);
        end = std::chrono::high_resolution_clock::now();
        auto lookup_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        double lookup_throughput = (lookup_time_ms > 0) ? (n * 1000.0 / lookup_time_ms) : 0;
        
        std::cout << "| " << std::setw(10) << n << " | Lookup    | " 
                  << std::setw(9) << lookup_time_ms << " | " 
                  << std::setw(20) << std::fixed << std::setprecision(2) << lookup_throughput << " |" << std::endl;
        
        start = std::chrono::high_resolution_clock::now();
        ht->batch_delete(delete_keys.data(), n, delete_results.data(), num_threads);
        end = std::chrono::high_resolution_clock::now();
        auto delete_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        double delete_throughput = (delete_time_ms > 0) ? (n * 1000.0 / delete_time_ms) : 0;
        
        std::cout << "| " << std::setw(10) << n << " | Delete    | " 
                  << std::setw(9) << delete_time_ms << " | " 
                  << std::setw(20) << std::fixed << std::setprecision(2) << delete_throughput << " |" << std::endl;
    }
}

void test1(HashTableInterface* ht) {
    std::cout << "\n========= Test 1: Basic Operations ==========" << std::endl;
    
    const size_t n = 10;
    uint32_t keys[n] = {1, 5, 3, 7, 10, 15, 13, 20, 25, 30};
    uint32_t vals[n] = {100, 500, 300, 700, 1000, 1500, 1300, 2000, 2500, 3000};
    uint8_t insertResults[n] = {0};
    
    ht->batch_insert(keys, vals, n, insertResults, 1);
    
    bool insertCorrect = true;
    for (size_t i = 0; i < n; i++) {
        std::cout << "Key " << keys[i] << " insert result: " << (insertResults[i] ? "Success" : "Failed") << std::endl;
        if (!insertResults[i]) insertCorrect = false;
    }
    
    uint32_t lookupResults[n] = {0};
    ht->batch_lookup(keys, n, lookupResults, 1);
    
    bool lookupCorrect = true;
    for (size_t i = 0; i < n; i++) {
        std::cout << "Lookup key " << keys[i] << " got value: " << lookupResults[i] << std::endl;
        if (lookupResults[i] != vals[i]) lookupCorrect = false;
    }
    
    uint8_t deleteResults[n] = {0};
    ht->batch_delete(keys, n, deleteResults, 1);
    
    bool deleteCorrect = true;
    for (size_t i = 0; i < n; i++) {
        std::cout << "Delete key " << keys[i] << " result: " << (deleteResults[i] ? "Success" : "Failed") << std::endl;
        if (!deleteResults[i]) deleteCorrect = false;
    }
    
    std::cout << "\nTest 1 Result:" << std::endl;
    std::cout << "Insert operations: " << (insertCorrect ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Lookup operations: " << (lookupCorrect ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Delete operations: " << (deleteCorrect ? "PASSED" : "FAILED") << std::endl;
}

void test2(HashTableInterface* ht) {
    std::cout << "\n========= Test 2: Duplicate Handling ==========" << std::endl;
    
    const size_t n = 5;
    uint32_t keys[n] = {42, 42, 42, 42, 42};
    uint32_t vals[n] = {100, 200, 300, 400, 500};
    uint8_t insertResults[n] = {0};
    
    ht->batch_insert(keys, vals, n, insertResults, 1);
    
    bool dupeHandlingCorrect = true;
    for (size_t i = 0; i < n; i++) {
        std::cout << "Key " << keys[i] << " (value " << vals[i] << ") insert result: " 
                  << (insertResults[i] ? "Success" : "Failed") << std::endl;
        
        if ((i == 0 && !insertResults[i]) || (i > 0 && insertResults[i])) {
            dupeHandlingCorrect = false;
        }
    }
    
    uint32_t lookupResult = 0;
    ht->batch_lookup(keys, 1, &lookupResult, 1);
    std::cout << "Lookup key " << keys[0] << " got value: " << lookupResult << std::endl;
    
    uint8_t deleteResult = 0;
    ht->batch_delete(keys, 1, &deleteResult, 1);
    std::cout << "Delete key " << keys[0] << " result: " << (deleteResult ? "Success" : "Failed") << std::endl;
    
    std::cout << "\nTest 2 Result:" << std::endl;
    std::cout << "Duplicate handling: " << (dupeHandlingCorrect ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Expected lookup value 100, got: " << lookupResult << " - " 
              << (lookupResult == 100 ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Delete success: " << (deleteResult ? "PASSED" : "FAILED") << std::endl;
}

void test3(HashTableInterface* ht) {
    std::cout << "\n========= Test 3: Concurrent Operations ==========" << std::endl;
    
    const size_t n = 1000;
    std::vector<uint32_t> keys(n);
    std::vector<uint32_t> vals(n);
    
    for (size_t i = 0; i < n; i++) {
        keys[i] = i + 1;
        vals[i] = (i + 1) * 100;
    }
    
    std::vector<uint8_t> insertResults(n, false);
    ht->batch_insert(keys.data(), vals.data(), n, insertResults.data(), 4);
    
    size_t successCount = std::count(insertResults.begin(), insertResults.end(), true);
    std::cout << "Insert success rate: " << successCount << "/" << n << std::endl;
    
    std::vector<uint32_t> lookupResults(n, 0);
    ht->batch_lookup(keys.data(), n, lookupResults.data(), 4);
    
    size_t correctLookups = 0;
    for (size_t i = 0; i < n; i++) {
        if (insertResults[i]) {
            if (lookupResults[i] == vals[i]) {
                correctLookups++;
            }
        } else {
            if (lookupResults[i] == 0) {
                correctLookups++;
            }
        }
    }
    
    std::cout << "Lookup consistency: " << correctLookups << "/" << n << std::endl;
    
    std::vector<uint8_t> deleteResults(n/2, false);
    ht->batch_delete(keys.data(), n/2, deleteResults.data(), 4);
    
    size_t successDeletes = std::count(deleteResults.begin(), deleteResults.end(), true);
    std::cout << "Delete success rate: " << successDeletes << "/" << (n/2) << std::endl;
    
    std::cout << "\nTest 3 Result:" << std::endl;
    std::cout << "Concurrent inserts: " << (successCount == n ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Concurrent lookups: " << (correctLookups == n ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Concurrent deletes: " << (successDeletes == n/2 ? "PASSED" : "FAILED") << std::endl;
}

int main(int argc, char* argv[]) {
    int num_threads = 4;
    bool run_tests = true;
    bool run_benchmarks = true;
    size_t bucket_count = 10000;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--threads" && i + 1 < argc) {
            num_threads = std::stoi(argv[++i]);
        } else if (arg == "--buckets" && i + 1 < argc) {
            bucket_count = std::stoul(argv[++i]);
        } else if (arg == "--tests-only") {
            run_benchmarks = false;
        } else if (arg == "--benchmarks-only") {
            run_tests = false;
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --threads N       Number of threads to use (default: 4)" << std::endl;
            std::cout << "  --buckets N       Number of hash table buckets (default: 10000)" << std::endl;
            std::cout << "  --tests-only      Run only the tests, not benchmarks" << std::endl;
            std::cout << "  --benchmarks-only Run only benchmarks, not tests" << std::endl;
            std::cout << "  --help            Display this help message" << std::endl;
            return 0;
        }
    }
    
    std::cout << "===================================================" << std::endl;
    std::cout << "Concurrent Hash Table Implementation" << std::endl;
    std::cout << "Using " << 
    #ifdef USE_TBB
        "Intel TBB"
    #else
        "Pthread"
    #endif
        << " with " << num_threads << " threads" << std::endl;
    std::cout << "Bucket count: " << bucket_count << std::endl;
    std::cout << "===================================================" << std::endl;
    
    std::unique_ptr<HashTableInterface> ht(HashTableFactory::createHashTable(bucket_count));
    
    if (run_tests) {
        test1(ht.get());
        test2(ht.get());
        test3(ht.get());
    }
    
    if (run_benchmarks) {
        std::vector<size_t> input_sizes = {100000, 1000000, 10000000};
        run_benchmark(ht.get(), num_threads, input_sizes);
    }
    
    return 0;
}
