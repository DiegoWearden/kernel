#include "tcb.h"
#include "sched.h"

static Queue<TCB*, SpinLock>* readyQueue = nullptr;

TCB* current = nullptr;

extern "C" void context_switch(CpuContext* save, CpuContext* restore);
extern "C" void trampoline();

extern "C" void trampoline(){
    current->get_thread().run();
    yield();
}

void schedInit(){
    if(!readyQueue){
        readyQueue = new Queue<TCB*, SpinLock>(256);
    }
    // dummy tcb for boot core
    current = new TCB([]{});
}

void schedule(TCB* tcb){
    readyQueue->enqueue(tcb);
}

void yield(){
    readyQueue->enqueue(current);
    TCB* next = readyQueue->dequeue();
    if(!next){
        return;
    }
    TCB* prev = current;
    current = next;
    context_switch(prev->get_context(), next->get_context());
    printf("thread id %d has run again\n", current->get_id());
}