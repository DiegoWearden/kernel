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