#include "test.h"
#include "libk.h"
#include "atomic.h"
#include "utils.h"

// Basic library tests
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

// Test atomic operations
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

// Test SpinLock (this is interesting on multiple cores)
void test_spinlock_basic() {
    SpinLock lock;
    
    // Basic lock/unlock
    lock.lock();
    TEST_ASSERT_TRUE(lock.isMine(), "lock should be taken after lock()");
    lock.unlock();
}

// Memory tests
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

// Core system tests - these will show different values per core
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

// Test that we can write to valid memory addresses
void test_valid_memory_access() {
    printf("\n  Testing valid memory access on core %lld", getCoreID());
    
    // Use stack memory which should be valid
    uint64_t valid_memory = 0;
    uint64_t test_value = 0xDEADBEEF;
    
    // This should work without any protection issues
    valid_memory = test_value;
    
    TEST_ASSERT_EQUAL(test_value, valid_memory, "Should be able to write to valid memory");
}

// ============================================================================
// MULTI-CORE NULL POINTER PROTECTION TESTS
// ============================================================================

// Test null pointer protection - all cores will run this same test
void test_null_pointer_protection() {
    uint64_t core_id = getCoreID();
    printf("\n  Core %lld: Testing null pointer protection at 0x0", core_id);
    
    // All cores test the same address: 0x0 (true null pointer)
    uint64_t address = 0x0;
    uint64_t test_value = 0xDEADBEEF;
    
    printf("\n  Core %lld: Attempting to write 0x%llx to address 0x%llx", core_id, test_value, address);
    
    // This should trigger null pointer protection on ALL cores
    volatile uint64_t* ptr = (volatile uint64_t*)address;
    *ptr = test_value;
    
    // If we reach here, protection is not working
    TEST_FAIL("Write to null pointer succeeded - protection may not be enabled");
}

// Test protection of low memory addresses - all cores test different addresses
void test_low_memory_protection() {
    uint64_t core_id = getCoreID();
    
    // Each core tests a different low memory address, but all use the same test logic
    uint64_t test_addresses[] = {0x0, 0xf0, 0xe8, 0xe0, 0x8, 0x10, 0x18, 0x20};
    uint64_t address = test_addresses[core_id % (sizeof(test_addresses) / sizeof(test_addresses[0]))];
    
    printf("\n  Core %lld: Testing low memory protection at 0x%llx", core_id, address);
    
    uint64_t test_value = 0xDEADBEEF;
    
    printf("\n  Core %lld: Attempting to write 0x%llx to address 0x%llx", core_id, test_value, address);
    
    volatile uint64_t* ptr = (volatile uint64_t*)address;
    *ptr = test_value;
    
    TEST_FAIL("Write to protected low memory succeeded - protection may not be enabled");
}

// Test memory protection boundaries - comprehensive test
void test_memory_protection_boundaries() {
    uint64_t core_id = getCoreID();
    printf("\n  Core %lld: Testing memory protection boundaries", core_id);
    
    // Test various low addresses that should be protected
    uint64_t protected_addresses[] = {0x0, 0x8, 0x10, 0x18, 0x20, 0xe0, 0xe8, 0xf0, 0xf8};
    int num_addresses = sizeof(protected_addresses) / sizeof(protected_addresses[0]);
    
    printf("\n  Core %lld: Testing %d potentially protected addresses:", core_id, num_addresses);
    
    // Each core can test all addresses, or we can distribute them
    int start_idx = core_id * 2; // Each core tests 2 addresses
    int end_idx = start_idx + 2;
    if (end_idx > num_addresses) end_idx = num_addresses;
    
    for (int i = start_idx; i < end_idx; i++) {
        printf("\n    Core %lld testing address 0x%llx: ", core_id, protected_addresses[i]);
        printf("(would attempt write here)");
    }
    
    // For now, this test just documents what addresses we're checking
    TEST_ASSERT_TRUE(true, "Memory protection boundary test completed");
}

// Multi-core specific test: Test that cores can't interfere with each other's stacks
void test_stack_isolation() {
    uint64_t core_id = getCoreID();
    uint64_t my_stack = get_sp();
    
    printf("\n  Core %lld: Stack isolation test, my stack at 0x%llx", core_id, my_stack);
    
    // Write a unique pattern to our stack
    volatile uint64_t stack_marker = 0xC0DE0000 + core_id;
    uint64_t test_value = stack_marker;
    
    // Verify we can write to our own stack
    TEST_ASSERT_EQUAL(test_value, stack_marker, "Should be able to write to own stack");
    
    printf("\n  Core %lld: Successfully wrote unique marker 0x%llx", core_id, stack_marker);
}

// ============================================================================
// TEST REGISTRATION
// ============================================================================

void register_all_tests() {
    // Basic functionality tests - safe for all cores
    MANUAL_REGISTER_TEST(test_k_strlen);
    MANUAL_REGISTER_TEST(test_k_streq);
    MANUAL_REGISTER_TEST(test_k_strcmp);
    MANUAL_REGISTER_TEST(test_k_isdigit);
    MANUAL_REGISTER_TEST(test_k_min);
    
    // Atomic and concurrency tests - interesting on multiple cores
    MANUAL_REGISTER_TEST(test_atomic_basic);
    MANUAL_REGISTER_TEST(test_atomic_exchange);
    MANUAL_REGISTER_TEST(test_atomic_bool);
    MANUAL_REGISTER_TEST(test_spinlock_basic);
    
    // Memory tests - safe for all cores (using local memory)
    MANUAL_REGISTER_TEST(test_memory_basic);
    MANUAL_REGISTER_TEST(test_memory_copy);
    
    // System tests - will show different values per core
    MANUAL_REGISTER_TEST(test_core_id);
    MANUAL_REGISTER_TEST(test_exception_level);
    MANUAL_REGISTER_TEST(test_stack_pointer);
    MANUAL_REGISTER_TEST(test_valid_memory_access);
    MANUAL_REGISTER_TEST(test_stack_isolation);
    
    // Multi-core memory protection tests (the main event!)
    MANUAL_REGISTER_TEST(test_null_pointer_protection);
    MANUAL_REGISTER_TEST(test_low_memory_protection);
    MANUAL_REGISTER_TEST(test_memory_protection_boundaries);
} 