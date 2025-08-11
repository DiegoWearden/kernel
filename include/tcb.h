#include "stdint.h"
#include "utils.h"
#include "core.h"
#include "atomic.h"
#include "thread.h"

class TCB {
    private:

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
            size_t elements = stack_size / sizeof(uint64_t);
            sp_bottom = new uint64_t[elements];
            sp_top = ALIGN_PTR_DOWN_16(sp_bottom + elements);
        }
        ~TCB(){
            delete[] sp_bottom;
        }

        uint64_t* get_sp_top() {
            return sp_top;
        }

        uint64_t* get_sp_bottom() {
            return sp_bottom;
        }

        size_t get_stack_size() {
            return stack_size;
        }

        uint64_t get_id() {
            return id;
        }

        Thread get_entry() {
            return thread;
        }

        TCB(const TCB&) = delete;
        TCB& operator=(const TCB&) = delete;
};