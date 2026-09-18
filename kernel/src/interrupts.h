#ifndef JOSHOS_INTERRUPTS_H
#define JOSHOS_INTERRUPTS_H

void interrupts_init(void);

#ifdef JOSHOS_FAULT_TEST_DOUBLE_FAULT
void interrupts_arm_double_fault_test(void);
#endif

#endif
