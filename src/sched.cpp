#include "tcb.h"
#include "sched.h"

TCB* current = nullptr;
TCB* boot_tcb = nullptr;

extern "C" void context_switch(CpuContext* save, CpuContext* restore);
extern "C" void trampoline();

extern "C" void trampoline(){
    current->get_thread().run();
    context_switch(current->get_context(), boot_tcb->get_context());
}

void start_thread(TCB& tcb){
    current = &tcb;
    context_switch(boot_tcb->get_context(), current->get_context());
}

void yield(){
    context_switch(current->get_context(), boot_tcb->get_context());
}