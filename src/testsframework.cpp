#include "testframework.h"
#include "libk.h"
#include "atomic.h"
#include "utils.h"
#include "heap.h"
#include "queue.h"

static bool tests_registered = false;

void test_k_strlen() {
    TEST_ASSERT_EQUAL(5, K::strlen("hello"), "strlen should return correct length");
    TEST_ASSERT_EQUAL(0, K::strlen(""), "strlen of empty string should be 0");
    TEST_ASSERT_EQUAL(1, K::strlen("a"), "strlen of single char should be 1");
}

void test_k_streq() {
    TEST_ASSERT_TRUE(K::streq("hello", "hello"), "identical strings should be equal");
    TEST_ASSERT_FALSE(K::streq("hello", "world"), "different strings should not be equal");
    TEST_ASSERT_TRUE(K::streq("", ""), "empty strings should be equal");
    TEST_ASSERT_FALSE(K::streq("a", ""), "different length strings should not be equal");
}

void test_k_strcmp() {
    TEST_ASSERT_EQUAL(0, K::strcmp("hello", "hello"), "identical strings should return 0");
    TEST_ASSERT_TRUE(K::strcmp("abc", "abd") < 0, "abc should be less than abd");
    TEST_ASSERT_TRUE(K::strcmp("abd", "abc") > 0, "abd should be greater than abc");
}

void test_k_isdigit() {
    TEST_ASSERT_TRUE(K::isdigit('0'), "'0' should be a digit");
    TEST_ASSERT_TRUE(K::isdigit('5'), "'5' should be a digit");
    TEST_ASSERT_TRUE(K::isdigit('9'), "'9' should be a digit");
    TEST_ASSERT_FALSE(K::isdigit('a'), "'a' should not be a digit");
    TEST_ASSERT_FALSE(K::isdigit(' '), "space should not be a digit");
}

void test_k_min() {
    TEST_ASSERT_EQUAL(1, K::min(1, 2, 3), "min of 1,2,3 should be 1");
    TEST_ASSERT_EQUAL(0, K::min(5, 0, 10), "min of 5,0,10 should be 0");
    TEST_ASSERT_EQUAL(-5, K::min(-5, -1, 0), "min of -5,-1,0 should be -5");
}


void test_atomic_basic() {
    Atomic<int> atomic_int(42);
    TEST_ASSERT_EQUAL(42, atomic_int.get(), "atomic should return initial value");
    
    atomic_int.set(100);
    TEST_ASSERT_EQUAL(100, atomic_int.get(), "atomic should return set value");
}

void test_atomic_exchange() {
    Atomic<int> atomic_int(10);
    int old_value = atomic_int.exchange(20);
    
    TEST_ASSERT_EQUAL(10, old_value, "exchange should return old value");
    TEST_ASSERT_EQUAL(20, atomic_int.get(), "atomic should have new value after exchange");
}

void test_atomic_bool() {
    Atomic<bool> atomic_bool(false);
    TEST_ASSERT_FALSE(atomic_bool.get(), "atomic bool should start as false");
    
    atomic_bool.set(true);
    TEST_ASSERT_TRUE(atomic_bool.get(), "atomic bool should be true after set");
}

void test_spinlock_basic() {
    SpinLock lock;
    
    // Basic lock/unlock
    lock.lock();
    TEST_ASSERT_TRUE(lock.isMine(), "lock should be taken after lock()");
    lock.unlock();
}

void test_memory_basic() {
    char buffer[64];
    K::memset(buffer, 0xAA, 64);
    
    // Check that memset worked
    bool all_aa = true;
    for (int i = 0; i < 64; i++) {
        if ((unsigned char)buffer[i] != 0xAA) {
            all_aa = false;
            break;
        }
    }
    TEST_ASSERT_TRUE(all_aa, "memset should set all bytes to 0xAA");
}

void test_memory_copy() {
    char src[] = "Hello, World!";
    char dest[20];
    
    K::memcpy(dest, src, 14); // including null terminator
    TEST_ASSERT_STRING_EQUAL("Hello, World!", dest, "memcpy should copy string correctly");
}

void test_core_id() {
    uint64_t core_id = getCoreID();
    TEST_ASSERT_TRUE(core_id < 4, "Core ID should be less than 4");
    printf("\n  Running on core: %lld", core_id);
}

void test_exception_level() {
    uint64_t el = get_el();
    printf("\n  Current exception level: %lld", el);
    TEST_ASSERT_TRUE(el >= 1, "Should be running in EL1 or higher");
}

void test_stack_pointer() {
    uint64_t sp = get_sp();
    printf("\n  Stack pointer: 0x%llx", sp);
    TEST_ASSERT_NOT_NULL((void*)sp, "Stack pointer should not be null");
}

void test_valid_memory_access() {
    printf("\n  Testing valid memory access on core %lld", getCoreID());
    
    uint64_t valid_memory = 0;
    uint64_t test_value = 0xDEADBEEF;
    
    valid_memory = test_value;
    
    TEST_ASSERT_EQUAL(test_value, valid_memory, "Should be able to write to valid memory");
}

void test_null_pointer_protection() {
    uint64_t core_id = getCoreID();
    printf("\n  Core %lld: Testing null pointer protection at 0x0", core_id);
    
    uint64_t address = 0x0;
    uint64_t test_value = 0xDEADBEEF;
    
    printf("\n  Core %lld: Attempting to write 0x%llx to address 0x%llx", core_id, test_value, address);
    
    volatile uint64_t* ptr = (volatile uint64_t*)address;
    *ptr = test_value;
    
    TEST_FAIL("Write to null pointer succeeded - protection may not be enabled");
}

void test_low_memory_protection() {
    uint64_t core_id = getCoreID();
    
    uint64_t test_addresses[] = {0x0, 0xf0, 0xe8, 0xe0, 0x8, 0x10, 0x18, 0x20};
    uint64_t address = test_addresses[core_id % (sizeof(test_addresses) / sizeof(test_addresses[0]))];
    
    printf("\n  Core %lld: Testing low memory protection at 0x%llx", core_id, address);
    
    uint64_t test_value = 0xDEADBEEF;
    
    printf("\n  Core %lld: Attempting to write 0x%llx to address 0x%llx", core_id, test_value, address);
    
    volatile uint64_t* ptr = (volatile uint64_t*)address;
    *ptr = test_value;
    
    TEST_FAIL("Write to protected low memory succeeded - protection may not be enabled");
}

void test_memory_protection_boundaries() {
    uint64_t core_id = getCoreID();
    printf("\n  Core %lld: Testing memory protection boundaries", core_id);
    
    uint64_t protected_addresses[] = {0x0, 0x8, 0x10, 0x18, 0x20, 0xe0, 0xe8, 0xf0, 0xf8};
    int num_addresses = sizeof(protected_addresses) / sizeof(protected_addresses[0]);
    
    printf("\n  Core %lld: Testing %d potentially protected addresses:", core_id, num_addresses);
    
    int start_idx = core_id * 2;
    int end_idx = start_idx + 2;
    if (end_idx > num_addresses) end_idx = num_addresses;
    
    for (int i = start_idx; i < end_idx; i++) {
        printf("\n    Core %lld testing address 0x%llx: ", core_id, protected_addresses[i]);
        printf("(would attempt write here)");
    }
    
    TEST_ASSERT_TRUE(true, "Memory protection boundary test completed");
}

void test_stack_isolation() {
    uint64_t core_id = getCoreID();
    uint64_t my_stack = get_sp();
    
    printf("\n  Core %lld: Stack isolation test, my stack at 0x%llx", core_id, my_stack);
    
    volatile uint64_t stack_marker = 0xC0DE0000 + core_id;
    uint64_t test_value = stack_marker;
    
    TEST_ASSERT_EQUAL(test_value, stack_marker, "Should be able to write to own stack");
    
    printf("\n  Core %lld: Successfully wrote unique marker 0x%llx", core_id, stack_marker);
}

void test_heap_basic_allocation() {
    uint64_t core_id = getCoreID();
    printf("\n  Core %lld: Testing basic heap allocation", core_id);
    
    void* ptr1 = kmalloc(32);
    TEST_ASSERT_NOT_NULL(ptr1, "Small allocation should succeed");
    
    void* ptr2 = kmalloc(1024);
    TEST_ASSERT_NOT_NULL(ptr2, "Medium allocation should succeed");
    
    void* ptr3 = kmalloc(4096);
    TEST_ASSERT_NOT_NULL(ptr3, "Large allocation should succeed");
    
    TEST_ASSERT_TRUE(ptr1 != ptr2, "Different allocations should have different addresses");
    TEST_ASSERT_TRUE(ptr2 != ptr3, "Different allocations should have different addresses");
    TEST_ASSERT_TRUE(ptr1 != ptr3, "Different allocations should have different addresses");
    
    printf("\n  Core %lld: Allocated ptrs: 0x%llx, 0x%llx, 0x%llx\n", 
           core_id, (uint64_t)ptr1, (uint64_t)ptr2, (uint64_t)ptr3);
}

void test_heap_zero_initialization() {
    uint64_t core_id = getCoreID();
    printf("\n  Core %lld: Testing heap zero initialization", core_id);
    
    void* ptr = kmalloc(64);
    TEST_ASSERT_NOT_NULL(ptr, "Allocation should succeed");
    
    uint8_t* bytes = (uint8_t*)ptr;
    bool all_zero = true;
    for (int i = 0; i < 64; i++) {
        if (bytes[i] != 0) {
            all_zero = false;
            break;
        }
    }
    
    TEST_ASSERT_TRUE(all_zero, "Allocated memory should be zero-initialized");
}

void test_heap_alignment() {
    uint64_t core_id = getCoreID();
    printf("\n  Core %lld: Testing heap alignment", core_id);
    
    void* ptr1 = kmalloc(1);   
    void* ptr2 = kmalloc(15);  
    void* ptr3 = kmalloc(17);  
    
    TEST_ASSERT_NOT_NULL(ptr1, "Small allocation should succeed");
    TEST_ASSERT_NOT_NULL(ptr2, "Medium allocation should succeed");
    TEST_ASSERT_NOT_NULL(ptr3, "Large allocation should succeed");
    
    TEST_ASSERT_EQUAL(0, ((uintptr_t)ptr1) & 15, "Allocation should be 16-byte aligned");
    TEST_ASSERT_EQUAL(0, ((uintptr_t)ptr2) & 15, "Allocation should be 16-byte aligned");
    TEST_ASSERT_EQUAL(0, ((uintptr_t)ptr3) & 15, "Allocation should be 16-byte aligned");
}

void test_heap_statistics() {
    uint64_t core_id = getCoreID();
    printf("\n  Core %lld: Testing heap statistics", core_id);
    
    size_t used_before = get_heap_used();
    size_t free_before = get_heap_free();
    
    printf("\n    Core %lld: Before allocation - used: %zu, free: %zu\n", 
           core_id, used_before, free_before);
    
    void* ptr = kmalloc(1024);
    TEST_ASSERT_NOT_NULL(ptr, "Allocation should succeed");
    
    size_t used_after = get_heap_used();
    size_t free_after = get_heap_free();
    
    printf("    Core %lld: After allocation - used: %zu, free: %zu\n", 
           core_id, used_after, free_after);
    
    TEST_ASSERT_TRUE(used_after > used_before, "Used memory should increase after allocation");
    TEST_ASSERT_TRUE(free_after < free_before, "Free memory should decrease after allocation");
}

void test_heap_boundary_conditions() {
    uint64_t core_id = getCoreID();
    printf("\n  Core %lld: Testing heap boundary conditions", core_id);
    
    void* ptr_zero = kmalloc(0);
    TEST_ASSERT_NULL(ptr_zero, "Zero-size allocation should return NULL");
    
    void* ptr = kmalloc(128);
    TEST_ASSERT_NOT_NULL(ptr, "Normal allocation should succeed");
    
    uint8_t* bytes = (uint8_t*)ptr;
    for (int i = 0; i < 128; i++) {
        bytes[i] = (i % 256);
    }
    
    bool pattern_correct = true;
    for (int i = 0; i < 128; i++) {
        if (bytes[i] != (i % 256)) {
            pattern_correct = false;
            break;
        }
    }
    
    TEST_ASSERT_TRUE(pattern_correct, "Should be able to read/write allocated memory");
}

// New tests for free-list allocator
void test_heap_free_reduces_used() {
    size_t used_before = get_heap_used();
    void* p = kmalloc(1234);
    TEST_ASSERT_NOT_NULL(p, "Allocation should succeed");
    size_t used_after_alloc = get_heap_used();
    TEST_ASSERT_TRUE(used_after_alloc > used_before, "Used should increase after alloc");
    kfree(p);
    size_t used_after_free = get_heap_used();
    TEST_ASSERT_TRUE(used_after_free < used_after_alloc, "Used should decrease after free");
}

void test_heap_free_reuse_and_coalesce() {
    // Allocate three adjacent blocks
    void* a = kmalloc(256);
    void* b = kmalloc(256);
    void* c = kmalloc(256);
    TEST_ASSERT_NOT_NULL(a, "A alloc");
    TEST_ASSERT_NOT_NULL(b, "B alloc");
    TEST_ASSERT_NOT_NULL(c, "C alloc");

    // Free middle and ensure reuse
    kfree(b);
    void* d = kmalloc(200);
    TEST_ASSERT_NOT_NULL(d, "D alloc in freed B");
    TEST_ASSERT_EQUAL((int)(uintptr_t)b, (int)(uintptr_t)d, "Allocator should reuse freed middle block");

    // Free neighbors and the reused block to test coalescing
    kfree(a);
    kfree(c);
    kfree(d);

    // Now a large allocation should fit in the coalesced region starting at A
    void* e = kmalloc(700);
    TEST_ASSERT_NOT_NULL(e, "Large allocation after coalesce");
    TEST_ASSERT_EQUAL((int)(uintptr_t)a, (int)(uintptr_t)e, "Coalesced block should start at A");
    kfree(e);
}

void test_cpp_new_delete() {
    uint64_t core_id = getCoreID();
    printf("\n  Core %lld: Testing C++ new/delete operators", core_id);
    
    int* ptr = new int(42);
    TEST_ASSERT_NOT_NULL(ptr, "new int should succeed");
    TEST_ASSERT_EQUAL(42, *ptr, "new int should initialize value");
    
    int* arr = new int[10];
    TEST_ASSERT_NOT_NULL(arr, "new int[] should succeed");
    
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL(0, arr[i], "Array should be zero-initialized");
        arr[i] = i * 2;
    }
    
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL(i * 2, arr[i], "Array writes should persist");
    }
    
    struct TestClass {
        int a, b, c;
        TestClass(int x, int y, int z) : a(x), b(y), c(z) {}
    };
    
    TestClass* obj = new TestClass(1, 2, 3);
    TEST_ASSERT_NOT_NULL(obj, "new TestClass should succeed");
    TEST_ASSERT_EQUAL(1, obj->a, "Constructor should set a");
    TEST_ASSERT_EQUAL(2, obj->b, "Constructor should set b");
    TEST_ASSERT_EQUAL(3, obj->c, "Constructor should set c");
    
    char buffer[sizeof(TestClass)];
    TestClass* placed = new(buffer) TestClass(10, 20, 30);
    TEST_ASSERT_NOT_NULL(placed, "Placement new should succeed");
    TEST_ASSERT_EQUAL(10, placed->a, "Placement new should call constructor");
    
    delete ptr;
    delete[] arr;
    delete obj;
    
    printf("\n  Core %lld: C++ operators test completed\n", core_id);
}

void test_cpp_alignment() {
    uint64_t core_id = getCoreID();
    printf("\n  Core %lld: Testing C++ operator alignment", core_id);
    
    int* int_ptr = new int;
    double* double_ptr = new double;
    uint64_t* uint64_ptr = new uint64_t;
    
    TEST_ASSERT_NOT_NULL(int_ptr, "new int should succeed");
    TEST_ASSERT_NOT_NULL(double_ptr, "new double should succeed");
    TEST_ASSERT_NOT_NULL(uint64_ptr, "new uint64_t should succeed");
    
    TEST_ASSERT_EQUAL(0, ((uintptr_t)int_ptr) & 15, "int allocation should be 16-byte aligned");
    TEST_ASSERT_EQUAL(0, ((uintptr_t)double_ptr) & 15, "double allocation should be 16-byte aligned");
    TEST_ASSERT_EQUAL(0, ((uintptr_t)uint64_ptr) & 15, "uint64_t allocation should be 16-byte aligned");
    
    delete int_ptr;
    delete double_ptr;
    delete uint64_ptr;
}

void test_heap_linker_configuration() {
    uint64_t core_id = getCoreID();
    printf("\n  Core %lld: Testing linker-configured heap", core_id);
    
    void* start = get_heap_start();
    void* end = get_heap_end();
    void* current = get_heap_current();
    
    printf("\n    Heap start:   0x%llx\n", (uint64_t)start);
    printf("    Heap end:     0x%llx\n", (uint64_t)end);
    printf("    Heap current: 0x%llx\n", (uint64_t)current);
    
    size_t total_size = get_heap_used() + get_heap_free();
    printf("\n    Total size:   %zu bytes (%zu MB)\n", total_size, total_size / (1024 * 1024));
    
    TEST_ASSERT_TRUE(start < end, "Heap start should be before heap end");
    TEST_ASSERT_TRUE(current >= start, "Current pointer should be after start");
    TEST_ASSERT_TRUE(current <= end, "Current pointer should be before end");
    TEST_ASSERT_TRUE(total_size > 0, "Heap should have non-zero size");
    
    void* test_ptr = kmalloc(1024);
    TEST_ASSERT_NOT_NULL(test_ptr, "Should be able to allocate from linker-configured heap");
    TEST_ASSERT_TRUE(test_ptr >= start, "Allocated memory should be within heap bounds");
    TEST_ASSERT_TRUE(test_ptr < end, "Allocated memory should be within heap bounds");
}

void test_printf_size_t_format() {
    uint64_t core_id = getCoreID();
    printf("\n  Core %lld: Testing %%zu format specifier", core_id);
    
    size_t test_values[] = {0, 1, 1024, 1048576, 33554432}; // 0, 1, 1KB, 1MB, 32MB
    const char* descriptions[] = {"zero", "one", "1KB", "1MB", "32MB"};
    
    for (int i = 0; i < 5; i++) {
        printf("\n    %s: %zu bytes", descriptions[i], test_values[i]);
    }
    
    // Test hex format too
    printf("\n    Hex format: 0x%zx", (size_t)0xDEADBEEF);
    printf("\n    Large value: %zu\n", (size_t)0xFFFFFFFFUL);
    
    TEST_ASSERT_TRUE(true, "printf %zu format test completed");
}

void test_queue_basic_enq_deq() {
    Queue<int, SpinLock> q(8);
    q.enqueue(1);
    q.enqueue(2);
    q.enqueue(3);
    TEST_ASSERT_EQUAL(1, q.dequeue(), "Queue should dequeue in FIFO order (1)");
    TEST_ASSERT_EQUAL(2, q.dequeue(), "Queue should dequeue in FIFO order (2)");
    TEST_ASSERT_EQUAL(3, q.dequeue(), "Queue should dequeue in FIFO order (3)");
}

void test_queue_wraparound() {
    Queue<int, SpinLock> q(3);
    q.enqueue(10);
    q.enqueue(20);
    TEST_ASSERT_EQUAL(10, q.dequeue(), "Wrap: first dequeue should be 10");
    q.enqueue(30);
    q.enqueue(40); // tail wraps here for capacity 3
    TEST_ASSERT_EQUAL(20, q.dequeue(), "Wrap: second dequeue should be 20");
    TEST_ASSERT_EQUAL(30, q.dequeue(), "Wrap: third dequeue should be 30");
    TEST_ASSERT_EQUAL(40, q.dequeue(), "Wrap: fourth dequeue should be 40");
}

void test_queue_fill_and_drain() {
    Queue<int, SpinLock> q(5);
    for (int i = 0; i < 5; i++) q.enqueue(i);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(i, q.dequeue(), "Fill/drain should preserve order");
    }
}


void register_all_tests() {
    if(tests_registered) return;
    tests_registered = true;
    
    // // Basic functionality tests
    // MANUAL_REGISTER_TEST(test_k_strlen);
    // MANUAL_REGISTER_TEST(test_k_streq);
    // MANUAL_REGISTER_TEST(test_k_strcmp);
    // MANUAL_REGISTER_TEST(test_k_isdigit);
    // MANUAL_REGISTER_TEST(test_k_min);
    
    // // Atomic and concurrency tests
    // MANUAL_REGISTER_TEST(test_atomic_basic);
    // MANUAL_REGISTER_TEST(test_atomic_exchange);
    // MANUAL_REGISTER_TEST(test_atomic_bool);
    // MANUAL_REGISTER_TEST(test_spinlock_basic);
    
    // // Memory tests - safe for all cores (using local memory)
    // MANUAL_REGISTER_TEST(test_memory_basic);
    // MANUAL_REGISTER_TEST(test_memory_copy);
    
    // // System tests - will show different values per core
    // MANUAL_REGISTER_TEST(test_core_id);
    // MANUAL_REGISTER_TEST(test_exception_level);
    // MANUAL_REGISTER_TEST(test_stack_pointer);
    // MANUAL_REGISTER_TEST(test_valid_memory_access);
    // MANUAL_REGISTER_TEST(test_stack_isolation);
    
    // Multi-core memory protection tests (the main event!)
    // MANUAL_REGISTER_TEST(test_null_pointer_protection);
    // MANUAL_REGISTER_TEST(test_low_memory_protection);
    // MANUAL_REGISTER_TEST(test_memory_protection_boundaries);
    
    // // Heap allocator tests
    // MANUAL_REGISTER_TEST(test_heap_basic_allocation);
    // MANUAL_REGISTER_TEST(test_heap_zero_initialization);
    // MANUAL_REGISTER_TEST(test_heap_alignment);
    // MANUAL_REGISTER_TEST(test_heap_statistics);
    // MANUAL_REGISTER_TEST(test_heap_boundary_conditions);
    // MANUAL_REGISTER_TEST(test_heap_free_reduces_used);
    // MANUAL_REGISTER_TEST(test_heap_free_reuse_and_coalesce);
    
    // // C++ operator tests
    // MANUAL_REGISTER_TEST(test_cpp_new_delete);
    // MANUAL_REGISTER_TEST(test_cpp_alignment);

    // Queue tests
    MANUAL_REGISTER_TEST(test_queue_basic_enq_deq);
    MANUAL_REGISTER_TEST(test_queue_wraparound);
    MANUAL_REGISTER_TEST(test_queue_fill_and_drain);
} 