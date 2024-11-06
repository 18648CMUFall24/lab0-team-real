#ifndef _RTES_FRAMEWORK_H
#define _RTES_FRAMEWORK_H

#include <linux/time.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/hrtimer.h>
#include <linux/kobject.h>

#define BUFFER_SIZE    4096
extern struct rtesThreadHead threadHead;
extern bool monitoring_active;

// Define the structure for linked list nodes
struct threadNode {
    struct timespec C;		// Computation time (ms)
    struct timespec T;		// Period time (ms)
    pid_t tid;			// Thread ID
    int cpuid;			// CPU ID
    ktime_t periodDuration;	// Duration as ktime_t
    u64 cost_us;	// Cost as uncs
    u64 periodTime;	// Accumulator for period time (us)
    ktime_t prev_schedule;	// Period time remaining at start of running
    struct hrtimer high_res_timer;	// High-res timer
    struct kobj_attribute *thread_obj; // Pointer to the kObject for the file
    ktime_t startTimer;
    char utilization[20];
    unsigned long periodIncrement;
    unsigned long costIncrement;
    size_t offset;
    char dataBuffer[BUFFER_SIZE];
    struct threadNode* next;	// Pointer to the next node
};

struct rtesThreadHead {
	struct threadNode* head; 
	spinlock_t mutex;
};


void lockScheduleLL(void);
void unlockScheduleLL(void);
void rtesScheduleTask(struct threadNode *task);
void rtesDescheduleTask(struct threadNode *task);
struct threadNode *findThreadInScheduleLL(pid_t tid);
int removeThreadInScheduleLL(pid_t tid);
int createThreadFile(struct threadNode  *thread);
int removeThreadFile(struct threadNode  *thread);


#endif /* _RTES_FRAMEWORK_H */
