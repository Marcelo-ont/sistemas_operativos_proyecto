#include "mlfq_scheduler.h"
#include "serial_com1.h"

static void copy_name(char *dst, const char *src) {
    size_t i = 0;

    if (src == 0) {
        dst[0] = '\0';
        return;
    }

    while (src[i] != '\0' && i < (MLFQ_NAME_LEN - 1)) {
        dst[i] = src[i];
        ++i;
    }

    dst[i] = '\0';
}

static void queue_push(run_queue_t *queue, process_t *proc) {
    proc->next = 0;
    proc->in_run_queue = true;

    if (queue->tail != 0) {
        queue->tail->next = proc;
    } else {
        queue->head = proc;
    }

    queue->tail = proc;
    queue->length++;
}

static process_t *queue_pop(run_queue_t *queue) {
    process_t *proc = queue->head;

    if (proc == 0) {
        return 0;
    }

    queue->head = proc->next;
    if (queue->head == 0) {
        queue->tail = 0;
    }

    proc->next = 0;
    proc->in_run_queue = false;
    queue->length--;
    return proc;
}

static bool queues_have_runnable(const mlfq_scheduler_t *sched) {
    size_t level;

    for (level = 0; level < MLFQ_LEVELS; ++level) {
        if (sched->queues[level].head != 0) {
            return true;
        }
    }

    return false;
}

static process_t *pick_next(mlfq_scheduler_t *sched) {
    size_t level;

    for (level = 0; level < MLFQ_LEVELS; ++level) {
        process_t *next = queue_pop(&sched->queues[level]);
        if (next != 0) {
            return next;
        }
    }

    return sched->idle;
}

static void append_text(char *buffer, size_t *offset, const char *text) {
    while (*text != '\0' && *offset < 255) {
        buffer[*offset] = *text;
        (*offset)++;
        text++;
    }

    buffer[*offset] = '\0';
}

static void append_uint(char *buffer, size_t *offset, unsigned value) {
    char digits[16];
    size_t count = 0;

    if (value == 0) {
        if (*offset < 255) {
            buffer[*offset] = '0';
            (*offset)++;
            buffer[*offset] = '\0';
        }
        return;
    }

    while (value > 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (count > 0 && *offset < 255) {
        buffer[*offset] = digits[--count];
        (*offset)++;
    }

    buffer[*offset] = '\0';
}

static void append_pid_list(char *buffer, size_t *offset, const run_queue_t *queue) {
    const process_t *cursor = queue->head;
    bool first = true;

    if (cursor == 0) {
        append_text(buffer, offset, "-");
        return;
    }

    while (cursor != 0 && *offset < 255) {
        if (!first) {
            append_text(buffer, offset, ",");
        }

        append_uint(buffer, offset, (unsigned)cursor->pid);
        first = false;
        cursor = cursor->next;
    }
}

static void reset_time_slice(mlfq_scheduler_t *sched, process_t *proc) {
    if (proc == 0) {
        return;
    }

    if (proc->is_idle) {
        proc->time_slice_remaining = sched->quantums[MLFQ_LEVELS - 1];
        return;
    }

    proc->time_slice_remaining = sched->quantums[proc->mlfq_level];
}

void process_init(process_t *proc,
                  int pid,
                  const char *name,
                  proc_entry_t entry,
                  void *arg,
                  bool is_idle) {
    proc->pid = pid;
    copy_name(proc->name, name);
    proc->state = PROC_UNUSED;
    proc->entry = entry;
    proc->arg = arg;
    proc->mlfq_level = is_idle ? (MLFQ_LEVELS - 1) : 0;
    proc->time_slice_remaining = 0;
    proc->in_run_queue = false;
    proc->is_idle = is_idle;
    proc->next = 0;
}

void mlfq_init(mlfq_scheduler_t *sched) {
    size_t level;

    for (level = 0; level < MLFQ_LEVELS; ++level) {
        sched->queues[level].head = 0;
        sched->queues[level].tail = 0;
        sched->queues[level].length = 0;
    }

    sched->current = 0;
    sched->idle = 0;
    sched->quantums[0] = MLFQ_Q0_QUANTUM;
    sched->quantums[1] = MLFQ_Q1_QUANTUM;
    sched->quantums[2] = MLFQ_Q2_QUANTUM;
}

void mlfq_set_idle_process(mlfq_scheduler_t *sched, process_t *idle_proc) {
    sched->idle = idle_proc;
    if (idle_proc != 0) {
        idle_proc->is_idle = true;
        idle_proc->state = PROC_RUNNABLE;
        idle_proc->mlfq_level = MLFQ_LEVELS - 1;
        idle_proc->in_run_queue = false;
        idle_proc->next = 0;
    }
}

void mlfq_add_new_process(mlfq_scheduler_t *sched, process_t *proc) {
    proc->state = PROC_RUNNABLE;
    proc->mlfq_level = 0;
    proc->time_slice_remaining = sched->quantums[0];

    if (!proc->in_run_queue) {
        queue_push(&sched->queues[0], proc);
    }
}

void mlfq_wake_process(mlfq_scheduler_t *sched, process_t *proc) {
    if (proc == 0 || proc->is_idle || proc->state == PROC_ZOMBIE) {
        return;
    }

    if (proc == sched->current || proc->in_run_queue) {
        return;
    }

    proc->state = PROC_RUNNABLE;
    reset_time_slice(sched, proc);
    queue_push(&sched->queues[proc->mlfq_level], proc);
}

void mlfq_schedule(mlfq_scheduler_t *sched) {
    process_t *prev = sched->current;
    process_t *next = pick_next(sched);

    sched->current = next;

    if (next != 0) {
        next->state = PROC_RUNNING;
        reset_time_slice(sched, next);
    }

    mlfq_dump_telemetry(sched);

    if (prev != next && next != 0) {
        arch_context_switch(prev, next);
    }
}

void mlfq_yield_current(mlfq_scheduler_t *sched) {
    process_t *current = sched->current;

    if (current == 0) {
        mlfq_schedule(sched);
        return;
    }

    if (!current->is_idle) {
        current->state = PROC_RUNNABLE;
        queue_push(&sched->queues[current->mlfq_level], current);
    }

    mlfq_schedule(sched);
}

void mlfq_block_current(mlfq_scheduler_t *sched) {
    process_t *current = sched->current;

    if (current != 0 && !current->is_idle) {
        current->state = PROC_SLEEPING;
    }

    mlfq_schedule(sched);
}

void mlfq_tick(mlfq_scheduler_t *sched) {
    process_t *current = sched->current;

    if (current == 0) {
        mlfq_schedule(sched);
        return;
    }

    if (current->is_idle) {
        if (queues_have_runnable(sched)) {
            mlfq_schedule(sched);
        }
        return;
    }

    if (current->state != PROC_RUNNING) {
        mlfq_schedule(sched);
        return;
    }

    if (current->time_slice_remaining > 0) {
        current->time_slice_remaining--;
    }

    if (current->time_slice_remaining == 0) {
        if (current->mlfq_level < (MLFQ_LEVELS - 1)) {
            current->mlfq_level++;
        }

        current->state = PROC_RUNNABLE;
        queue_push(&sched->queues[current->mlfq_level], current);
        mlfq_schedule(sched);
    }
}

process_t *mlfq_current(const mlfq_scheduler_t *sched) {
    return sched->current;
}

void mlfq_dump_telemetry(const mlfq_scheduler_t *sched) {
    char buffer[256];
    size_t offset = 0;
    size_t level;

    buffer[0] = '\0';
    append_text(buffer, &offset, "MLFQ|RUN:");

    if (sched->current == 0 || sched->current->is_idle) {
        append_text(buffer, &offset, "-");
    } else {
        append_uint(buffer, &offset, (unsigned)sched->current->pid);
    }

    for (level = 0; level < MLFQ_LEVELS; ++level) {
        append_text(buffer, &offset, "|Q");
        append_uint(buffer, &offset, (unsigned)level);
        append_text(buffer, &offset, ":");
        append_pid_list(buffer, &offset, &sched->queues[level]);
    }

    append_text(buffer, &offset, "\n");
    com1_write_str(buffer);
}
