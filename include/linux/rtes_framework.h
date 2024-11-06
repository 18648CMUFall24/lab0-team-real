#ifndef _RTES_FRAMEWORK_H
#define _RTES_FRAMEWORK_H

#include <linux/time.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/hrtimer.h>


// Define the structure for linked list nodes
struct threadNode {
    struct timespec C;			// Computation time (ms)
    struct timespec T;			// Period time (ms)
    pid_t tid;				// Thread ID
    int cpuid;				// CPU ID
    ktime_t periodDuration;		// Duration as ktime_t
    u64 cost_us;			// Cost as usec
    u64 periodTime;			// Accumulator for period time (us)
    u64 prev_schedule;			// Period time remaining at start of running
    struct hrtimer high_res_timer;	// High-res timer
    struct threadNode* next;		// Pointer to the next node
};


void lockScheduleLL(void);
void unlockScheduleLL(void);
void rtesScheduleTask(struct task_struct *task);
void rtesDescheduleTask(struct task_struct *task);
struct threadNode *findThreadInScheduleLL(pid_t tid);
int removeThreadInScheduleLL(pid_t tid);

#endif /* _RTES_FRAMEWORK_H */
