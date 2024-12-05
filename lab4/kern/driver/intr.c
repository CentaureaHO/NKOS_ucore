#include <intr.h>
#include <riscv.h>

/* intr_enable - enable irq interrupt 将SIE置位 */
void intr_enable(void) { set_csr(sstatus, SSTATUS_SIE); }

/* intr_disable - disable irq interrupt 将SIE清零*/
void intr_disable(void) { clear_csr(sstatus, SSTATUS_SIE); }
