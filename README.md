# MLFQ Kernel Reference

This scaffold gives you:

- A 3-level MLFQ scheduler in C with `Q0`, `Q1`, and `Q2`
- CPU-bound and I/O-bound test tasks
- COM1 telemetry on every scheduling decision
- A `tkinter` host visualizer that reads from `stdin`

## File Layout

- `kernel/mlfq_scheduler.h`
- `kernel/mlfq_scheduler.c`
- `kernel/serial_com1.h`
- `kernel/test_processes.h`
- `kernel/test_processes.c`
- `host/mlfq_visualizer.py`

## Scheduler Behavior

- `Q0` quantum: `2` ticks
- `Q1` quantum: `4` ticks
- `Q2` quantum: `8` ticks
- New processes always start in `Q0`
- If a process uses its full slice, `mlfq_tick()` demotes it one level
- If a process yields or blocks before its slice ends, it keeps its current level

## Kernel Integration

1. Initialize the serial port once during boot:

```c
com1_init();
```

2. Initialize the scheduler and an idle thread:

```c
static mlfq_scheduler_t g_sched;
static process_t idle_proc;
static process_t cpu_proc;
static process_t io_proc;

process_init(&idle_proc, 0, "idle", idle_thread, 0, true);
process_init(&cpu_proc, 1, "cpu-bound", cpu_bound_process, 0, false);
process_init(&io_proc, 2, "io-bound", io_bound_process, 0, false);

mlfq_init(&g_sched);
mlfq_set_idle_process(&g_sched, &idle_proc);
mlfq_add_new_process(&g_sched, &cpu_proc);
mlfq_add_new_process(&g_sched, &io_proc);
mlfq_schedule(&g_sched);
```

3. Call the scheduler hooks from your kernel:

- Timer interrupt: `mlfq_tick(&g_sched);`
- Voluntary yield syscall: `mlfq_yield_current(&g_sched);`
- Before blocking on sleep or I/O: `mlfq_block_current(&g_sched);`
- When an I/O event wakes a task: `mlfq_wake_process(&g_sched, proc);`

4. Provide your architecture-specific context switch routine:

```c
void arch_context_switch(process_t *prev, process_t *next);
```

If your kernel defers switches until it exits the interrupt handler, keep the
same policy logic but replace the direct `arch_context_switch()` call with a
`need_resched` flag.

## Test Tasks

- `cpu_bound_process()` does a long arithmetic loop and never yields, so timer
  preemption will move it from `Q0` to `Q1` to `Q2`.
- `io_bound_process()` does a tiny burst of work and then calls
  `sleep_ticks(2)`, so it gives up the CPU early and tends to remain in
  `Q0` or `Q1`.

## Telemetry Format

Every schedule emits one line on COM1:

```text
MLFQ|RUN:2|Q0:1|Q1:-|Q2:3,4
```

The GUI accepts the requested queue-only format too:

```text
MLFQ|Q0:1|Q1:2|Q2:3,4
```

## Host Visualizer

Run the visualizer like this:

```bash
python3 host/mlfq_visualizer.py
```

It waits for lines on `stdin`, parses the `MLFQ|...` telemetry, and animates
each PID into the `RUN`, `Q0`, `Q1`, and `Q2` lanes.

## Exact QEMU Pipeline For macOS

If your OS image is `build/os-image.bin`, use:

```bash
qemu-system-i386 -drive format=raw,file=build/os-image.bin,if=ide -monitor none -serial stdio | python3 host/mlfq_visualizer.py
```

Notes:

- `-monitor none` keeps the terminal clean so only COM1 bytes flow to Python
- `-serial stdio` sends COM1 directly to the pipe
- If you boot with `-kernel` instead of a raw disk image, keep the same serial
  flags and only swap the boot-image portion of the command
# sistemas_operativos_proyecto
