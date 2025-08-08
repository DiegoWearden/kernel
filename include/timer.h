#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void timer_init_qA7(uint32_t reload_us);
uint64_t timer_ticks();

#ifdef __cplusplus
}
#endif

#endif 