#include "testframework.h"
#include "mailbox.h"
#include "vm.h"
#include "stdint.h"

// Basic alignment test for the global mailbox buffer
static void test_mailbox_buffer_alignment() {
    TEST_ASSERT_EQUAL(0, ((uintptr_t)mbox & 0xFULL), "Mailbox buffer must be 16-byte aligned");
}

// Verify mailbox register addresses are 4-byte aligned
static void test_mailbox_register_alignment() {
    TEST_ASSERT_EQUAL(0, ((uintptr_t)MAILBOX_READ) & 3, "MAILBOX_READ must be 4-byte aligned");
    TEST_ASSERT_EQUAL(0, ((uintptr_t)MAILBOX_WRITE) & 3, "MAILBOX_WRITE must be 4-byte aligned");
    TEST_ASSERT_EQUAL(0, ((uintptr_t)MAILBOX_STATUS) & 3, "MAILBOX_STATUS must be 4-byte aligned");
}

// Sanity-check mailbox macro constants
static void test_mailbox_macro_constants() {
    TEST_ASSERT_NOT_EQUAL(0, (int)MAILBOX_FULL, "MAILBOX_FULL should be non-zero");
    TEST_ASSERT_NOT_EQUAL(0, (int)MAILBOX_EMPTY, "MAILBOX_EMPTY should be non-zero");
    TEST_ASSERT_EQUAL(8, (int)MBOX_CH_PROP, "Property channel number should be 8");
}

// Always-on address conversion tests (should fail if unimplemented)
static void test_gpu_convert_address_low_va_literal() {
    void* va = (void*)(0x00001000ULL);
    unsigned int bus = gpu_convert_address(va);
    unsigned int expected = 0xC0000000u | 0x00001000u;
    TEST_ASSERT_EQUAL((int)expected, (int)bus, "Low VA should map to 0xC0000000 | VA");
}

static void test_gpu_convert_address_high_va_literal() {
    void* va = (void*)(VA_START + 0x00123000ULL);
    unsigned int bus = gpu_convert_address(va);
    unsigned int expected = 0xC0000000u | 0x00123000u;
    TEST_ASSERT_EQUAL((int)expected, (int)bus, "High VA should map to 0xC0000000 | (VA - VA_START)");
}

static void test_gpu_convert_address_zero_ptr() {
    void* va = (void*)(0x0ULL);
    unsigned int bus = gpu_convert_address(va);
    unsigned int expected = 0xC0000000u | 0x0u;
    TEST_ASSERT_EQUAL((int)expected, (int)bus, "Null VA should map to 0xC0000000");
}

static void test_gpu_convert_address_va_start_exact() {
    void* va = (void*)(VA_START);
    unsigned int bus = gpu_convert_address(va);
    unsigned int expected = 0xC0000000u | 0x0u;
    TEST_ASSERT_EQUAL((int)expected, (int)bus, "VA_START should map to 0xC0000000");
}

static void test_gpu_convert_address_unaligned() {
    void* va = (void*)(VA_START + 0x12345ULL);
    unsigned int bus = gpu_convert_address(va);
    unsigned int expected = 0xC0000000u | 0x12345u;
    TEST_ASSERT_EQUAL((int)expected, (int)bus, "Unaligned VA should preserve low bits and add 0xC0000000");
}

static void prepare_buffer_header(unsigned total_bytes) {
    // Zero out a reasonable window of the buffer used by our tests
    for (int i = 0; i < 16; i++) mbox[i] = 0;
    mbox[0] = total_bytes;
    mbox[1] = MBOX_REQUEST;
}

static void assert_property_success_basic() {
    TEST_ASSERT_EQUAL((int)0x80000000u, (int)mbox[1], "Property buffer success bit must be set in mbox[1]");
}

static void test_mailbox_prop_get_firmware_revision() {
    // Buffer: size(7*4), req(0), tag, vbuf_size(4), vlen(0), value, end
    prepare_buffer_header(7 * 4);
    mbox[2] = RPI_FIRMWARE_GET_FIRMWARE_REVISION;
    mbox[3] = 4;
    mbox[4] = 0;
    mbox[5] = 0;
    mbox[6] = MBOX_TAG_LAST;

    unsigned int ok = mbox_call(MBOX_CH_PROP);
    TEST_ASSERT_TRUE(ok != 0, "mbox_call should succeed (GetFirmwareRevision)");
    assert_property_success_basic();
    TEST_ASSERT_TRUE((mbox[4] & 0x80000000u) != 0, "GetFirmwareRevision tag not supported or failed");
    TEST_ASSERT_EQUAL(4, (int)(mbox[3]), "GetFirmwareRevision value buffer size should be 4");
    TEST_ASSERT_NOT_EQUAL(0, (int)mbox[5], "Firmware revision should be non-zero");
}

static void test_mailbox_prop_get_board_model() {
    // Buffer: size(7*4), req(0), tag, vbuf_size(4), vlen(0), value, end
    prepare_buffer_header(7 * 4);
    mbox[2] = RPI_FIRMWARE_GET_BOARD_MODEL;
    mbox[3] = 4;
    mbox[4] = 0;
    mbox[5] = 0;
    mbox[6] = MBOX_TAG_LAST;

    unsigned int ok = mbox_call(MBOX_CH_PROP);
    TEST_ASSERT_TRUE(ok != 0, "mbox_call should succeed (GetBoardModel)");
    assert_property_success_basic();

    // Tag-level success: response bit in length and correct size
    TEST_ASSERT_TRUE((mbox[4] & 0x80000000u) != 0, "GetBoardModel tag not supported or failed");
    TEST_ASSERT_EQUAL(4, (int)(mbox[3]), "GetBoardModel value buffer size should be 4");

    if (mbox[5] == 0) {
        // Some firmware returns 0 for model; verify property channel works by checking revision
        prepare_buffer_header(7 * 4);
        mbox[2] = RPI_FIRMWARE_GET_BOARD_REVISION;
        mbox[3] = 4;
        mbox[4] = 0;
        mbox[5] = 0;
        mbox[6] = MBOX_TAG_LAST;

        ok = mbox_call(MBOX_CH_PROP);
        TEST_ASSERT_TRUE(ok != 0, "mbox_call should succeed (GetBoardRevision as fallback)");
        assert_property_success_basic();
        TEST_ASSERT_TRUE((mbox[4] & 0x80000000u) != 0, "GetBoardRevision tag failed");
        TEST_ASSERT_NOT_EQUAL(0, (int)mbox[5], "Board revision should be non-zero");
        return; // Accept as pass since firmware reports model=0
    }

    TEST_ASSERT_NOT_EQUAL(0, (int)mbox[5], "Board model should be non-zero");
}

static void test_mailbox_prop_get_board_revision() {
    // Buffer: size(7*4), req(0), tag, vbuf_size(4), vlen(0), value, end
    prepare_buffer_header(7 * 4);
    mbox[2] = RPI_FIRMWARE_GET_BOARD_REVISION;
    mbox[3] = 4;
    mbox[4] = 0;
    mbox[5] = 0;
    mbox[6] = MBOX_TAG_LAST;

    unsigned int ok = mbox_call(MBOX_CH_PROP);
    TEST_ASSERT_TRUE(ok != 0, "mbox_call should succeed (GetBoardRevision)");
    assert_property_success_basic();
    TEST_ASSERT_TRUE((mbox[4] & 0x80000000u) != 0, "GetBoardRevision tag not supported or failed");
    TEST_ASSERT_EQUAL(4, (int)(mbox[3]), "GetBoardRevision value buffer size should be 4");
    TEST_ASSERT_NOT_EQUAL(0, (int)mbox[5], "Board revision should be non-zero");
}

static void test_mailbox_prop_get_board_serial() {
    // Buffer: size(8*4), req(0), tag, vbuf_size(8), vlen(0), lo, hi, end
    prepare_buffer_header(8 * 4);
    mbox[2] = RPI_FIRMWARE_GET_BOARD_SERIAL;
    mbox[3] = 8;
    mbox[4] = 0;
    mbox[5] = 0; // serial low
    mbox[6] = 0; // serial high
    mbox[7] = MBOX_TAG_LAST;

    unsigned int ok = mbox_call(MBOX_CH_PROP);
    TEST_ASSERT_TRUE(ok != 0, "mbox_call should succeed (GetBoardSerial)");
    assert_property_success_basic();
    TEST_ASSERT_TRUE((mbox[4] & 0x80000000u) != 0, "GetBoardSerial tag not supported or failed");
    TEST_ASSERT_EQUAL(8, (int)(mbox[3]), "GetBoardSerial value buffer size should be 8");
    TEST_ASSERT_TRUE((mbox[5] != 0) || (mbox[6] != 0), "At least one 32-bit half of serial should be non-zero");
}

extern "C" void register_mailbox_tests() {
    MANUAL_REGISTER_TEST(test_mailbox_buffer_alignment);
    MANUAL_REGISTER_TEST(test_mailbox_register_alignment);
    MANUAL_REGISTER_TEST(test_mailbox_macro_constants);

    // // Always-on address conversion tests
    MANUAL_REGISTER_TEST(test_gpu_convert_address_low_va_literal);
    MANUAL_REGISTER_TEST(test_gpu_convert_address_high_va_literal);
    MANUAL_REGISTER_TEST(test_gpu_convert_address_zero_ptr);
    MANUAL_REGISTER_TEST(test_gpu_convert_address_va_start_exact);
    MANUAL_REGISTER_TEST(test_gpu_convert_address_unaligned);

    // Actual mailbox property-channel calls
    MANUAL_REGISTER_TEST(test_mailbox_prop_get_firmware_revision);
    MANUAL_REGISTER_TEST(test_mailbox_prop_get_board_model);
    MANUAL_REGISTER_TEST(test_mailbox_prop_get_board_revision);
    MANUAL_REGISTER_TEST(test_mailbox_prop_get_board_serial);
} 