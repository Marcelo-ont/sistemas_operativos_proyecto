#ifndef MLFQ_SCHEDULER_H
#define MLFQ_SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MLFQ_LEVELS 3
#define MLFQ_Q0_QUANTUM 2
#define MLFQ_Q1_QUANTUM 4
#define MLFQ_Q2_QUANTUM 8
#define MLFQ_NAME_LEN 32

typedef enum {
    PROC_UNUSED = 0,
    PROC_RUNNABLE,
    PROC_RUNNING,
    PROC_SLEEPING,
    PROC_ZOMBIE
} proc_state_t;

typedef void (*proc_entry_t)(void *arg);

typedef struct process {
    int pid;
    char name[MLFQ_NAME_LEN];
    proc_state_t state;
    proc_entry_t entry;
    void *arg;
    uint8_t mlfq_level;
    uint8_t time_slice_remaining;
    bool in_run_queue;
    bool is_idle;
    struct process *next;
} process_t;

typedef struct {
    process_t *head;
    process_t *tail;
    size_t length;
} run_queue_t;

typedef struct {
    run_queue_t queues[MLFQ_LEVELS];
    process_t *current;
    process_t *idle;
    uint8_t quantums[MLFQ_LEVELS];
} mlfq_scheduler_t;

void process_init(process_t *proc,
                  int pid,
                  const char *name,
                  proc_entry_t entry,
                  void *arg,
                  bool is_idle);

void mlfq_init(mlfq_scheduler_t *sched);
void mlfq_set_idle_process(mlfq_scheduler_t *sched, process_t *idle_proc);
void mlfq_add_new_process(mlfq_scheduler_t *sched, process_t *proc);
void mlfq_wake_process(mlfq_scheduler_t *sched, process_t *proc);
void mlfq_yield_current(mlfq_scheduler_t *sched);
void mlfq_block_current(mlfq_scheduler_t *sched);
void mlfq_tick(mlfq_scheduler_t *sched);
void mlfq_schedule(mlfq_scheduler_t *sched);
void mlfq_dump_telemetry(const mlfq_scheduler_t *sched);
process_t *mlfq_current(const mlfq_scheduler_t *sched);

/*
 * Supply this hook from your architecture layer. In many kernels it wraps the
 * low-level assembly routine that swaps saved register contexts.
 */
void arch_context_switch(process_t *prev, process_t *next);

#endif
