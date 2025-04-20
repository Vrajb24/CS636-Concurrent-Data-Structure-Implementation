#include "bloom_filter.h"
#include <fstream>
#include <vector>
#include <pthread.h>
#include <chrono>
#include <random>
#include <atomic>
#include <iomanip>

struct ThreadArgs {
    BloomFilter* filter;
    const std::vector<uint32_t>* values;
    size_t startIndex;
    size_t endIndex;
    double addProbability;  
    std::atomic<size_t>* operationsCount;
    std::atomic<size_t>* falsePositiveCount;
    std::vector<bool>* addedValues;
};

void* workerThread(void* arg) {
    ThreadArgs* args = static_cast<ThreadArgs*>(arg);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dist(0.0, 1.0);
    
    for (size_t i = args->startIndex; i < args->endIndex; ++i) {
        uint32_t value = (*args->values)[i];
        
        if (dist(gen) < args->addProbability) {
            args->filter->add(value);
            (*args->addedValues)[i] = true;
        } else {
            bool found = args->filter->contains(value);
            
            if (found && !(*args->addedValues)[i]) {
                (*args->falsePositiveCount)++;
            }
        }
        
        (*args->operationsCount)++;
    }
    
    return nullptr;
}

void runTest1(BloomFilter& filter) {
    std::cout << "\n==== Unit Test 1: Basic Functionality ====" << std::endl;
    
    filter.add(42);
    filter.add(100);
    filter.add(255);
    filter.add(1000);
    filter.add(65535);
    
    std::cout << "Contains 42: " << (filter.contains(42) ? "Yes" : "No") << std::endl;
    std::cout << "Contains 100: " << (filter.contains(100) ? "Yes" : "No") << std::endl;
    std::cout << "Contains 500: " << (filter.contains(500) ? "Yes" : "No") << std::endl;
    
    filter.print();
}

void runTest2(int numThreads) {
    std::cout << "\n==== Unit Test 2: Concurrent Add Operations ====" << std::endl;
    
    BloomFilter filter;
    const size_t ELEMENTS_PER_THREAD = 10000;
    
    std::vector<pthread_t> threads(numThreads);
    std::vector<ThreadArgs> args(numThreads);
    std::atomic<size_t> operationsCount(0);
    std::atomic<size_t> falsePositiveCount(0);
    
    std::vector<uint32_t> testValues;
    for (size_t i = 0; i < numThreads * ELEMENTS_PER_THREAD; ++i) {
        testValues.push_back(i + 1);
    }
    
    std::vector<bool> addedValues(testValues.size(), false);
    
    for (int i = 0; i < numThreads; ++i) {
        args[i].filter = &filter;
        args[i].values = &testValues;
        args[i].startIndex = i * ELEMENTS_PER_THREAD;
        args[i].endIndex = (i + 1) * ELEMENTS_PER_THREAD;
        args[i].addProbability = 1.0;
        args[i].operationsCount = &operationsCount;
        args[i].falsePositiveCount = &falsePositiveCount;
        args[i].addedValues = &addedValues;
        
        pthread_create(&threads[i], nullptr, workerThread, &args[i]);
    }
    
    for (int i = 0; i < numThreads; ++i) {
        pthread_join(threads[i], nullptr);
    }
    
    size_t notFoundCount = 0;
    for (size_t i = 0; i < testValues.size(); ++i) {
        if (!filter.contains(testValues[i])) {
            notFoundCount++;
        }
    }
    
    std::cout << "Added " << testValues.size() << " elements concurrently" << std::endl;
    std::cout << "Elements not found: " << notFoundCount << " (should be 0)" << std::endl;
    
    filter.print();
}

int main() {
    BloomFilter testFilter;
    runTest1(testFilter);
    runTest2(4);
    
    std::vector<uint32_t> randomKeys;
    std::ifstream keyFile("bin/random_keys_insert.bin", std::ios::binary);
    
    if (!keyFile) {
        std::cerr << "Failed to open random_keys_insert.bin" << std::endl;
        std::cerr << "Using generated random values instead..." << std::endl;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
        
        for (size_t i = 0; i < 10000000; ++i) {
            randomKeys.push_back(dist(gen));
        }
    } else {
        uint32_t value;
        while (keyFile.read(reinterpret_cast<char*>(&value), sizeof(value))) {
            randomKeys.push_back(value);
        }
        keyFile.close();
    }
    
    std::cout << "\n==== Performance Benchmark ====" << std::endl;
    std::cout << "Loaded " << randomKeys.size() << " random keys" << std::endl;
    
    const std::vector<size_t> operationCounts = {100000, 1000000, 10000000};
    const std::vector<int> threadCounts = {1, 2, 4, 8, 16};
    const double ADD_PROBABILITY = 0.5;
    
    for (size_t opCount : operationCounts) {
        if (opCount > randomKeys.size()) {
            std::cout << "Not enough random keys for " << opCount << " operations. Skipping." << std::endl;
            continue;
        }
        
        std::cout << "\n----- Testing with " << opCount << " operations -----" << std::endl;
        
        std::vector<uint32_t> testKeys(randomKeys.begin(), randomKeys.begin() + opCount);
        
        for (int threadCount : threadCounts) {
            std::cout << "\nRunning with " << threadCount << " threads:" << std::endl;
            
            BloomFilter filter;
            std::vector<pthread_t> threads(threadCount);
            std::vector<ThreadArgs> args(threadCount);
            std::atomic<size_t> operationsCount(0);
            std::atomic<size_t> falsePositiveCount(0);
            std::vector<bool> addedValues(testKeys.size(), false);
            
            size_t keysPerThread = testKeys.size() / threadCount;
            
            auto startTime = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < threadCount; ++i) {
                args[i].filter = &filter;
                args[i].values = &testKeys;
                args[i].startIndex = i * keysPerThread;
                args[i].endIndex = (i == threadCount - 1) ? testKeys.size() : (i + 1) * keysPerThread;
                args[i].addProbability = ADD_PROBABILITY;
                args[i].operationsCount = &operationsCount;
                args[i].falsePositiveCount = &falsePositiveCount;
                args[i].addedValues = &addedValues;
                
                pthread_create(&threads[i], nullptr, workerThread, &args[i]);
            }
            
            for (int i = 0; i < threadCount; ++i) {
                pthread_join(threads[i], nullptr);
            }
            
            auto endTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = endTime - startTime;
            
            size_t notAddedCount = 0;
            for (bool added : addedValues) {
                if (!added) notAddedCount++;
            }
            
            double throughput = opCount / elapsed.count();
            double falsePositiveRate = (notAddedCount > 0) ? 
                static_cast<double>(falsePositiveCount) / notAddedCount : 0;
            
            std::cout << "Time elapsed: " << std::fixed << std::setprecision(6) 
                      << elapsed.count() << " seconds" << std::endl;
            std::cout << "Throughput: " << std::fixed << std::setprecision(2) 
                      << throughput << " operations/second" << std::endl;
            std::cout << "False positive rate: " << std::fixed << std::setprecision(8) 
                      << falsePositiveRate << std::endl;
            
            filter.print();
        }
    }
    
    return 0;
}
