#include "hash_table.h"
#include <cstdlib>
#include <algorithm>
#include <functional>


PthreadHashTable::PthreadHashTable(size_t cap)
    : capacity(cap), buckets(cap, nullptr), locks(cap), pool_index(0)
{
    size_t initial_pool_size = std::min(POOL_SIZE, capacity * 10);
    node_pool.reserve(initial_pool_size);
    
    for (size_t i = 0; i < initial_pool_size; ++i) {
        node_pool.push_back(static_cast<Node*>(std::malloc(sizeof(Node))));
        if (!node_pool.back()) {
            std::cerr << "Error: Failed to allocate memory for node pool" << std::endl;
            exit(1);
        }
    }
}

PthreadHashTable::~PthreadHashTable() {
    for (size_t i = 0; i < capacity; i++) {
        Node* curr = buckets[i];
        buckets[i] = nullptr;
        while (curr) {
            Node* temp = curr;
            curr = curr->next;
            std::free(temp);
        }
    }
    
    for (size_t i = 0; i < node_pool.size(); i++) {
        if (node_pool[i]) {
            std::free(node_pool[i]);
            node_pool[i] = nullptr;
        }
    }
    
    std::lock_guard<std::mutex> lock(free_list_mutex);
    free_list.clear();
}


Node* PthreadHashTable::allocate_node(uint32_t key, uint32_t value) {
    Node* node = nullptr;
    
    {
        std::lock_guard<std::mutex> lock(free_list_mutex);
        if (!free_list.empty()) {
            node = free_list.back();
            free_list.pop_back();
        }
    }
    
    if (!node) {
        size_t idx = pool_index.fetch_add(1, std::memory_order_relaxed);
        if (idx < node_pool.size()) {
            node = node_pool[idx];
        } else {
            std::lock_guard<std::mutex> lock(pool_mutex);
            node = static_cast<Node*>(std::malloc(sizeof(Node)));
            if (!node) {
                std::cerr << "Error: Failed to allocate memory for node" << std::endl;
                exit(1);
            }
        }
    }
    
    node->key = key;
    node->value = value;
    node->next = nullptr;
    
    return node;
}

void PthreadHashTable::free_node(Node* node) {
    if (!node) return;
    
    std::lock_guard<std::mutex> lock(free_list_mutex);
    free_list.push_back(node);
}

void PthreadHashTable::print() {
    std::cout << "Hash Table Contents:" << std::endl;
    size_t total_nodes = 0;
    
    for (size_t i = 0; i < capacity; ++i) {
        std::lock_guard<std::mutex> lg(locks[i]);
        Node* curr = buckets[i];
        
        if (!curr) continue;
        
        std::cout << "Bucket " << i << ": ";
        size_t chain_length = 0;
        
        while (curr) {
            std::cout << "(" << curr->key << "->" << curr->value << ") ";
            curr = curr->next;
            chain_length++;
            total_nodes++;
        }
        
        std::cout << "Length: " << chain_length << std::endl;
    }
    
    std::cout << "Total nodes in table: " << total_nodes << std::endl;
    std::cout << "Pool usage: " << pool_index.load() << "/" << node_pool.size() << std::endl;
    
    {
        std::lock_guard<std::mutex> lock(free_list_mutex);
        std::cout << "Free list size: " << free_list.size() << std::endl;
    }
}

struct InsertArgs {
    PthreadHashTable* ht;
    size_t start;
    size_t end;
    const uint32_t* keys;
    const uint32_t* vals;
    uint8_t* results;
};

void insert_thread_func(InsertArgs* args) {
    PthreadHashTable* ht = args->ht;
    
    for (size_t i = args->start; i < args->end; ++i) {
        uint32_t key = args->keys[i];
        uint32_t val = args->vals[i];
        size_t bucket = key % ht->size();
        
        bool exists = false;
        {
            std::lock_guard<std::mutex> lg(ht->locks[bucket]);
            Node* curr = ht->buckets[bucket];
            while (curr) {
                if (curr->key == key) {
                    exists = true;
                    break;
                }
                curr = curr->next;
            }
            
            if (!exists) {
                Node* newNode = ht->allocate_node(key, val);
                newNode->next = ht->buckets[bucket];
                ht->buckets[bucket] = newNode;
                args->results[i] = true;
            } else {
                args->results[i] = false;
            }
        }
    }
    
    delete args;
}

void PthreadHashTable::batch_insert(const uint32_t* keys, const uint32_t* vals, size_t n, uint8_t* results, int numThreads) {
    numThreads = std::max(1, numThreads);
    
    std::vector<std::thread> threads;
    size_t chunk = (n + numThreads - 1) / numThreads;
    
    for (int t = 0; t < numThreads; ++t) {
        size_t start = t * chunk;
        size_t end = std::min(n, (t + 1) * chunk);
        
        if (start >= end) continue;
        
        InsertArgs* args = new InsertArgs{this, start, end, keys, vals, results};
        threads.emplace_back(insert_thread_func, args);
    }
    
    for (auto& thr : threads) {
        thr.join();
    }
}

struct LookupArgs {
    PthreadHashTable* ht;
    size_t start;
    size_t end;
    const uint32_t* keys;
    uint32_t* results;
};

void lookup_thread_func(LookupArgs* args) {
    PthreadHashTable* ht = args->ht;
    
    for (size_t i = args->start; i < args->end; ++i) {
        uint32_t key = args->keys[i];
        size_t bucket = key % ht->size();
        
        std::lock_guard<std::mutex> lg(ht->locks[bucket]);
        
        Node* curr = ht->buckets[bucket];
        uint32_t value = 0;
        
        while (curr) {
            if (curr->key == key) {
                value = curr->value;
                break;
            }
            curr = curr->next;
        }
        
        args->results[i] = value;
    }
    
    delete args;
}

void PthreadHashTable::batch_lookup(const uint32_t* keys, size_t n, uint32_t* results, int numThreads) {
    numThreads = std::max(1, numThreads);
    
    std::vector<std::thread> threads;
    size_t chunk = (n + numThreads - 1) / numThreads;
    
    for (int t = 0; t < numThreads; ++t) {
        size_t start = t * chunk;
        size_t end = std::min(n, (t + 1) * chunk);
        
        if (start >= end) continue;
        
        LookupArgs* args = new LookupArgs{this, start, end, keys, results};
        threads.emplace_back(lookup_thread_func, args);
    }
    
    for (auto& thr : threads) {
        thr.join();
    }
}

struct DeleteArgs {
    PthreadHashTable* ht;
    size_t start;
    size_t end;
    const uint32_t* keys;
    uint8_t* results;
};

void delete_thread_func(DeleteArgs* args) {
    PthreadHashTable* ht = args->ht;
    
    for (size_t i = args->start; i < args->end; ++i) {
        uint32_t key = args->keys[i];
        size_t bucket = key % ht->size();
        
        std::lock_guard<std::mutex> lg(ht->locks[bucket]);
        
        Node* curr = ht->buckets[bucket];
        Node* prev = nullptr;
        bool found = false;
        
        while (curr) {
            if (curr->key == key) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    ht->buckets[bucket] = curr->next;
                }
                
                Node* to_free = curr;
                ht->free_node(to_free);
                
                found = true;
                break;
            }
            
            prev = curr;
            curr = curr->next;
        }
        
        args->results[i] = found;
    }
    
    delete args;
}

void PthreadHashTable::batch_delete(const uint32_t* keys, size_t n, uint8_t* results, int numThreads) {
    numThreads = std::max(1, numThreads);
    
    std::vector<std::thread> threads;
    size_t chunk = (n + numThreads - 1) / numThreads;
    
    for (int t = 0; t < numThreads; ++t) {
        size_t start = t * chunk;
        size_t end = std::min(n, (t + 1) * chunk);
        
        if (start >= end) continue;
        
        DeleteArgs* args = new DeleteArgs{this, start, end, keys, results};
        threads.emplace_back(delete_thread_func, args);
    }
    
    for (auto& thr : threads) {
        thr.join();
    }
}

#ifdef USE_TBB

TBBHashTable::TBBHashTable(size_t cap) : capacity(cap) {
}

void TBBHashTable::print() {
    std::cout << "TBB Hash Table Contents:" << std::endl;
    size_t total_nodes = 0;
    
    for (auto it = table.begin(); it != table.end(); ++it) {
        std::cout << "(" << it->first << "->" << it->second << ") ";
        total_nodes++;
        
        if (total_nodes % 5 == 0) std::cout << std::endl;
    }
    
    std::cout << "\nTotal nodes in table: " << total_nodes << std::endl;
}

void TBBHashTable::batch_insert(const uint32_t* keys, const uint32_t* vals, size_t n, uint8_t* results, int numThreads) {
    #pragma omp parallel for num_threads(numThreads)
    for (size_t i = 0; i < n; ++i) {
        typename ConcurrentHashMap::accessor acc;
        bool inserted = table.insert(acc, keys[i]);
        if (inserted) {
            acc->second = vals[i];
            results[i] = 1;
        } else {
            results[i] = 0;
        }
    }
}

void TBBHashTable::batch_lookup(const uint32_t* keys, size_t n, uint32_t* results, int numThreads) {
    #pragma omp parallel for num_threads(numThreads)
    for (size_t i = 0; i < n; ++i) {
        typename ConcurrentHashMap::const_accessor acc;
        if (table.find(acc, keys[i])) {
            results[i] = acc->second;
        } else {
            results[i] = 0;
        }
    }
}

void TBBHashTable::batch_delete(const uint32_t* keys, size_t n, uint8_t* results, int numThreads) {
    #pragma omp parallel for num_threads(numThreads)
    for (size_t i = 0; i < n; ++i) {
        results[i] = table.erase(keys[i]) ? 1 : 0;
    }
}

#endif
