#include "sched.h"
#include "percpu.h"
#include "utils.h"
#include "printf.h"
#include "heap.h"
#include "atomic.h"

struct Thread {
    uint64_t* stack_top;
    thread_entry_t entry;
    void* arg;
    int cpu;
    bool runnable;
    bool exited;
};

struct RunQueue {
    Thread* threads[64];
    int count;
    int current_idx;
};

static PerCPU<RunQueue> rq;
static Atomic<int> global_thread_count(0);

extern "C" void context_switch(uint64_t** old_sp, uint64_t* new_sp);
extern "C" void thread_trampoline();

static uint64_t* make_stack(thread_entry_t entry, void* arg)
{
    const size_t stack_size = 16 * 1024;
    uint8_t* mem = (uint8_t*)kmalloc(stack_size);
    if (!mem) return nullptr;
    // Layout: [x29][x30=tramp][x27][x28][x25][x26][x23][x24][x21][x22][x19][x20][arg][entry]
    const int qwords_regs = 12;
    uint64_t* sp = (uint64_t*)(mem + stack_size - (qwords_regs + 2) * sizeof(uint64_t));
    for (int i = 0; i < qwords_regs + 2; ++i) sp[i] = 0;
    sp[0] = 0;                                     // x29
    sp[1] = (uint64_t)thread_trampoline;           // x30 (LR)
    sp[qwords_regs + 0] = (uint64_t)arg;           // arg
    sp[qwords_regs + 1] = (uint64_t)entry;         // entry
    return sp;
}

static inline Thread* current_thread()
{
    RunQueue& r = rq.mine();
    if (r.current_idx < 0 || r.current_idx >= r.count) return nullptr;
    return r.threads[r.current_idx];
}

void sched_init()
{
    for (int i = 0; i < 4; ++i) {
        rq.forCPU(i).count = 0;
        rq.forCPU(i).current_idx = -1;
    }
}

int thread_create(thread_entry_t entry, void* arg, int preferred_cpu)
{
    Thread* t = (Thread*)kmalloc(sizeof(Thread));
    if (!t) return -1;
    t->stack_top = make_stack(entry, arg);
    if (!t->stack_top) return -1;
    t->entry = entry;
    t->arg = arg;
    t->runnable = true;
    t->exited = false;

    int target = preferred_cpu;
    if (target < 0 || target >= 4) {
        int best = 0;
        int best_load = rq.forCPU(0).count;
        for (int c = 1; c < 4; ++c) {
            if (rq.forCPU(c).count < best_load) {
                best_load = rq.forCPU(c).count;
                best = c;
            }
        }
        target = best;
    }
    t->cpu = target;

    RunQueue& r = rq.forCPU(target);
    if (r.count < 64) {
        r.threads[r.count++] = t;
        if (r.current_idx < 0) r.current_idx = 0;
    } else {
        printf("Runqueue full on cpu %d\n", target);
        return -1;
    }
    global_thread_count.fetch_add(1);
    printf("thread created on cpu %d (total=%d)\n", target, (int)global_thread_count.get());
    return 0;
}

static int find_next_index(RunQueue& r, int start)
{
    if (r.count == 0) return -1;
    for (int offset = 1; offset <= r.count; ++offset) {
        int idx = (start + offset) % r.count;
        Thread* t = r.threads[idx];
        if (t && t->runnable && !t->exited) return idx;
    }
    return -1;
}

void sched_tick(uint64_t isr_sp)
{
    RunQueue& r = rq.mine();
    if (r.count == 0) return;

    Thread* cur = (r.current_idx >= 0) ? r.threads[r.current_idx] : nullptr;
    int next_idx = find_next_index(r, r.current_idx < 0 ? 0 : r.current_idx);
    if (next_idx < 0) return; // nothing runnable

    Thread* nxt = r.threads[next_idx];
    r.current_idx = next_idx;

    uint64_t* old_sp_ptr = cur ? cur->stack_top : (uint64_t*)isr_sp;
    context_switch(&old_sp_ptr, nxt->stack_top);
    if (cur) cur->stack_top = old_sp_ptr;
}

void sched_start_cpu()
{
    RunQueue& r = rq.mine();
    if (r.count == 0) {
        while (1) asm volatile("wfe");
    }
    int idx = (r.current_idx >= 0) ? r.current_idx : find_next_index(r, 0);
    if (idx < 0) while (1) asm volatile("wfe");
    r.current_idx = idx;
    Thread* t = r.threads[idx];
    uint64_t* dummy_old = nullptr;
    context_switch(&dummy_old, t->stack_top);
    while (1) asm volatile("wfe");
}

void sched_yield()
{
    RunQueue& r = rq.mine();
    Thread* cur = current_thread();
    if (!cur) return;
    int next_idx = find_next_index(r, r.current_idx);
    if (next_idx < 0 || next_idx == r.current_idx) return;
    Thread* nxt = r.threads[next_idx];
    r.current_idx = next_idx;
    uint64_t* old_sp_ptr = cur->stack_top;
    context_switch(&old_sp_ptr, nxt->stack_top);
    cur->stack_top = old_sp_ptr;
}

void sched_exit()
{
    RunQueue& r = rq.mine();
    Thread* cur = current_thread();
    if (!cur) return;
    cur->exited = true;
    cur->runnable = false;

    // pick next runnable
    int next_idx = find_next_index(r, r.current_idx);
    if (next_idx < 0) {
        while (1) asm volatile("wfe");
    }
    Thread* nxt = r.threads[next_idx];
    r.current_idx = next_idx;

    uint64_t* dummy_old = nullptr;
    context_switch(&dummy_old, nxt->stack_top);
    while (1) asm volatile("wfe");
}

// Simple blocking atomic: wait until value equals expected then swap with new
bool atomic_wait_and_swap(volatile uint32_t* addr, uint32_t expected, uint32_t desired)
{
    while (true) {
        uint32_t cur = __atomic_load_n(addr, __ATOMIC_SEQ_CST);
        if (cur == expected) {
            if (__atomic_compare_exchange_n(addr, &cur, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
                return true;
        }
        // block/yield until next tick
        asm volatile("wfe");
    }
} 