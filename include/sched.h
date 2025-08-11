#include "context.h"


struct TCB;
extern TCB* current;
extern TCB*  boot_tcb;


extern "C" void trampoline();
extern "C" void context_switch(CpuContext* save, CpuContext* restore);

void start_thread(TCB& tcb);
void yield();