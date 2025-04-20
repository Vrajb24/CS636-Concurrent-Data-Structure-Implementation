#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <vector>
#include <mutex>
#include <atomic>
#include <iostream>
#include <memory>
#include <fstream>
#include <chrono>
#include <thread>
#include <functional>
#include <string>
#include <iomanip>

#ifdef USE_TBB
#include <tbb/concurrent_hash_map.h>
#endif

struct Node {
    uint32_t key;
    uint32_t value;
    Node* next;
};

class HashTableInterface {
public:
    virtual ~HashTableInterface() = default;
    
    virtual void batch_insert(const uint32_t* keys, const uint32_t* vals, size_t n, uint8_t* results, int numThreads) = 0;
    virtual void batch_lookup(const uint32_t* keys, size_t n, uint32_t* results, int numThreads) = 0;
    virtual void batch_delete(const uint32_t* keys, size_t n, uint8_t* results, int numThreads) = 0;
    
    virtual void print() = 0;
    
    virtual size_t size() const = 0;
};

struct InsertArgs;
struct LookupArgs;
struct DeleteArgs;

class PthreadHashTable : public HashTableInterface {

public:

    PthreadHashTable(size_t cap);
    ~PthreadHashTable();

    void print() override;

    void batch_insert(const uint32_t* keys, const uint32_t* vals, size_t n, uint8_t* results, int numThreads) override;
    void batch_lookup(const uint32_t* keys, size_t n, uint32_t* results, int numThreads) override;
    void batch_delete(const uint32_t* keys, size_t n, uint8_t* results, int numThreads) override;

    size_t size() const override { return capacity; }

private:
    Node* allocate_node(uint32_t key, uint32_t value);
    void free_node(Node* node);
    size_t hash_function(uint32_t key) const { return key % capacity; }

    size_t capacity;
    std::vector<Node*> buckets;
    std::vector<std::mutex> locks;
    
    static constexpr size_t POOL_SIZE = 10000000;
    std::vector<Node*> node_pool;
    std::atomic<size_t> pool_index;
    std::mutex pool_mutex;

    std::vector<Node*> free_list;
    std::mutex free_list_mutex;

    friend void insert_thread_func(InsertArgs*);
    friend void lookup_thread_func(LookupArgs*);
    friend void delete_thread_func(DeleteArgs*);

};

#ifdef USE_TBB
class TBBHashTable : public HashTableInterface {
public:
    TBBHashTable(size_t cap);
    ~TBBHashTable() = default;
    
    void print() override;
    void batch_insert(const uint32_t* keys, const uint32_t* vals, size_t n, uint8_t* results, int numThreads) override;
    void batch_lookup(const uint32_t* keys, size_t n, uint32_t* results, int numThreads) override;
    void batch_delete(const uint32_t* keys, size_t n, uint8_t* results, int numThreads) override;
    
    size_t size() const override { return capacity; }

private:
    size_t capacity;
    struct HashCompare {
        static size_t hash(const uint32_t& x) { return x; }
        static bool equal(const uint32_t& x, const uint32_t& y) { return x == y; }
    };
    
    typedef tbb::concurrent_hash_map<uint32_t, uint32_t, HashCompare> ConcurrentHashMap;
    ConcurrentHashMap table;
};
#endif

class HashTableFactory {
public:
    static HashTableInterface* createHashTable(size_t capacity) {
#ifdef USE_TBB
        return new TBBHashTable(capacity);
#else
        return new PthreadHashTable(capacity);
#endif
    }
};

#ifdef USE_TBB
typedef TBBHashTable HashTable;
#else
typedef PthreadHashTable HashTable;
#endif

#endif
