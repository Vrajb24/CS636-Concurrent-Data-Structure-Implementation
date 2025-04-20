#include "ms_queue.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <boost/lockfree/queue.hpp>

using namespace std;
using HR = std::chrono::high_resolution_clock;

std::vector<uint32_t> read_binary_data(const std::string& file_path, size_t n) {
    std::vector<uint32_t> data(n);
    std::ifstream file(file_path, std::ios::binary);
    
    if (!file) {
        std::cerr << "Error: Could not open file " << file_path << std::endl;
        return {};
    }
    
    file.read(reinterpret_cast<char*>(data.data()), n * sizeof(uint32_t));
    size_t items_read = file.gcount() / sizeof(uint32_t);
    
    if (items_read < n) {
        std::cerr << "Warning: Requested " << n << " items but read only " << items_read << std::endl;
        data.resize(items_read);
    }
    
    return data;
}

void run_correctness_test() {
    std::cout << "\n=== Running Correctness Tests ===\n";
    
    std::cout << "Starting single-threaded tests...\n";
    MSQueue* q = createMSQueue();
    
    int res = deq(q);
    if (res != -1) {
        std::cerr << "FAIL: Dequeue from empty queue returned " << res 
                  << " instead of -1\n";
    } else {
        std::cout << "PASS: Dequeue from empty queue returned -1 as expected\n";
    }
    
    enq(q, 10);
    enq(q, 20);
    enq(q, 30);
    std::cout << "After enqueuing 10, 20, 30: ";
    printQueue(q);
    
    res = deq(q);
    if (res != 10) {
        std::cerr << "FAIL: Expected dequeue value 10, got " << res << "\n";
    } else {
        std::cout << "PASS: Dequeued 10 as expected\n";
    }
    
    res = deq(q);
    if (res != 20) {
        std::cerr << "FAIL: Expected dequeue value 20, got " << res << "\n";
    } else {
        std::cout << "PASS: Dequeued 20 as expected\n";
    }
    
    res = deq(q);
    if (res != 30) {
        std::cerr << "FAIL: Expected dequeue value 30, got " << res << "\n";
    } else {
        std::cout << "PASS: Dequeued 30 as expected\n";
    }
    
    res = deq(q);
    if (res != -1) {
        std::cerr << "FAIL: Expected -1 from empty queue, got " << res << "\n";
    } else {
        std::cout << "PASS: Dequeue from empty queue returned -1 as expected\n";
    }
    
    deleteMSQueue(q);
    
    std::cout << "\nStarting multi-threaded tests...\n";
    q = createMSQueue();
    
    std::atomic<int> enq_count(0);
    std::atomic<int> deq_count(0);
    std::atomic<int> empty_returns(0);
    
    const int num_enqueue_threads = 4;
    const int enqueues_per_thread = 25;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_enqueue_threads; i++) {
        threads.emplace_back([&q, i, enqueues_per_thread, &enq_count]() {
            for (int j = 0; j < enqueues_per_thread; j++) {
                int value = i * 1000 + j;
                enq(q, value);
                enq_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "After multi-threaded enqueues:\n";
    std::cout << "Total enqueues done: " << enq_count.load() << "\n";
    std::cout << "Queue size: " << countQueue(q) << "\n";
    
    threads.clear();
    const int num_mixed_threads = 4;
    const int ops_per_thread = 25;
    
    for (int i = 0; i < num_mixed_threads; i++) {
        threads.emplace_back([&q, i, ops_per_thread, &enq_count, &deq_count, &empty_returns]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 100);
            
            for (int j = 0; j < ops_per_thread; j++) {
                if (dis(gen) < 50) {
                    int value = i * 1000 + j + 100;
                    enq(q, value);
                    enq_count.fetch_add(1, std::memory_order_relaxed);
                } else {
                    int res = deq(q);
                    if (res != -1) {
                        deq_count.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        empty_returns.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "\nAfter multi-threaded mixed operations:\n";
    std::cout << "Total successful dequeues: " << deq_count.load() << "\n";
    std::cout << "Total empty queue returns: " << empty_returns.load() << "\n";
    std::cout << "Final queue size: " << countQueue(q) << "\n";
    
    deleteMSQueue(q);
    std::cout << "Correctness tests completed\n";
}

void run_performance_test(size_t thread_count, size_t op_count, int enq_probability = 50) {
    std::cout << "\n=== Running Performance Test ===\n";
    std::cout << "Threads: " << thread_count << ", Operations per thread: " << op_count 
              << ", Enqueue probability: " << enq_probability << "%\n";
    
    MSQueue* q = createMSQueue();
    
    std::vector<uint32_t> enq_values;
    size_t total_ops = thread_count * op_count;
    size_t expected_enqueues = total_ops * enq_probability / 100;
    
    std::string file_path = "bin/random_values_insert.bin";
    if (std::filesystem::exists(file_path)) {
        enq_values = read_binary_data(file_path, expected_enqueues);
    } else {
        std::cout << "Warning: Binary file not found. Using generated values instead.\n";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dis(1, 1000000);
        
        enq_values.resize(expected_enqueues);
        for (auto& val : enq_values) {
            val = dis(gen);
        }
    }
    
    std::atomic<size_t> enq_index(0);
    
    std::atomic<size_t> actual_enqueues(0);
    std::atomic<size_t> actual_dequeues(0);
    std::atomic<size_t> empty_dequeues(0);
    
    auto worker = [&](int thread_id) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> op_dis(0, 99);
        
        for (size_t i = 0; i < op_count; i++) {
            if (op_dis(gen) < enq_probability) {
                size_t idx = enq_index.fetch_add(1, std::memory_order_relaxed);
                int value = (idx < enq_values.size()) ? enq_values[idx] : thread_id * 1000 + i;
                enq(q, value);
                actual_enqueues.fetch_add(1, std::memory_order_relaxed);
            } else {
                int result = deq(q);
                if (result != -1) {
                    actual_dequeues.fetch_add(1, std::memory_order_relaxed);
                } else {
                    empty_dequeues.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
    };
    
    auto start_time = HR::now();
    
    std::vector<std::thread> threads;
    for (size_t i = 0; i < thread_count; i++) {
        threads.emplace_back(worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end_time = HR::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    double elapsed_ms = elapsed.count() / 1000.0;
    double throughput = (actual_enqueues.load() + actual_dequeues.load()) / (elapsed_ms / 1000.0);
    
    std::cout << "Performance Results:\n";
    std::cout << "Total time: " << elapsed_ms << " ms\n";
    std::cout << "Throughput: " << std::fixed << std::setprecision(2) << throughput 
              << " operations/second\n";
    std::cout << "Enqueues performed: " << actual_enqueues.load() << "\n";
    std::cout << "Successful dequeues: " << actual_dequeues.load() << "\n";
    std::cout << "Empty dequeues: " << empty_dequeues.load() << "\n";
    std::cout << "Final queue size: " << countQueue(q) << "\n";
    
    deleteMSQueue(q);
}

void run_scalability_test(size_t op_count = 1000000, int enq_probability = 50) {
    std::cout << "\n=== Running Scalability Test ===\n";
    std::cout << "Operations per thread: " << op_count 
              << ", Enqueue probability: " << enq_probability << "%\n";
    
    std::vector<size_t> thread_counts = {1, 2, 4, 8, 16};
    
    std::cout << "---------------------------------------------------------\n";
    std::cout << "| Threads |   Time (ms)  | Throughput (ops/s) | Speedup |\n";
    std::cout << "---------------------------------------------------------\n";
    
    double base_throughput = 0;
    
    for (size_t thread_count : thread_counts) {
        MSQueue* q = createMSQueue();
        
        std::vector<uint32_t> enq_values;
        size_t total_ops = thread_count * op_count;
        size_t expected_enqueues = total_ops * enq_probability / 100;
        
        std::string file_path = "bin/random_values_insert.bin";
        if (std::filesystem::exists(file_path)) {
            enq_values = read_binary_data(file_path, expected_enqueues);
        } else {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint32_t> dis(1, 1000000);
            
            enq_values.resize(expected_enqueues);
            for (auto& val : enq_values) {
                val = dis(gen);
            }
        }
        
        std::atomic<size_t> enq_index(0);
        
        std::atomic<size_t> actual_ops(0);
        
        auto worker = [&](int thread_id) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> op_dis(0, 99);
            
            for (size_t i = 0; i < op_count; i++) {
                if (op_dis(gen) < enq_probability) {
                    size_t idx = enq_index.fetch_add(1, std::memory_order_relaxed);
                    int value = (idx < enq_values.size()) ? enq_values[idx] : thread_id * 1000 + i;
                    enq(q, value);
                } else {
                    deq(q);
                }
                actual_ops.fetch_add(1, std::memory_order_relaxed);
            }
        };
        
        auto start_time = HR::now();
        
        std::vector<std::thread> threads;
        for (size_t i = 0; i < thread_count; i++) {
            threads.emplace_back(worker, i);
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto end_time = HR::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double elapsed_ms = elapsed.count() / 1000.0;
        double throughput = actual_ops.load() / (elapsed_ms / 1000.0);
        
        double speedup = 1.0;
        if (thread_count == 1) {
            base_throughput = throughput;
        } else {
            speedup = throughput / base_throughput;
        }
        
        std::cout << "| " << std::setw(7) << thread_count 
                  << " | " << std::setw(12) << std::fixed << std::setprecision(2) << elapsed_ms 
                  << " | " << std::setw(18) << std::fixed << std::setprecision(2) << throughput 
                  << " | " << std::setw(7) << std::fixed << std::setprecision(2) << speedup 
                  << " |\n";
        
        deleteMSQueue(q);
    }
    
    std::cout << "---------------------------------------------------------\n";
}

void compare_with_boost(size_t thread_count, size_t op_count, int enq_probability = 50) {
    std::cout << "\n=== Comparing MS Queue with Boost Queue ===\n";
    std::cout << "Threads: " << thread_count << ", Operations per thread: " << op_count 
              << ", Enqueue probability: " << enq_probability << "%\n";
    
    std::vector<uint32_t> enq_values;
    size_t total_ops = thread_count * op_count;
    size_t expected_enqueues = total_ops * enq_probability / 100;
    
    std::string file_path = "bin/random_values_insert.bin";
    if (std::filesystem::exists(file_path)) {
        enq_values = read_binary_data(file_path, expected_enqueues);
    } else {
        std::cout << "Warning: Binary file not found. Using generated values instead.\n";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dis(1, 1000000);
        
        enq_values.resize(expected_enqueues);
        for (auto& val : enq_values) {
            val = dis(gen);
        }
    }
    
    MSQueue* ms_queue = createMSQueue();
    std::atomic<size_t> ms_enq_index(0);
    std::atomic<size_t> ms_actual_ops(0);
    
    auto ms_worker = [&](int thread_id) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> op_dis(0, 99);
        
        for (size_t i = 0; i < op_count; i++) {
            if (op_dis(gen) < enq_probability) {
                size_t idx = ms_enq_index.fetch_add(1, std::memory_order_relaxed);
                int value = (idx < enq_values.size()) ? enq_values[idx] : thread_id * 1000 + i;
                enq(ms_queue, value);
            } else {
                deq(ms_queue);
            }
            ms_actual_ops.fetch_add(1, std::memory_order_relaxed);
        }
    };
    
    auto ms_start_time = HR::now();
    
    std::vector<std::thread> ms_threads;
    for (size_t i = 0; i < thread_count; i++) {
        ms_threads.emplace_back(ms_worker, i);
    }
    
    for (auto& t : ms_threads) {
        t.join();
    }
    
    auto ms_end_time = HR::now();
    auto ms_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(ms_end_time - ms_start_time);
    double ms_elapsed_ms = ms_elapsed.count() / 1000.0;
    double ms_throughput = ms_actual_ops.load() / (ms_elapsed_ms / 1000.0);
    
    boost::lockfree::queue<int> boost_queue(1000);
    std::atomic<size_t> boost_enq_index(0);
    std::atomic<size_t> boost_actual_ops(0);
    
    auto boost_worker = [&](int thread_id) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> op_dis(0, 99);
        
        for (size_t i = 0; i < op_count; i++) {
            if (op_dis(gen) < enq_probability) {
                size_t idx = boost_enq_index.fetch_add(1, std::memory_order_relaxed);
                int value = (idx < enq_values.size()) ? enq_values[idx] : thread_id * 1000 + i;
                boost_queue.push(value);
            } else {
                int value;
                boost_queue.pop(value);
            }
            boost_actual_ops.fetch_add(1, std::memory_order_relaxed);
        }
    };
    
    auto boost_start_time = HR::now();
    
    std::vector<std::thread> boost_threads;
    for (size_t i = 0; i < thread_count; i++) {
        boost_threads.emplace_back(boost_worker, i);
    }
    
    for (auto& t : boost_threads) {
        t.join();
    }
    
    auto boost_end_time = HR::now();
    auto boost_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(boost_end_time - boost_start_time);
    double boost_elapsed_ms = boost_elapsed.count() / 1000.0;
    double boost_throughput = boost_actual_ops.load() / (boost_elapsed_ms / 1000.0);
    
    std::cout << "\nComparison Results:\n";
    std::cout << "--------------------------------------------------------\n";
    std::cout << "| Implementation |   Time (ms)  | Throughput (ops/s)   |\n";
    std::cout << "--------------------------------------------------------\n";
    std::cout << "| MS Queue       | " << std::setw(12) << std::fixed << std::setprecision(2) << ms_elapsed_ms 
              << " | " << std::setw(20) << std::fixed << std::setprecision(2) << ms_throughput << " |\n";
    std::cout << "| Boost Queue    | " << std::setw(12) << std::fixed << std::setprecision(2) << boost_elapsed_ms 
              << " | " << std::setw(20) << std::fixed << std::setprecision(2) << boost_throughput << " |\n";
    std::cout << "--------------------------------------------------------\n";
    
    double relative_perf = ms_throughput / boost_throughput;
    if (relative_perf > 1.0) {
        std::cout << "MS Queue is " << std::fixed << std::setprecision(2) << relative_perf 
                  << "x faster than Boost Queue\n";
    } else {
        std::cout << "MS Queue is " << std::fixed << std::setprecision(2) << (1.0 / relative_perf) 
                  << "x slower than Boost Queue\n";
    }
    
    deleteMSQueue(ms_queue);
}

void run_workload_tests() {
    std::cout << "\n=== Running Workload Size Tests ===\n";
    
    std::vector<size_t> workload_sizes = {100000, 1000000, 10000000};
    
    for (size_t workload : workload_sizes) {
        std::cout << "\nTesting workload size: " << workload << " operations\n";
        
        const size_t thread_count = 4;
        size_t ops_per_thread = workload / thread_count;
        
        run_performance_test(thread_count, ops_per_thread);
    }
}

void show_usage() {
    std::cout << "Usage: ./problem2 <test_type> [options]\n\n";
    std::cout << "Test types:\n";
    std::cout << "  correctness    - Run correctness tests\n";
    std::cout << "  performance    - Run performance test (default)\n";
    std::cout << "  scalability    - Run scalability test with varying thread counts\n";
    std::cout << "  boost          - Compare with Boost's lock-free queue\n";
    std::cout << "  workload       - Test with different workload sizes\n";
    std::cout << "  all            - Run all tests\n\n";
    
    std::cout << "Options:\n";
    std::cout << "  --threads <n>  - Set number of threads (default: 4)\n";
    std::cout << "  --ops <n>      - Set operations per thread (default: 1000000)\n";
    std::cout << "  --enq-prob <n> - Set enqueue probability percent (default: 50)\n";
}

int main(int argc, char* argv[]) {
    std::string test_type = "performance";
    size_t thread_count = 4;
    size_t op_count = 1000000;
    int enq_probability = 50;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (i == 1 && arg[0] != '-') {
            test_type = arg;
        } else if (arg == "--threads" && i + 1 < argc) {
            thread_count = std::stoi(argv[++i]);
        } else if (arg == "--ops" && i + 1 < argc) {
            op_count = std::stoi(argv[++i]);
        } else if (arg == "--enq-prob" && i + 1 < argc) {
            enq_probability = std::stoi(argv[++i]);
            if (enq_probability < 0 || enq_probability > 100) {
                std::cerr << "Error: Enqueue probability must be between 0 and 100\n";
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            show_usage();
            return 0;
        }
    }
    
    std::cout << "=== Lock-free Queue Implementation ===\n";
    
    if (test_type == "correctness" || test_type == "all") {
        run_correctness_test();
    }
    
    if (test_type == "performance" || test_type == "all") {
        run_performance_test(thread_count, op_count, enq_probability);
    }
    
    if (test_type == "scalability" || test_type == "all") {
        run_scalability_test(op_count, enq_probability);
    }
    
    if (test_type == "boost" || test_type == "all") {
        compare_with_boost(thread_count, op_count, enq_probability);
    }
    
    if (test_type == "workload" || test_type == "all") {
        run_workload_tests();
    }
    
    return 0;
}
