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

#include "scheduler.h"
#include "../arch/gdt.h"
#include "../arch/usermode.h"
#include "../lib/printf.h"
#include "../lib/debuglog.h"
#include "../mm/pmm.h"
#include <stdint.h>

// external assembly functions for context switching
extern void context_switch(task_context_t *old_ctx, task_context_t *new_ctx);
extern void scheduler_start_first_task(task_context_t *ctx);

// our task table and state tracking
static task_t tasks[MAX_TASKS];
static int current_task_id = -1;  // no task running initially
static int next_task_id = 0;      // for assigning unique task ids

// tiny bootstrapper that enters ring-3 for user tasks
static void user_task_bootstrap(void) {
    task_t *t = scheduler_current_task();
    if (!t || !t->is_user) {
        printf("[SCHED] user_task_bootstrap called without user task\n");
        while (1) { __asm__ volatile("hlt"); }
    }

    if (debug_print_is_enabled()) {
        printf("[SCHED] Bootstrapping user task %s (task=%d)\n", t->name, (int)t->id);
    }
    tss_set_kernel_stack(t->stack_top);
    usermode_enter(t->user_entry, t->user_stack_top);

    // usermode_enter should never return, but just in case...
    printf("[SCHED] user_task_bootstrap returned unexpectedly\n");
    while (1) { __asm__ volatile("hlt"); }
}

// find the next ready task using round-robin
static int find_next_ready_task(void) {
    int start = (current_task_id + 1) % MAX_TASKS;
    for (int i = 0; i < MAX_TASKS; i++) {
        int idx = (start + i) % MAX_TASKS;
        if (tasks[idx].state == TASK_READY) {
            return idx;
        }
    }
    return -1;  // no ready tasks found
}

// set up the task table and scheduler state
void scheduler_init(void) {
    if (debug_print_is_enabled()) {
        printf("[SCHED] Initializing scheduler...\n");
    }
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_UNUSED;
        tasks[i].id = 0;
        tasks[i].stack_top = 0;
        tasks[i].user_stack_bottom = 0;
        tasks[i].user_stack_top = 0;
        tasks[i].user_entry = 0;
        tasks[i].is_user = 0;
        tasks[i].tick_count = 0;
    }
    current_task_id = -1;
    next_task_id = 0;
    if (debug_print_is_enabled()) {
        printf("[SCHED] Scheduler ready (max %d tasks)\n", MAX_TASKS);
    }
}

// create a new kernel-mode task with its own 16kb stack
int scheduler_create_task(const char *name, void (*entry)(void)) {
    // find a free slot in the task table
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED || tasks[i].state == TASK_DEAD) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        printf("[SCHED] No free task slots\n");
        return -1;
    }

    task_t *t = &tasks[slot];
    t->id = next_task_id++;
    t->state = TASK_UNUSED;
    t->tick_count = 0;

    // copy the task name
    int i = 0;
    while (name[i] && i < 31) {
        t->name[i] = name[i];
        i++;
    }
    t->name[i] = '\0';

    // allocate a 16 kb kernel stack as four consecutive pages
    // the pmm allocates pages sequentially from low memory, so this is fine
    // for our identity-mapped kernel
    void *stack_bottom = pmm_alloc_page();
    if (!stack_bottom) {
        printf("[SCHED] Failed to allocate stack for task %s\n", name);
        return -1;
    }
    for (int p = 1; p < 4; p++) {
        if (!pmm_alloc_page()) {
            printf("[SCHED] Failed to allocate stack page %d for task %s\n", p, name);
            return -1;
        }
    }

    t->stack_top = (uint64_t)stack_bottom + TASK_STACK_SIZE;
    t->user_stack_bottom = 0;
    t->user_stack_top = 0;
    t->user_entry = 0;
    t->is_user = 0;

    // set up the initial stack frame
    // context_switch does: mov rsp,[ctx+48]; mov rax,[ctx+56]; mov [rsp],rax; ret
    // so we need rsp pointing at a writable slot, and rip = entry
    // we pre-allocate one slot at the top of the stack
    uint64_t *sp = (uint64_t *)t->stack_top;
    sp--;                       // one slot below stack_top
    *sp = (uint64_t)entry;      // pre-fill with entry point

    t->ctx.rip = (uint64_t)entry;
    t->ctx.rsp = (uint64_t)sp;  // rsp points at the slot context_switch will write entry into
    t->ctx.rbp = (uint64_t)sp;
    t->ctx.rbx = 0;
    t->ctx.r12 = 0;
    t->ctx.r13 = 0;
    t->ctx.r14 = 0;
    t->ctx.r15 = 0;

    // mark task runnable only after everything is initialized
    t->state = TASK_READY;

    if (debug_print_is_enabled()) {
        printf("[SCHED] Created task %d: %s (slot=%d, entry=%p, stack=%p)\n", 
               t->id, t->name, slot, (void*)entry, (void*)t->stack_top);
    }

    return t->id;
}

// create a user-mode task with separate kernel and user stacks
int scheduler_create_user_task(const char *name, void (*entry)(void)) {
    // find a free slot in the task table
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED || tasks[i].state == TASK_DEAD) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        printf("[SCHED] No free task slots\n");
        return -1;
    }

    task_t *t = &tasks[slot];
    t->id = next_task_id++;
    t->state = TASK_UNUSED;
    t->tick_count = 0;
    t->is_user = 1;
    t->user_entry = (uint64_t)entry;

    int i = 0;
    while (name[i] && i < 31) {
        t->name[i] = name[i];
        i++;
    }
    t->name[i] = '\0';

    // allocate a 16 kb kernel stack for bootstrap
    void *kernel_stack_bottom = pmm_alloc_page();
    if (!kernel_stack_bottom) {
        printf("[SCHED] Failed to allocate kernel stack for user task %s\n", name);
        return -1;
    }
    for (int p = 1; p < 4; p++) {
        if (!pmm_alloc_page()) {
            printf("[SCHED] Failed to allocate kernel stack page %d for user task %s\n", p, name);
            return -1;
        }
    }
    t->stack_top = (uint64_t)kernel_stack_bottom + TASK_STACK_SIZE;

    // allocate a separate 4 kb user stack
    void *user_stack_bottom = pmm_alloc_page();
    if (!user_stack_bottom) {
        printf("[SCHED] Failed to allocate user stack for task %s\n", name);
        return -1;
    }
    t->user_stack_bottom = (uint64_t)user_stack_bottom;
    t->user_stack_top = (uint64_t)user_stack_bottom + 4096;

    // the initial kernel entry point will bootstrap the task into ring-3
    uint64_t *sp = (uint64_t *)t->stack_top;
    sp--;
    *sp = (uint64_t)user_task_bootstrap;

    t->ctx.rip = (uint64_t)user_task_bootstrap;
    t->ctx.rsp = (uint64_t)sp;
    t->ctx.rbp = (uint64_t)sp;
    t->ctx.rbx = 0;
    t->ctx.r12 = 0;
    t->ctx.r13 = 0;
    t->ctx.r14 = 0;
    t->ctx.r15 = 0;

    // mark runnable only after full initialization
    t->state = TASK_READY;

    if (debug_print_is_enabled()) {
        printf("[SCHED] Created user task %d: %s (slot=%d, entry=%p, kstack=%p, ustack=%p)\n",
               t->id, t->name, slot, (void*)entry, (void*)t->stack_top, (void*)t->user_stack_top);
    }

    return t->id;
}

// called on each timer tick - switches to the next ready task
void scheduler_tick(void) {
    if (current_task_id >= 0) {
        tasks[current_task_id].tick_count++;
    }

    int next = find_next_ready_task();
    if (next == -1) {
        return;
    }
    if (next == current_task_id) {
        return;
    }

    int old_id = current_task_id;
    current_task_id = next;

    if (old_id >= 0) {
        tasks[old_id].state = TASK_READY;
    }
    tasks[next].state = TASK_RUNNING;

    // each task needs its own kernel stack for interrupts/syscalls
    tss_set_kernel_stack(tasks[next].stack_top);

    if (old_id >= 0) {
        context_switch(&tasks[old_id].ctx, &tasks[next].ctx);
    }
}

// start the scheduler - takes over execution and runs the first ready task
void scheduler_start(void) {
    __asm__ volatile("cli");

    // always start from slot 0 regardless of what pit ticks may have done
    current_task_id = -1;
    int next = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_READY) {
            next = i;
            break;
        }
    }

    if (next == -1) {
        printf("[SCHED] No tasks to run!\n");
        __asm__ volatile("sti");
        return;
    }

    // mark all running tasks back to ready (clean up any partial tick state)
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_RUNNING) {
            tasks[i].state = TASK_READY;
        }
    }

    current_task_id = next;
    tasks[next].state = TASK_RUNNING;
    tss_set_kernel_stack(tasks[next].stack_top);
    if (debug_print_is_enabled()) {
        printf("[SCHED] Starting first task: %s (slot=%d)\n", tasks[next].name, next);
    }
    // don't enable interrupts here — switch into the first task atomically
    // so irqs won't run with the wrong kernel stack. the asm bootstrap
    // will enable interrupts after switching stacks.
    scheduler_start_first_task(&tasks[next].ctx);
}

// manually trigger a task switch from the current task
void scheduler_yield(void) {
    // disable interrupts so the switch can't race with pit/keyboard irqs
    __asm__ volatile("cli");
    scheduler_tick();
    __asm__ volatile("sti");
}

// dump all task info for debugging
void scheduler_dump(void) {
    printf("\n[SCHED] Task List:\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_UNUSED) {
            const char *state_str = "UNKNOWN";
            switch (tasks[i].state) {
                case TASK_READY:   state_str = "READY";   break;
                case TASK_RUNNING: state_str = "RUNNING"; break;
                case TASK_BLOCKED: state_str = "BLOCKED"; break;
                case TASK_DEAD:    state_str = "DEAD";    break;
                default: break;
            }
            printf("  [%d] %s: %s (ticks=%d)\n", 
                   tasks[i].id, tasks[i].name, state_str, (int)tasks[i].tick_count);
        }
    }
    printf("\n");
}

// get the id of the currently running task
int scheduler_current_id(void) {
    return current_task_id;
}

// get a pointer to the currently running task's control block
task_t *scheduler_current_task(void) {
    if (current_task_id < 0 || current_task_id >= MAX_TASKS) {
        return 0;
    }
    return &tasks[current_task_id];
}
