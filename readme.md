# CS 636 Assignment 2 - Concurrent Data Structures

**Author:** Vraj Patel  
**Roll Number:** 241110080  
**Submission Date:** April 12, 2025

## Introduction

This project contains implementations of the following concurrent data structures for the CS 636 course (Semester 2024-2025-II):

1. A concurrent closed-chaining-based hash table using Pthreads.
2. An unbounded, total, lock-free concurrent queue.
3. A concurrent Bloom filter.

The project includes performance analysis for each implementation and comparisons with standard libraries like Intel TBB and Boost.

## Directory Structure

```
.
├── Hash_table/
│   ├── hash_table.h
│   ├── hash_table.cpp
│   └── problem1.cpp   # Main file for hash table tests and benchmarks
├── Queue/
│   ├── ms_queue.h
│   ├── ms_queue.cpp
│   └── problem2.cpp   # Main file for lock-free queue tests and benchmarks
├── Bloom_filter/
│   ├── bloom_filter.h
│   ├── bloom_filter.cpp
│   └── problem3.cpp   # Main file for Bloom filter tests and benchmarks
├── bin/
│   ├── random_keys_insert.bin    # Data for hash table inserts
│   ├── random_values_insert.bin  # Data for hash table/queue inserts
│   ├── random_keys_delete.bin    # Data for hash table deletes
│   └── random_keys_search.bin    # Data for hash table lookups
└── Makefile
```

**Note:** Please ensure all four binary data files are present in the `bin` directory for the benchmarks to run correctly using pre-loaded data. If the files are missing, the programs will generate random data instead.

## Compilation Instructions

A Makefile is provided for easy compilation and execution.

**Compile specific problems:**

* Compile Problem 1 (Pthread Hash Table): `make p1`
* Compile Problem 1 with TBB comparison: `make p1_tbb`
* Compile Problem 2 (Lock-Free Queue): `make p2`
* Compile Problem 3 (Bloom Filter): `make p3`
* Compile all problems: `make all`

**Run tests:**

* Run tests for Problem 1: `make p1_test`
* Run tests for Problem 2: `make p2_test`
* Run tests for Problem 3: `make p3_test`

**Run benchmarks (default 4 threads):**

* Run benchmarks for Problem 1: `make p1_benchmark`
* Run benchmarks for Problem 2: `make p2_benchmark`
* Run benchmarks for Problem 3: `make p3_benchmark`

**Run comparisons:**

* Compare Pthread vs TBB Hash Table: `make p1_compare`
* Compare MS Queue vs Boost Queue: `make p2_compare`

**Clean up:**

* Clean build files: `make clean`
* Clean binary data files: `make clean_bin`
* Clean everything: `make clean_all`

## Implementations

### Problem 1: Concurrent Hash Table

* Implements a closed-chaining hash table using Pthreads for concurrency.
* Provides `batch_insert`, `batch_lookup`, and `batch_delete` operations.
* Includes options to compile and compare against Intel TBB's concurrent hash map.
* Source files: `Hash_table/hash_table.h`, `Hash_table/hash_table.cpp`, `Hash_table/problem1.cpp`

### Problem 2: Lock-Free Queue

* Implements the Michael-Scott (MS) lock-free queue algorithm.
* Provides `enq` (enqueue) and `deq` (dequeue) operations.
* Includes correctness tests, performance tests, scalability analysis, and comparison with `boost::lockfree::queue`.
* Source files: `Queue/ms_queue.h`, `Queue/ms_queue.cpp`, `Queue/problem2.cpp`

### Problem 3: Concurrent Bloom Filter

* Implements a concurrent Bloom filter using atomic operations.
* Uses multiple hash functions (based on MurmurHash variations) to map elements.
* Provides `add` and `contains` operations.
* Includes correctness tests, performance benchmarks, and analysis of false positive rates.
* Source files: `Bloom_filter/bloom_filter.h`, `Bloom_filter/bloom_filter.cpp`, `Bloom_filter/problem3.cpp`

## Performance

Performance analysis results for various operations, thread counts, and workload sizes are detailed in the accompanying report (`241110080-assign2.pdf`). This includes throughput measurements and comparisons against Intel TBB and Boost libraries.