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
extern struct kobject *rtes_kobject;

struct calc_data {
    bool negative;
    u16 whole;
    u16 decimal;
};

struct energy_data {
    unsigned long energy;
    unsigned long power;
    struct kobject *pidFile;
};

// Define the structure for linked list nodes
struct threadNode {
    struct timespec C;		    // Computation time (ms)
    struct timespec T;		    // Period time (ms)
    struct energy_data energyData;    //storing the energy data per thread
    pid_t tid;			    // Thread ID
    int cpuid;			    // CPU ID
    ktime_t periodDuration;	    // Duration as ktime_t
    u64 cost_ns;		    // Cost as ns
    u64 periodTime;		    // Accumulator for period time (ns)
    u64 prev_schedule;		    // Time of previous scheduling
    bool actively_running;	    // Flag to see if currently running
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


void reservationStatus_init(void);
void reservationStatus_exit(void);
void energyTracking_init(void);
void energyTracking_exit(void);
void lockScheduleLL(void);
void unlockScheduleLL(void);
void rtesScheduleTask(struct task_struct *task);
void rtesDescheduleTask(struct task_struct *task);
struct threadNode *findThreadInScheduleLL(pid_t tid);
int removeThreadInScheduleLL(pid_t tid);
int createThreadFile(struct threadNode  *thread);
int removeThreadFile(struct threadNode  *thread);
int createEnergyThreadFile(struct threadNode *thread);
int removeEnergyThreadFile(struct threadNode *thread);

int structured_calc(
    struct calc_data p1, 
    struct calc_data p2, 
    char op, 
    struct calc_data *result
);
#endif /* _RTES_FRAMEWORK_H */
