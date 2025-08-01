#ifndef _SYSREGS_H
#define _SYSREGS_H


#define SCTLR_MMU_ENABLED \
    (                     \
        (0xC00800) |      /* set mandatory reserved bits */              \
        (1 << 12) |       /* I: enable instruction cache */              \
        (1 << 4)  |       /* SA0: stack alignment faulting for EL0 */    \
        (1 << 3)  |       /* SA: stack alignment faulting */             \
        (1 << 2)  |       /* C: data cache enable */                     \
        (1 << 1)  |       /* A: alignment faulting */                    \
        (1 << 11) |       /* Z: program flow prediction */               \
        (1 << 0)          /* M: MMU enable */                            \
    )


#define HCR_RW (1 << 31)
#define HCR_VALUE HCR_RW


#define SCR_RESERVED (3 << 4)
#define SCR_RW (1 << 10)
#define SCR_NS (1 << 0)
#define SCR_VALUE (SCR_RESERVED | SCR_RW | SCR_NS)


#define SPSR_MASK_ALL (7 << 6)
#define SPSR_EL1h (5 << 0)
#define SPSR_VALUE (SPSR_MASK_ALL | SPSR_EL1h)

#endif