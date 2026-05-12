#include "scheduler.h"
#include "../arch/gdt.h"
#include "../arch/usermode.h"
#include "../lib/printf.h"
#include "../mm/pmm.h"
#include <stdint.h>

// External assembly functions
extern void context_switch(task_context_t *old_ctx, task_context_t *new_ctx);
extern void scheduler_start_first_task(task_context_t *ctx);

// Task table
static task_t tasks[MAX_TASKS];
static int current_task_id = -1;  // No task running initially
static int next_task_id = 0;      // For assigning IDs

static void user_task_bootstrap(void) {
    task_t *t = scheduler_current_task();
    if (!t || !t->is_user) {
        printf("[SCHED] user_task_bootstrap called without user task\n");
        while (1) { __asm__ volatile("hlt"); }
    }

    printf("[SCHED] Bootstrapping user task %s (task=%d)\n", t->name, (int)t->id);
    tss_set_kernel_stack(t->stack_top);
    usermode_enter(t->user_entry, t->user_stack_top);

    // usermode_enter should never return
    printf("[SCHED] user_task_bootstrap returned unexpectedly\n");
    while (1) { __asm__ volatile("hlt"); }
}

// Helper: find next READY task (round-robin)
static int find_next_ready_task(void) {
    int start = (current_task_id + 1) % MAX_TASKS;
    for (int i = 0; i < MAX_TASKS; i++) {
        int idx = (start + i) % MAX_TASKS;
        if (tasks[idx].state == TASK_READY) {
            return idx;
        }
    }
    return -1;  // No ready tasks
}

void scheduler_init(void) {
    printf("[SCHED] Initializing scheduler...\n");
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
    printf("[SCHED] Scheduler ready (max %d tasks)\n", MAX_TASKS);
}

int scheduler_create_task(const char *name, void (*entry)(void)) {
    // Find free slot
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
    t->state = TASK_READY;
    t->tick_count = 0;

    // Copy name
    int i = 0;
    while (name[i] && i < 31) {
        t->name[i] = name[i];
        i++;
    }
    t->name[i] = '\0';

    // Allocate stack (4 pages = 16KB)
    void *stack_bottom = pmm_alloc_page();
    if (!stack_bottom) {
        printf("[SCHED] Failed to allocate stack for task %s\n", name);
        return -1;
    }
    pmm_alloc_page();  // page 2
    pmm_alloc_page();  // page 3
    pmm_alloc_page();  // page 4

    t->stack_top = (uint64_t)stack_bottom + TASK_STACK_SIZE;
    t->user_stack_bottom = 0;
    t->user_stack_top = 0;
    t->user_entry = 0;
    t->is_user = 0;

    // Set up initial stack frame.
    // context_switch does: mov rsp,[ctx+48]; mov rax,[ctx+56]; mov [rsp],rax; ret
    // So we need rsp to point at a valid writable slot, and rip = entry.
    // We pre-allocate one slot at the top of the stack for that purpose.
    uint64_t *sp = (uint64_t *)t->stack_top;
    sp--;                       // one slot below stack_top
    *sp = (uint64_t)entry;      // pre-fill with entry (will be overwritten by context_switch, but good practice)

    t->ctx.rip = (uint64_t)entry;
    t->ctx.rsp = (uint64_t)sp;  // rsp points at the slot context_switch will write entry into
    t->ctx.rbp = (uint64_t)sp;
    t->ctx.rbx = 0;
    t->ctx.r12 = 0;
    t->ctx.r13 = 0;
    t->ctx.r14 = 0;
    t->ctx.r15 = 0;

    printf("[SCHED] Created task %d: %s (slot=%d, entry=%p, stack=%p)\n", 
           t->id, t->name, slot, (void*)entry, (void*)t->stack_top);

    return t->id;
}

int scheduler_create_user_task(const char *name, void (*entry)(void)) {
    // Find free slot
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
    t->state = TASK_READY;
    t->tick_count = 0;
    t->is_user = 1;
    t->user_entry = (uint64_t)entry;

    int i = 0;
    while (name[i] && i < 31) {
        t->name[i] = name[i];
        i++;
    }
    t->name[i] = '\0';

    // Allocate kernel stack (16 KB) for the task while it is executing in kernel mode.
    void *kernel_stack_bottom = pmm_alloc_page();
    if (!kernel_stack_bottom) {
        printf("[SCHED] Failed to allocate kernel stack for user task %s\n", name);
        return -1;
    }
    pmm_alloc_page();
    pmm_alloc_page();
    pmm_alloc_page();
    t->stack_top = (uint64_t)kernel_stack_bottom + TASK_STACK_SIZE;

    // Allocate separate user stack (4 KB)
    void *user_stack_bottom = pmm_alloc_page();
    if (!user_stack_bottom) {
        printf("[SCHED] Failed to allocate user stack for task %s\n", name);
        return -1;
    }
    t->user_stack_bottom = (uint64_t)user_stack_bottom;
    t->user_stack_top = (uint64_t)user_stack_bottom + 4096;

    // Initial kernel entry point: bootstrap the task into Ring-3.
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

    printf("[SCHED] Created user task %d: %s (slot=%d, entry=%p, kstack=%p, ustack=%p)\n",
           t->id, t->name, slot, (void*)entry, (void*)t->stack_top, (void*)t->user_stack_top);

    return t->id;
}

void scheduler_tick(void) {
    if (current_task_id >= 0) {
        tasks[current_task_id].tick_count++;
    }

    int next = find_next_ready_task();
    if (next == -1) return;
    if (next == current_task_id) return;

    int old_id = current_task_id;
    current_task_id = next;

    if (old_id >= 0) {
        tasks[old_id].state = TASK_READY;
    }
    tasks[next].state = TASK_RUNNING;

    // Each task owns its own kernel stack for interrupts/syscalls.
    tss_set_kernel_stack(tasks[next].stack_top);

    if (old_id >= 0) {
        context_switch(&tasks[old_id].ctx, &tasks[next].ctx);
    }
}

void scheduler_start(void) {
    __asm__ volatile("cli");

    // Always start from slot 0 regardless of what PIT ticks may have done
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

    // Mark all other ready tasks back to READY (undo any partial tick damage)
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_RUNNING) {
            tasks[i].state = TASK_READY;
        }
    }

    current_task_id = next;
    tasks[next].state = TASK_RUNNING;
    tss_set_kernel_stack(tasks[next].stack_top);
    printf("[SCHED] Starting first task: %s (slot=%d)\n", tasks[next].name, next);
    scheduler_start_first_task(&tasks[next].ctx);
}

void scheduler_yield(void) {
    // Manually trigger a task switch
    scheduler_tick();
}

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

int scheduler_current_id(void) {
    return current_task_id;
}

task_t *scheduler_current_task(void) {
    if (current_task_id < 0 || current_task_id >= MAX_TASKS) {
        return 0;
    }
    return &tasks[current_task_id];
}
