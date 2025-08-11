#include "stdint.h"
#include "utils.h"
#include "core.h"
#include "atomic.h"
#include "sched.h"
#include "heap.h"

using ThreadEntry = void (*)(void*);

struct Thread {
    ThreadEntry entry;
    void*       arg;
    Thread() : entry(nullptr), arg(nullptr) {}

    template<typename F>
    explicit Thread(F f) { 
        init(f);   
    }

    template<typename F, typename... Args>
    Thread(F f, Args... as) {
        init([=]{ f(as...); }); 
    }

    void run() {
        entry(arg);
    }

private:
    template<typename G>
    void init(G g) {
        struct ThreadBuilder {
            G fn;
            static void thunk(void* p) {
                ThreadBuilder* b = static_cast<ThreadBuilder*>(p);
                b->fn();        
                b->~ThreadBuilder();      
                kfree(b);       
            }
        };
        void* mem = kmalloc(sizeof(ThreadBuilder));  
        ThreadBuilder* b = new (mem) ThreadBuilder{ g };
        entry = &ThreadBuilder::thunk;
        arg   = b;
    }
};

class TCB {
    private:
        CpuContext context{};
        size_t stack_size{STACK_SIZE};
        inline static Atomic<uint64_t> next_id{0};
        uint64_t id{0};
        uint64_t* sp_bottom{nullptr};
        uint64_t* sp_top{nullptr};
        Thread thread{};

    public:
        TCB(Thread thread, size_t stack_size = STACK_SIZE) : stack_size(stack_size), id(next_id.fetch_add(1)) {
            this->thread = thread;
            ASSERT(stack_size >= 1024, "Stack size must be at least 1024");
            ASSERT((stack_size % 16) == 0, "Stack size must be a multiple of 16");
            this->context.x19 = 0;
            this->context.x20 = 0;
            this->context.x21 = 0;
            this->context.x22 = 0;
            this->context.x23 = 0;
            this->context.x24 = 0;
            this->context.x25 = 0;
            this->context.x26 = 0;
            this->context.x27 = 0;
            this->context.x28 = 0;
            this->context.x29 = 0;
            this->context.x30 = reinterpret_cast<uint64_t>(&trampoline);
            size_t elements = stack_size / sizeof(uint64_t);
            sp_bottom = new uint64_t[elements];
            sp_top = ALIGN_PTR_DOWN_16(sp_bottom + elements);
            this->context.sp = (uint64_t)sp_top;
        }
        ~TCB(){
            delete[] sp_bottom;
        }

        CpuContext* get_context() { return &context; }
        Thread& get_thread() { return thread; }

        uint64_t* get_sp_top() { return sp_top; }
        uint64_t* get_sp_bottom() { return sp_bottom; }
        size_t get_stack_size() { return stack_size; }
        uint64_t get_id() { return id; }

        TCB(const TCB&) = delete;
        TCB& operator=(const TCB&) = delete;
};