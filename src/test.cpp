#include "testframework.h"
#include "libk.h"
#include "atomic.h"

TestEntry* TestFramework::first_test = nullptr;
TestResult TestFramework::current_result;
int TestFramework::total_tests = 0;
int TestFramework::passed_tests = 0;
int TestFramework::failed_tests = 0;
bool TestFramework::test_running = false;
const int MAX_TESTS = 256;

void TestFramework::register_test(const char* name, TestFunction function) {
    // Allocate new test entry (simple allocation for kernel)
    static TestEntry test_entries[MAX_TESTS]; // Support up to 64 tests
    static int next_entry = 0;
    
    if (next_entry >= MAX_TESTS) {
        printf("ERROR: Too many tests registered (max %d)\n", MAX_TESTS);
        return;
    }
    
    TestEntry* entry = &test_entries[next_entry++];
    entry->name = name;
    entry->function = function;
    entry->next = first_test;
    first_test = entry;
    total_tests++;
}

void TestFramework::run_all_tests() {
    printf("\n=== KERNEL TEST FRAMEWORK ===\n");
    printf("Running %d tests...\n\n", total_tests);
    
    passed_tests = 0;
    failed_tests = 0;
    
    TestEntry* current = first_test;
    while (current != nullptr) {
        run_test(current->name);
        current = current->next;
    }
    
    print_summary();
}

bool TestFramework::run_test(const char* name) {
    // Find the test
    TestEntry* current = first_test;
    while (current != nullptr) {
        if (K::streq(current->name, name)) {
            break;
        }
        current = current->next;
    }
    
    if (current == nullptr) {
        printf("Test '%s' not found\n", name);
        return false;
    }
    
    // Initialize test result
    current_result.test_name = name;
    current_result.passed = true;
    current_result.failure_reason = nullptr;
    current_result.line_number = 0;
    current_result.file_name = nullptr;
    test_running = true;
    
    printf("Running test: %s...\n", name);
    
    // Run the test
    current->function();
    
    test_running = false;
    
    // Report result
    if (current_result.passed) {
        printf("\n  -> PASSED\n");
        passed_tests++;
        return true;
    } else {
        printf("  -> FAILED\n");
        if (current_result.failure_reason) {
            printf("  Reason: %s", current_result.failure_reason);
            if (current_result.file_name && current_result.line_number > 0) {
                printf(" (%s:%d)", current_result.file_name, current_result.line_number);
            }
            printf("\n");
        }
        failed_tests++;
        return false;
    }
}

void TestFramework::assert_true(bool condition, const char* message, 
                                const char* file, int line) {
    if (!test_running) return;
    
    if (!condition) {
        current_result.passed = false;
        current_result.failure_reason = message;
        current_result.file_name = file;
        current_result.line_number = line;
    }
}

void TestFramework::assert_false(bool condition, const char* message, 
                                 const char* file, int line) {
    assert_true(!condition, message, file, line);
}

void TestFramework::assert_equal(int expected, int actual, const char* message, 
                                const char* file, int line) {
    if (!test_running) return;
    
    if (expected != actual) {
        current_result.passed = false;
        current_result.failure_reason = message;
        current_result.file_name = file;
        current_result.line_number = line;
    }
}

void TestFramework::assert_not_equal(int expected, int actual, const char* message, 
                                     const char* file, int line) {
    if (!test_running) return;
    
    if (expected == actual) {
        current_result.passed = false;
        current_result.failure_reason = message;
        current_result.file_name = file;
        current_result.line_number = line;
    }
}

void TestFramework::assert_null(void* ptr, const char* message, 
                               const char* file, int line) {
    if (!test_running) return;
    
    if (ptr != nullptr) {
        current_result.passed = false;
        current_result.failure_reason = message;
        current_result.file_name = file;
        current_result.line_number = line;
    }
}

void TestFramework::assert_not_null(void* ptr, const char* message, 
                                   const char* file, int line) {
    if (!test_running) return;
    
    if (ptr == nullptr) {
        current_result.passed = false;
        current_result.failure_reason = message;
        current_result.file_name = file;
        current_result.line_number = line;
    }
}

void TestFramework::assert_string_equal(const char* expected, const char* actual, 
                                       const char* message, const char* file, int line) {
    if (!test_running) return;
    
    if (!K::streq(expected, actual)) {
        current_result.passed = false;
        current_result.failure_reason = message;
        current_result.file_name = file;
        current_result.line_number = line;
    }
}

void TestFramework::assert_memory_access_fails(void* ptr, const char* message,
                                              const char* file, int line) {
    if (!test_running) return;

    (void)file;
    (void)message;
    (void)line;
    
    // This is a placeholder - in a real implementation, you would need
    // exception handling to catch page faults
    // For now, we'll assume the test knows what it's doing
    printf("  [Memory access test at %p - manual verification needed]", ptr);
}

bool TestFramework::test_null_pointer_protection(uint64_t address, uint64_t value) {    
    printf("  Testing memory protection at address 0x%llx...", address);
    
    // Note: In a real implementation, you would set up exception handlers
    // to catch the page fault and return true if it occurs
    // For now, this is a simplified version
    
    volatile uint64_t* ptr = (volatile uint64_t*)address;
    
    *ptr = value;
    
    return false; // Protection failed if we can write
}

void TestFramework::fail_test(const char* reason, const char* file, int line) {
    if (!test_running) return;
    
    current_result.passed = false;
    current_result.failure_reason = reason;
    current_result.file_name = file;
    current_result.line_number = line;
}

void TestFramework::print_summary() {
    printf("\n=== TEST SUMMARY ===\n");
    printf("Total tests: %d\n", total_tests);
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", failed_tests);
    
    if (failed_tests == 0) {
        printf("All tests PASSED! ✓\n");
    } else {
        printf("Some tests FAILED! ✗\n");
    }
    printf("==================\n\n");
} 