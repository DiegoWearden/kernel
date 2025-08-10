
#include "mailbox.h"
#include "utils.h"
#include "dcache.h"
#include "vm.h"

// Define the global mailbox buffer (16-byte aligned)
volatile unsigned int mbox[35] __attribute__((aligned(16), section(".mailbox")));

extern "C" void clear_mailbox(void) {
    while(!(*MAILBOX_STATUS & MAILBOX_EMPTY)){
        (void)*MAILBOX_READ;
    }
}

extern "C" void mailbox_write(unsigned char channel, unsigned int value) {
    while(*MAILBOX_STATUS & MAILBOX_FULL);
    if(channel == MBOX_CH_PROP){
        value = gpu_convert_address((void*)(uintptr_t)value);
    }
    *MAILBOX_WRITE = (value & ~0xF) | (channel & 0xF);
}

extern "C" unsigned int mailbox_read(unsigned char channel) {
    while(1){
        while(*MAILBOX_STATUS & MAILBOX_EMPTY);
        unsigned int response = *MAILBOX_READ;
        if((response & 0xF) == channel){
            return response & ~0xF;
        }
    }
    return 0;
}

extern "C" unsigned int gpu_convert_address(void* addr) {
    unsigned long p = (unsigned long)addr;
    if(p >= VA_START){
        p -= VA_START;
    }
    // set gpu alias as 0xC0000000 which is the direct uncached alias, look at page 5 of
    // https://datasheets.raspberrypi.com/bcm2835/bcm2835-peripherals.pdf for more details
    return p | 0xC0000000u;
}

extern "C" unsigned int mbox_call(unsigned char ch) {
    clean_dcache_range((void*)mbox, sizeof(mbox));
    clear_mailbox();
    unsigned int value = gpu_convert_address((void*)mbox);
    mailbox_write(ch, value);
    unsigned int response = mailbox_read(ch);
    invalidate_dcache_range((void*)mbox, sizeof(mbox));
    return response;
}