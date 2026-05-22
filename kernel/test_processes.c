#include "test_processes.h"

#include <stdint.h>

void cpu_bound_process(void *arg) {
    volatile uint64_t accumulator = (uint64_t)(uintptr_t)arg + 1;

    for (;;) {
        uint64_t i;

        for (i = 1; i < 500000; ++i) {
            accumulator += (i * 1103515245ULL) ^ (accumulator >> 3);
            accumulator ^= (accumulator << 7) | (accumulator >> 11);
            accumulator %= 1000000007ULL;
        }

        /*
         * Keep the compiler from optimizing the loop away while still never
         * yielding voluntarily. Timer interrupts should preempt this thread
         * and drive it down from Q0 to Q2.
         */
        if ((accumulator & 0xFFULL) == 0x42ULL) {
            accumulator ^= 0xA5A5A5A5ULL;
        }
    }
}

void io_bound_process(void *arg) {
    volatile uint32_t token = (uint32_t)(uintptr_t)arg;

    for (;;) {
        uint32_t i;

        for (i = 0; i < 3000; ++i) {
            token = (token + i) ^ (token << 1);
        }

        /*
         * This thread does a short burst of work and then blocks early, so it
         * keeps its high-priority slice instead of being demoted.
         */
        sleep_ticks(2);
    }
}
