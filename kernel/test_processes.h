#ifndef TEST_PROCESSES_H
#define TEST_PROCESSES_H

#include <stdint.h>

/*
 * Adapt these hooks to your kernel's existing scheduler/syscall layer.
 */
void yield(void);
void sleep_ticks(uint32_t ticks);

void cpu_bound_process(void *arg);
void io_bound_process(void *arg);

#endif
