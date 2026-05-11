#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>

// Maximum number of concurrent tasks
#define MAX_TASKS        8

// Stack size per task (16 KB)
#define TASK_STACK_SIZE  (16 * 1024)

// Task states
typedef enum {
    TASK_UNUSED  = 0,
    TASK_READY   = 1,
    TASK_RUNNING = 2,
    TASK_BLOCKED = 3,
    TASK_DEAD    = 4,
} task_state_t;

// Saved CPU context — layout must match context_switch.asm exactly
typedef struct {
    uint64_t r15;   // +0
    uint64_t r14;   // +8
    uint64_t r13;   // +16
    uint64_t r12;   // +24
    uint64_t rbx;   // +32
    uint64_t rbp;   // +40
    uint64_t rsp;   // +48  stack pointer
    uint64_t rip;   // +56  instruction pointer (entry / resume point)
} __attribute__((packed)) task_context_t;

// Task control block
typedef struct {
    task_context_t  ctx;            // Saved registers (must be first)
    task_state_t    state;
    uint32_t        id;
    uint64_t        stack_top;      // Highest address of this task's stack
    uint64_t        tick_count;
    char            name[32];
} task_t;

void scheduler_init(void);
int  scheduler_create_task(const char *name, void (*entry)(void));
void scheduler_tick(void);
void scheduler_dump(void);
void scheduler_start(void);
void scheduler_yield(void);
int  scheduler_current_id(void);

#endif
