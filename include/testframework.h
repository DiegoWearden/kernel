#ifndef _TEST_H_
#define _TEST_H_

#include "printf.h"
#include "stdint.h"

// Test result structure
struct TestResult {
    const char* test_name;
    bool passed;
    const char* failure_reason;
    int line_number;
    const char* file_name;
};

// Test function type
typedef void (*TestFunction)();

// Test registry entry
struct TestEntry {
    const char* name;
    TestFunction function;
    TestEntry* next;
};

class TestFramework {
private:
    static TestEntry* first_test;
    static TestResult current_result;
    static int total_tests;
    static int passed_tests;
    static int failed_tests;
    static bool test_running;

public:
    // Register a test
    static void register_test(const char* name, TestFunction function);
    
    // Run all registered tests
    static void run_all_tests();
    
    // Run a specific test by name
    static bool run_test(const char* name);
    
    // Assert functions for use in tests
    static void assert_true(bool condition, const char* message, 
                           const char* file, int line);
    static void assert_false(bool condition, const char* message, 
                            const char* file, int line);
    static void assert_equal(int expected, int actual, const char* message, 
                            const char* file, int line);
    static void assert_not_equal(int expected, int actual, const char* message, 
                                const char* file, int line);
    static void assert_null(void* ptr, const char* message, 
                           const char* file, int line);
    static void assert_not_null(void* ptr, const char* message, 
                                const char* file, int line);
    static void assert_string_equal(const char* expected, const char* actual, 
                                   const char* message, const char* file, int line);
    
    // Special assertion for testing memory access (expects page fault/exception)
    static void assert_memory_access_fails(void* ptr, const char* message,
                                          const char* file, int line);
    
    // Print test statistics
    static void print_summary();
    
    // Mark current test as failed
    static void fail_test(const char* reason, const char* file, int line);
    
    // Helper for null pointer protection tests
    static bool test_null_pointer_protection(uint64_t address, uint64_t value);
};

// Convenience macros for assertions
#define TEST_ASSERT_TRUE(condition, message) \
    TestFramework::assert_true((condition), (message), __FILE__, __LINE__)

#define TEST_ASSERT_FALSE(condition, message) \
    TestFramework::assert_false((condition), (message), __FILE__, __LINE__)

#define TEST_ASSERT_EQUAL(expected, actual, message) \
    TestFramework::assert_equal((expected), (actual), (message), __FILE__, __LINE__)

#define TEST_ASSERT_NOT_EQUAL(expected, actual, message) \
    TestFramework::assert_not_equal((expected), (actual), (message), __FILE__, __LINE__)

#define TEST_ASSERT_NULL(ptr, message) \
    TestFramework::assert_null((ptr), (message), __FILE__, __LINE__)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TestFramework::assert_not_null((ptr), (message), __FILE__, __LINE__)

#define TEST_ASSERT_STRING_EQUAL(expected, actual, message) \
    TestFramework::assert_string_equal((expected), (actual), (message), __FILE__, __LINE__)

#define TEST_ASSERT_MEMORY_ACCESS_FAILS(ptr, message) \
    TestFramework::assert_memory_access_fails((ptr), (message), __FILE__, __LINE__)

#define TEST_FAIL(message) \
    TestFramework::fail_test((message), __FILE__, __LINE__)

// Macro for manual test registration (suitable for kernel environment)
#define MANUAL_REGISTER_TEST(test_function) \
    TestFramework::register_test(#test_function, test_function)

#endif // _TEST_H_ 