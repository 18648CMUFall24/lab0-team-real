#include <linux/kernel.h>
#include <linux/rtes_framework.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/signal.h>
#include <asm/signal.h>
#include <asm/siginfo.h>


static size_t amountReserved;
struct rtesThreadHead threadHead;
// static bool head_was_init;

void debugPrints(void);

void __init threadHead_init(void) {
	threadHead.head = NULL;
	spin_lock_init(&threadHead.mutex);
	// head_was_init = true;
	threadHead.need_housekeeping = false;
	amountReserved = 0;
}

void lockScheduleLL() {
	// if (head_was_init) 
	spin_lock_irqsave(&threadHead.mutex, threadHead.flags);
}

void unlockScheduleLL() {
	// if (head_was_init) 
	spin_unlock_irqrestore(&threadHead.mutex, threadHead.flags);
}

void pause_timer(struct threadNode *task) {
	task->period_remaining_time = hrtimer_get_remaining(&task->period_timer);
	hrtimer_cancel(&task->period_timer);
}

void resume_timer(struct threadNode *task) {
	hrtimer_start(&task->period_timer, task->period_remaining_time, HRTIMER_MODE_REL);
}

static enum hrtimer_restart end_of_reserved_time(struct hrtimer *timer) {
	// siginfo_t info; // Do we need if enforcing period?
	struct threadNode *task_node = container_of(timer, struct threadNode, cost_timer);
	task_node->state = MAKE_SUSPEND;
	threadHead.need_housekeeping = true;

	// Send SIGEXCESS if overran period
	// Do we still do this if we're enforcing period?
	/*
	printk(KERN_DEBUG "Task %d exceeded scheduled computation time (%lld). Has run for %lld ns",
	  node->tid, node->cost_ns, node->periodTime);

	memset(&info, 0, sizeof(siginfo_t));
	info.si_signo = SIGEXCESS;
	info.si_code = SI_KERNEL;
	info.si_int = SIGEXCESS;

	if(send_sig_info(SIGEXCESS, &info, task)) {
		printk(KERN_ERR "Failed to send SIGEXCESS to thread %d", task->pid);
	}
	*/

	set_tsk_need_resched(current);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart restart_period(struct hrtimer *timer) {
	ktime_t elapsed_time;
	struct calc_data cost, period, util;
	int ret;
	struct threadNode *task = container_of(timer, struct threadNode, period_timer);

	
	energyCalc(task);


	if(monitoring_active) {
		do {
			elapsed_time = ktime_sub(task->periodDuration, task->period_remaining_time);

			cost.whole = (u16) ktime_to_ms(task->costDuration);
			cost.decimal = 0;
			period.whole = (u16) ktime_to_ms(task->periodDuration);
			period.decimal = 0;

			// Calculate the utilization
			ret = structured_calc(cost, period, '/', &util);
			if (ret != 0) {
				break;
			}

			if(BUFFER_SIZE - task->offset > 20) {
				task->offset += sprintf(task->dataBuffer+task->offset,"%llu .%d\n", ktime_to_ms(elapsed_time), util.decimal);
			} else {
				printk(KERN_INFO "Buffer full with offset: %d\n", task->offset);
			}

		} while (0);
	}

	task->period_remaining_time = task->periodDuration;
	task->state = MAKE_RUNNABLE;
	threadHead.need_housekeeping = true;

	set_tsk_need_resched(current);

	hrtimer_forward_now(timer, task->periodDuration);

	return HRTIMER_RESTART;
}

void rtesDescheduleTask(struct task_struct *task) {
	struct threadNode *node;

	if (task == NULL) return;

	lockScheduleLL();
	do {
		node = findThreadInScheduleLL(task->pid);
		if (node == NULL) break;

		if (!(node->actively_running)) break; // Don't double deschedule
		node->actively_running = false;

		// Accumulate time running this period
		pause_timer(node);

	} while (0);
	unlockScheduleLL();
}

void  rtesScheduleTask(struct task_struct *task) {
	struct threadNode *node;

	if (task == NULL) return;

	lockScheduleLL();
	do {
		node = findThreadInScheduleLL(task->pid);
		if (node == NULL) break;

		if (node->actively_running) break; // Don't double schedule
		node->actively_running = true;
		resume_timer(node);

	} while (0);
	unlockScheduleLL();
}


SYSCALL_DEFINE4(set_reserve, pid_t, tid, struct timespec*, C , struct timespec*, T , int, cpuid) {
	int output = 0;
	struct threadNode *node;
	struct timespec c,t;
	struct cpumask cpumask;
	struct calc_data cost, period, util;
	int ret;
	bool exists = false;

	if (cpuid < 0 || cpuid > 3) {
		printk(KERN_INFO "CPU ID does not exist!\n");
		return EINVAL;
	}

	// if (!head_was_init) { threadHead_init(); }

	if (tid == 0) { tid = current->pid; }

	if (!access_ok(VERIFY_READ, C, sizeof(struct timespec)) 
		|| !access_ok(VERIFY_READ, T, sizeof(struct timespec))) {
		printk(KERN_INFO "Invalid user space pointers!\n");
		return -EFAULT;
	}

	// Copy from user space
	if (copy_from_user(&c, C, sizeof(struct timespec))) {
		printk(KERN_INFO "Error in copying C!\n");
		return -EFAULT;
	}

	if (copy_from_user(&t, T, sizeof(struct timespec))) {
		printk(KERN_INFO "Error in copying T!\n");
		return -EFAULT;
	}

	// Setting up CPU affinity using syscall for sched_setaffinity
	cpumask_clear(&cpumask);
	cpumask_set_cpu(cpuid, &cpumask);

	//bin TID against a specific CPU
	if(sched_setaffinity(tid, &cpumask) == -1) {
		printk(KERN_INFO "Error in setting cpu!\n");
		return -1;
	}

	// converting to string
	cost.whole = (u16) ktime_to_ms(timespec_to_ktime(c));
	cost.decimal = 0;
	cost.negative = false;
	period.whole = (u16) ktime_to_ms(timespec_to_ktime(t));
	period.decimal = 0;
	period.negative = false;

	// Calculate the utilization
	ret = structured_calc(cost, period, '/', &util);
	if (ret != 0 || util.negative || (util.whole == 1 && util.decimal != 0) || util.whole > 1) {
		printk(KERN_ERR "calc failed with error: %d\n", ret);
		printk(KERN_ERR "Util = %c%d.%d", util.negative ? '-':' ', util.whole, util.decimal);
		return -EINVAL;
	}
	printk(KERN_INFO "Worst case utilization is: 0.%d\n", util.decimal);

	lockScheduleLL();
	do {
		node = findThreadInScheduleLL(tid);
		if (node == NULL) {
			node = kmalloc(sizeof(struct threadNode), GFP_KERNEL);
			if (node == NULL) {
				printk(KERN_ERR "Error allocating new scheduler node\n");
				output = -ENOMEM;
				break;
			}

			hrtimer_init(&node->period_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
			node->period_timer.function = restart_period;
			hrtimer_init(&node->cost_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
			node->cost_timer.function = end_of_reserved_time;
			node->next = threadHead.head;
			threadHead.head = node;
			amountReserved++;
		} else {
			printk(KERN_DEBUG "Modifying an existing reservation");
			exists = true;
		}

		node->C = c;
		node->T = t;
		node->tid = tid;
		node->cpuid = cpuid;
		node->periodDuration = timespec_to_ktime(t);
		node->cost_ns = timespec_to_ns(&c);
		node->actively_running = false;
		memset(node->dataBuffer,0,BUFFER_SIZE);
		node->offset = 0;


		// Creating thread utilziation file if nodes is not already exist
		if(!exists) {
			createThreadFile(node);
		}

		// start the timer
		hrtimer_start(&node->period_timer, node->periodDuration, HRTIMER_MODE_ABS);

	} while (0);
	debugPrints();

	unlockScheduleLL();

	return output;
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
	debugPrints();
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
			if (prev == NULL) {
				threadHead.head = loopedThread->next;
			} else {
				prev->next = loopedThread->next;
			}

			//remove the thread file utilization 
			removeThreadFile(loopedThread);

			//cancel timers
			hrtimer_cancel(&loopedThread->cost_timer);
			hrtimer_cancel(&loopedThread->period_timer);
			kfree(loopedThread);
			amountReserved--;
			return 0;
		}

		prev = loopedThread;
		loopedThread = loopedThread->next;
	}

	return -1;
}

void debugPrints() {
	struct threadNode *loopedThread = threadHead.head;

	if(amountReserved == 0) { 
		printk(KERN_NOTICE "Empty Linked List!\n"); 
		return;
	}

	printk(KERN_NOTICE "amount reserved is %d\n", amountReserved); 

	while (loopedThread != NULL) {
		printk(KERN_NOTICE "[%px] Thread ID: %lld, CPU ID: %lld, Period Duration: %llu, Cost: %llu\n", 
	 (void *) loopedThread,
	 (long long)loopedThread->tid, 
	 (long long)loopedThread->cpuid, 
	 (unsigned long long)ktime_to_us(loopedThread->periodDuration), 
	 (unsigned long long)loopedThread->cost_ns
	 );

		loopedThread = loopedThread->next;
	}
}


