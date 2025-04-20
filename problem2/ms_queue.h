#ifndef MS_QUEUE_H
#define MS_QUEUE_H

#include <atomic>
#include <type_traits>

struct Node;

struct CountedNodePtr {
    Node* ptr;
    unsigned int count;
};

inline bool operator==(const CountedNodePtr &lhs, const CountedNodePtr &rhs) {
    return lhs.ptr == rhs.ptr && lhs.count == rhs.count;
}

static_assert(std::is_trivial<CountedNodePtr>::value, "CountedNodePtr must be a trivial type");

struct Node {
    int value;
    std::atomic<CountedNodePtr> next;

    Node(int val) : value(val) {
        CountedNodePtr initNext = { nullptr, 0 };
        next.store(initNext, std::memory_order_relaxed);
    }
};

struct MSQueue {
    std::atomic<CountedNodePtr> head;
    std::atomic<CountedNodePtr> tail;
};

bool enq(MSQueue* q, int value);
int deq(MSQueue* q);

void printQueue(MSQueue* q);
int countQueue(MSQueue* q);
MSQueue* createMSQueue();
void deleteMSQueue(MSQueue* q);

#endif
