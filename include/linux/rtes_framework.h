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
extern  struct rtesThreadHead threadHead;
extern bool monitoring_active;


// Define the structure for linked list nodes
struct threadNode {
    struct timespec C;		    // Computation time (ms)
    struct timespec T;		    // Period time (ms)
    pid_t tid;			    // Thread ID
    int cpuid;			    // CPU ID
    int heartbeats;         //Check to see if thread has died
    ktime_t periodDuration;	    // Duration as ktime_t
    u64 cost_ns;		    // Cost as ns
    u64 periodTime;		    // Accumulator for period time (ns)
    u64 prev_schedule;		    // Time of previous scheduling
    bool actively_running;	    // Flag to see if currently running
    bool fileRemoved;            //Flag to set fileRemoved when thread or sigexcess
    struct hrtimer high_res_timer;  // High-res timer
    struct kobj_attribute *thread_obj; // Pointer to the kObject for the file
    ktime_t startTimer;              //Start time of when the thread was reserved
    char utilization[20];            //utilization string
    size_t offset;                   //offset of where it is in the data buffer
    char dataBuffer[BUFFER_SIZE];    //buffer of the data
    struct threadNode* next;	    // Pointer to the next node
};

struct rtesThreadHead {
	struct threadNode* head; 
	spinlock_t mutex;
	unsigned long flags;
};



void lockScheduleLL(void);
void unlockScheduleLL(void);
void rtesScheduleTask(struct task_struct *task);
void rtesDescheduleTask(struct task_struct *task);
struct threadNode *findThreadInScheduleLL(pid_t tid);
int removeThreadInScheduleLL(pid_t tid);
int createThreadFile(struct threadNode  *thread);
int removeThreadFile(struct threadNode  *thread);

#endif /* _RTES_FRAMEWORK_H */
