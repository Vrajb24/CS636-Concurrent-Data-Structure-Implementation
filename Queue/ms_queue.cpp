#include "ms_queue.h"
#include <iostream>

bool enq(MSQueue* q, int value) {
    Node* newNode = new Node(value);

    CountedNodePtr tail, next, newNext;
    while (true) {
        tail = q->tail.load(std::memory_order_acquire);
        next = tail.ptr->next.load(std::memory_order_acquire);
        if (tail == q->tail.load(std::memory_order_acquire)) {
            if (next.ptr == nullptr) {
                newNext.ptr = newNode;
                newNext.count = next.count + 1;
                if (tail.ptr->next.compare_exchange_weak(next, newNext,
                                                       std::memory_order_release,
                                                       std::memory_order_relaxed)) {
                    break;
                }
            } else {
                CountedNodePtr newTail;
                newTail.ptr = next.ptr;
                newTail.count = tail.count + 1;
                q->tail.compare_exchange_weak(tail, newTail,
                                           std::memory_order_release,
                                           std::memory_order_relaxed);
            }
        }
    }
    
    CountedNodePtr newTail;
    newTail.ptr = newNode;
    newTail.count = tail.count + 1;
    q->tail.compare_exchange_weak(tail, newTail,
                                std::memory_order_release,
                                std::memory_order_relaxed);
    return true;
}

int deq(MSQueue* q) {
    CountedNodePtr head, tail, next;
    while (true) {
        head = q->head.load(std::memory_order_acquire);
        tail = q->tail.load(std::memory_order_acquire);
        next = head.ptr->next.load(std::memory_order_acquire);
        if (head == q->head.load(std::memory_order_acquire)) {
            if (head.ptr == tail.ptr) {
                if (next.ptr == nullptr) {
                    return -1;
                }
                CountedNodePtr newTail;
                newTail.ptr = next.ptr;
                newTail.count = tail.count + 1;
                q->tail.compare_exchange_weak(tail, newTail,
                                           std::memory_order_release,
                                           std::memory_order_relaxed);
            } else {
                int value = next.ptr->value;
                CountedNodePtr newHead;
                newHead.ptr = next.ptr;
                newHead.count = head.count + 1;
                if (q->head.compare_exchange_weak(head, newHead,
                                               std::memory_order_release,
                                               std::memory_order_relaxed)) {
                    delete head.ptr;
                    return value;
                }
            }
        }
    }
}

int countQueue(MSQueue* q) {
    int count = 0;
    CountedNodePtr curr = q->head.load(std::memory_order_relaxed);
    Node* node = curr.ptr;
    
    if (node != nullptr) {
        CountedNodePtr next = node->next.load(std::memory_order_relaxed);
        node = next.ptr;
    }
    
    while (node != nullptr) {
        count++;
        CountedNodePtr next = node->next.load(std::memory_order_relaxed);
        node = next.ptr;
    }
    
    return count;
}

void printQueue(MSQueue* q) {
    CountedNodePtr curr = q->head.load(std::memory_order_acquire);
    Node* node = curr.ptr;
    
    if (node == nullptr) {
        std::cout << "Queue is empty (dummy head not set).\n";
        return;
    }
    
    CountedNodePtr next = node->next.load(std::memory_order_acquire);
    
    if (next.ptr == nullptr) {
        std::cout << "Queue is empty\n";
        return;
    }
    
    std::cout << "Queue elements: ";
    while (next.ptr != nullptr) {
        std::cout << next.ptr->value << " ";
        node = next.ptr;
        next = node->next.load(std::memory_order_acquire);
    }
    
    std::cout << std::endl;
}

MSQueue* createMSQueue() {
    MSQueue* q = new MSQueue();
    
    Node* dummy = new Node(0);
    CountedNodePtr init;
    init.ptr = dummy;
    init.count = 0;
    
    q->head.store(init, std::memory_order_relaxed);
    q->tail.store(init, std::memory_order_relaxed);
    
    return q;
}

void deleteMSQueue(MSQueue* q) {
    while (deq(q) != -1) {
    }
    
    CountedNodePtr head = q->head.load(std::memory_order_relaxed);
    if (head.ptr != nullptr) {
        delete head.ptr;
    }
    
    delete q;
}
