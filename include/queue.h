#ifndef _QUEUE_H_
#define _QUEUE_H_

#include "libk.h"
#include "heap.h"
#include "stdint.h"


template <typename T, typename LockType>
class Queue{
    public:
        Queue(int capacity){
            this->capacity = capacity;
            this->data = new int[capacity];
            this->head = 0;
            this->tail = 0;
            this->size = 0;
        };
        ~Queue(){
            if(this->data){
                delete[] this->data;
                this->data = nullptr;
            }
            this->capacity = 0;
            this->head = 0;
            this->tail = 0;
            this->size = 0;
        };

        void enqueue(int value){
            LockGuard g{this->lock};
            K::assert(size < capacity, "Queue::enqueue: queue is full");
            this->data[this->tail] = value;
            this->tail = (this->tail + 1) % this->capacity;
            this->size++;
        }
        int dequeue(){
            LockGuard g{this->lock};
            K::assert(size > 0, "Queue::dequeue: queue is empty");
            int value = this->data[this->head];
            this->head = (this->head + 1) % capacity;
            this->size--;
            return value;
        }

    private:
    int* data;
    int head;
    int tail;
    int size;
    int capacity;
    LockType lock;
};

#endif // _QUEUE_H_