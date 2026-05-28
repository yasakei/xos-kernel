// -------------------------------------------------------------------
// mit license
// 
// copyright (c) 2026 xos
// 
// permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "software"), to deal in the software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the software, and to permit persons to whom the
// software is furnished to do so, subject to the following
// conditions:
// 
// the above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the software.
// 
// the software is provided "as is", without warranty of any kind,
// express or implied, including but not limited to the warranties
// of merchantability, fitness for a particular purpose and
// noninfringement. in no event shall the authors or copyright
// holders be liable for any claim, damages or other liability,
// whether in an action of contract, tort or otherwise, arising
// from, out of or in connection with the software or the use or
// other dealings in the software.
// -------------------------------------------------------------------

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>

// maximum number of concurrent tasks
#define MAX_TASKS        8

// stack size per task. four pages gives enough room for c call depth,
// printf-heavy paths, and interrupt entry frames.
#define TASK_STACK_SIZE  (16 * 1024)

// task states
typedef enum {
    TASK_UNUSED  = 0,
    TASK_READY   = 1,
    TASK_RUNNING = 2,
    TASK_BLOCKED = 3,
    TASK_DEAD    = 4,
} task_state_t;

// saved cpu context — layout must match context_switch.asm exactly
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

// task control block
typedef struct {
    task_context_t  ctx;            // saved registers (must be first)
    task_state_t    state;
    uint32_t        id;
    uint64_t        stack_top;      // highest address of this task's stack
    uint64_t        user_stack_bottom;
    uint64_t        user_stack_top;
    uint64_t        user_entry;
    uint8_t         is_user;
    uint64_t        tick_count;
    char            name[32];
} task_t;

// initialize the scheduler
void scheduler_init(void);

// create a new kernel task
int  scheduler_create_task(const char *name, void (*entry)(void));

// create a new user (ring-3) task
int  scheduler_create_user_task(const char *name, void (*entry)(void));

// called on each timer tick - potentially switches tasks
void scheduler_tick(void);

// dump current task info via printf
void scheduler_dump(void);

// start the scheduler (takes over execution)
void scheduler_start(void);

// manually yield the current task
void scheduler_yield(void);

// get the id of the currently running task
int  scheduler_current_id(void);

// get a pointer to the currently running task
task_t *scheduler_current_task(void);

#endif
