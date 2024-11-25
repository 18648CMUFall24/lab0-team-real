#ifndef _RTES_FRAMEWORK_H
#define _RTES_FRAMEWORK_H

#include <linux/time.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/hrtimer.h>
#include <linux/math64.h>
#include <linux/kobject.h>

#define BUFFER_SIZE    4096
// extern  struct rtesThreadHead threadHead;
extern bool monitoring_active;

struct calc_data {
    bool negative;
    u16 whole;
    u16 decimal;
};

enum task_mode {
    MAKE_SUSPEND,
    SUSPENDED,
    MAKE_RUNNABLE,
    RUNNABLE,
};

//ta Define the structure for linked list nodes
struct threadNode {
    struct timespec C;			// Computation time (ms)
    struct timespec T;			// Period time (ms)
    struct task_struct *task;		// Task 
    pid_t tid;				// Thread ID
    int cpuid;				// CPU ID
    ktime_t periodDuration;		// Duration as ktime_t
    ktime_t costDuration;		// Cost as ktime_t
    u64 cost_ns;			// Cost as ns
    bool actively_running;		// Flag to see if currently running
    struct hrtimer period_timer;	// High-res timer
    struct hrtimer cost_timer;		// High-res timer
    ktime_t period_remaining_time;	// Time left on timer
    struct kobj_attribute *thread_obj;	// Pointer to the kObject for the file
    ktime_t startTimer;			// Start time of when the thread was reserved
    size_t offset;			// offset of where it is in the data buffer
    char dataBuffer[BUFFER_SIZE];	// buffer of the data
    struct threadNode* next;		// Pointer to the next node
    enum task_mode state;		// Current state
    
};

struct rtesThreadHead {
	struct threadNode* head; 
	spinlock_t mutex;
	unsigned long flags;
	bool need_housekeeping;
	bool head_was_init;
};




bool rtes_head_is_init(void);
bool rtes_needs_housekeeping(void);
void rtes_done_housekeeping(void);
struct threadNode *getFirstThreadNode(void);
void lockScheduleLL(void);
void unlockScheduleLL(void);
void rtesScheduleTask(struct task_struct *task);
void rtesDescheduleTask(struct task_struct *task);
struct threadNode *findThreadInScheduleLL(pid_t tid);
int removeThreadInScheduleLL(pid_t tid);
void handle_rt_task_state_updates(void);
int createThreadFile(struct threadNode  *thread);
int removeThreadFile(struct threadNode  *thread);
int structured_calc(
    struct calc_data p1, 
    struct calc_data p2, 
    char op, 
    struct calc_data *result
);


#endif /* _RTES_FRAMEWORK_H */
