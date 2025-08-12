#include "context.h"
#include "queue.h"


struct TCB;

extern TCB* current;
extern TCB*  boot_tcb;


extern "C" void trampoline();
extern "C" void context_switch(CpuContext* save, CpuContext* restore);

void schedInit();
void schedule(TCB* tcb);
void yield();