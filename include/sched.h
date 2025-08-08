#ifndef _SCHED_H_
#define _SCHED_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*thread_entry_t)(void*);

void sched_init();
void sched_start_cpu();
int thread_create(thread_entry_t entry, void* arg, int preferred_cpu);
void sched_tick(uint64_t isr_sp);
void sched_yield();
void sched_exit();

#ifdef __cplusplus
}
#endif

#endif 