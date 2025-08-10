class TCB {
    public:
        TCB();
        ~TCB();

    private:
        uint64_t id;
        uint64_t core_id;
        uint64_t stack_size;
        uint64_t stack_base;
        uint64_t stack_top;
};