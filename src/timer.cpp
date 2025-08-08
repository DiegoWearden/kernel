#include "timer.h"
#include "printf.h"
#include "stdint.h"
#include "utils.h"

// QA7 (ARM Local) base (legacy)
static constexpr uint64_t QA7_BASE = 0x40000000ULL; // legacy local peripherals
// Offsets (commonly documented for BCM2836/QA7)
static constexpr uint64_t QA7_CTRL_STATUS      = QA7_BASE + 0x34; // Local Timer Control/Status
static constexpr uint64_t QA7_RELOAD           = QA7_BASE + 0x38; // Local Timer Reload
static constexpr uint64_t QA7_IRQ_CTRL_BASE    = QA7_BASE + 0x40; // Local Timer IRQ Control/Status (core0..3 at +0x0,+0x4,+0x8,+0xC)
static constexpr uint64_t QA7_CNT_LO           = QA7_BASE + 0x1C; // Local Timer count low (optional)
static constexpr uint64_t QA7_CNT_HI           = QA7_BASE + 0x20; // Local Timer count high (optional)

// Bits
static constexpr uint32_t QA7_CTRL_ENABLE      = (1u << 28); // enable timer
static constexpr uint32_t QA7_CTRL_INT_ENABLE  = (1u << 29); // enable IRQ
static constexpr uint32_t QA7_CTRL_IRQ_PENDING = (1u << 31); // write 1 to clear
static constexpr uint32_t QA7_CTRL_VALUE_MASK  = 0x0FFFFFFFu; // value/reload field mask (assume 28 bits)

static inline void mmio_write(uint64_t addr, uint32_t val) {
    *(volatile uint32_t*)addr = val;
}
static inline uint32_t mmio_read(uint64_t addr) {
    return *(volatile uint32_t*)addr;
}

static inline uint64_t per_core_irq_ctrl_addr() {
    return QA7_IRQ_CTRL_BASE + (getCoreID() * 4);
}

// crude tick counter incremented from IRQ
static volatile uint64_t g_ticks = 0;
static volatile int dbg_budget[4] = {1,1,1,1};

extern "C" void timer_irq_handler()
{
    // Acknowledge interrupt on this core
    mmio_write(per_core_irq_ctrl_addr(), 1);
    // Clear on control/status
    mmio_write(QA7_CTRL_STATUS, QA7_CTRL_IRQ_PENDING);
    g_ticks++;
    int cid = (int)getCoreID();
    if (dbg_budget[cid] > 0) {
        printf("tick core=%d\n", cid);
        dbg_budget[cid]--;
    }
}

void timer_init_qA7(uint32_t reload_us)
{
    if (reload_us == 0) reload_us = 1;
    uint32_t val = reload_us & QA7_CTRL_VALUE_MASK;

    // Program reload
    mmio_write(QA7_RELOAD, val);

    // Route/enable IRQ on this core (bit0)
    mmio_write(per_core_irq_ctrl_addr(), 0x1);

    // Enable timer and interrupt, set current value field
    uint32_t ctrl = (val & QA7_CTRL_VALUE_MASK) | QA7_CTRL_ENABLE | QA7_CTRL_INT_ENABLE;
    mmio_write(QA7_CTRL_STATUS, ctrl);

    printf("QA7 timer initialized: reload=%u us\n", reload_us);
}

uint64_t timer_ticks()
{
    return g_ticks;
} 