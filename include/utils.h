#ifndef _BOOT_H
#define _BOOT_H

extern "C" void delay(unsigned long);
extern "C" void put32(volatile unsigned int*, unsigned int);
extern "C" unsigned int get32(volatile unsigned int*);
extern "C" unsigned long getCoreID();
extern "C" unsigned long get_el();
extern "C" unsigned long get_sp();

extern int onHypervisor;

#endif /*_BOOT_H */
