#include <linux/rtes_framework.h>
#include <linux/errno.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/spinlock.h>


struct rtesThreadHead {
	struct threadNode* head; 
	spinlock_t mutex;
};

static struct rtesThreadHead threadHead;
static bool head_was_init = false;


void threadHead_init(void) {
	threadHead.head = NULL;
	spin_lock_init(&threadHead.mutex);
	head_was_init = true;
}

void lockScheduleLL() {
	if (head_was_init) spin_lock_irq(&threadHead.mutex);
}

void unlockScheduleLL() {
	if (head_was_init) spin_unlock_irq(&threadHead.mutex);
}

static enum hrtimer_restart restart_period(struct hrtimer *timer) {
	struct threadNode *task = container_of(timer, struct threadNode, high_res_timer);
	hrtimer_forward_now(timer, task->periodDuration);
	task->periodTime = 0;
	task->prev_schedule = hrtimer_get_remaining(timer);
	return HRTIMER_RESTART;
}

void rtesDescheduleTask(struct threadNode *task) {
	ktime_t rem = hrtimer_get_remaining(&task->high_res_timer);
	ktime_t delta = ktime_sub(task->prev_schedule, rem);
	task->periodTime += ktime_to_us(delta);
	/*
	if (task->periodTime > task->cost_us) {
		kill(task->pid, SIGEXCESS);
	}
	*/
}

void rtesScheduleTask(struct threadNode *task) {
	task->prev_schedule = hrtimer_get_remaining(&task->high_res_timer);
}


SYSCALL_DEFINE4(set_reserve, pid_t, tid, struct timespec*, C , struct timespec*, T , int, cpuid) 
{
	struct threadNode *new_node;
	struct timespec c,t;
	struct cpumask cpumask;

	if(cpuid > 0 && cpuid <= 3) {
		return EINVAL;
	}

	if (!head_was_init) {
		threadHead_init();
	}

	if(tid == 0) {
		tid = current->pid;
	}

	// Copy from user space
	if (copy_from_user(&c, C, sizeof(struct timespec)) || copy_from_user(&t, T, sizeof(struct timespec))) {
		return -EFAULT;
	}


	new_node = kmalloc(sizeof(struct threadNode), GFP_KERNEL);
	if(!new_node) {
		return -ENOMEM;
	}

	//set the nodes with its parameters
	new_node->C = c;
	new_node->T = t;
	new_node->tid = tid;
	new_node->cpuid = cpuid;
	new_node->periodDuration = timespec_to_ktime(t);
	new_node->cost_us = ktime_to_us(timespec_to_ktime(c));
	new_node->periodTime = 0;
	hrtimer_init(&new_node->high_res_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	new_node->high_res_timer.function = &restart_period;


	// Setting up CPU affinity using syscall for sched_setaffinity
	cpumask_clear(&cpumask);
	cpumask_set_cpu(cpuid, &cpumask);

	//bin TID against a specific CPU
	if(sched_setaffinity(tid, &cpumask) == -1) {
		kfree(new_node);
		return -1;
	}


	lockScheduleLL();
	//setting the thread head and its next
	new_node->next = threadHead.head;
	threadHead.head = new_node; // Insert into linked list
	unlockScheduleLL();

	return 0;
}

SYSCALL_DEFINE1(cancel_reserve, pid_t, tid) 
{
	int output;

	// If tid is 0, use current thread's pid
	if (tid == 0) {
		tid = current->pid;
	}

	lockScheduleLL();
	output = removeThreadInScheduleLL(tid);
	unlockScheduleLL();

	return output;
}


// Requires locking struct
struct threadNode *findThreadInScheduleLL(pid_t tid){
	struct threadNode *loopedThread = threadHead.head;

	while(loopedThread != NULL) {
		if (loopedThread->tid == tid) {
			break;
		}

		loopedThread = loopedThread->next;
	}

	return loopedThread;
}

// Requires locking
int removeThreadInScheduleLL(pid_t tid) {
	struct threadNode *loopedThread = threadHead.head;
	struct threadNode *prev = NULL;

	while(loopedThread != NULL) {
		if(loopedThread->tid == tid) {
			//remove from linked List
			if(prev != NULL) {	
				prev->next = loopedThread->next;
			} else {
				threadHead.head = loopedThread->next;
			}

			kfree(loopedThread);
			return 0;
		}

		prev = loopedThread;
		loopedThread = loopedThread->next;
	}

	return -1;
}