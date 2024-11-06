#include <linux/kernel.h>
#include <linux/rtes_framework.h>
#include <linux/errno.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/signal.h>
#include <asm/signal.h>
#include <asm/siginfo.h>


struct rtesThreadHead {
	struct threadNode* head; 
	spinlock_t mutex;
	unsigned long flags;
};

static struct rtesThreadHead threadHead;
static bool head_was_init = false;

static size_t amountReserved;

void debugPrints(void);

void threadHead_init(void) {
	threadHead.head = NULL;
	spin_lock_init(&threadHead.mutex);
	head_was_init = true;
	amountReserved = 0;
}

void lockScheduleLL(void) {
	if (head_was_init) spin_lock_irqsave(&threadHead.mutex, threadHead.flags);
}

void unlockScheduleLL(void) {
	if (head_was_init) spin_unlock_irqrestore(&threadHead.mutex, threadHead.flags);
}

static enum hrtimer_restart restart_period(struct hrtimer *timer) {
	struct timespec cur;
	struct threadNode *task = container_of(timer, struct threadNode, high_res_timer);

	hrtimer_forward_now(timer, task->periodDuration);
	task->periodTime = 0;
	task->heartbeats++;
	if (heartbeats > 2) {
		// Task hasn't run since last period. Likely dead
		sys_cancel_reserve(task->tid);
		return HRTIMER_NORESTART;
	}

	getrawmonotonic(&cur);
	task->prev_schedule = timespec_to_ns(&cur);

	return HRTIMER_RESTART;
}

void rtesDescheduleTask(struct task_struct *task) {
	struct threadNode *node;
	struct timespec cur;
	siginfo_t info;

	if (task == NULL) return;

	lockScheduleLL();

	do {
		node = findThreadInScheduleLL(task->pid);
		if (node == NULL) break;
		if (!(node->actively_running)) break; // Don't double deschedule
		if (node->prev_schedule == 0) break; // Start on a clean period

		// Accumulate time running this period
		getrawmonotonic(&cur);
		node->periodTime += timespec_to_ns(&cur) - node->prev_schedule;

		// Send SIGEXCESS if overran period
		if (node->periodTime > node->cost_ns) {
			printk(KERN_DEBUG "Task %d exceeded scheduled computation time (%lld). Has run for %lld ns",
				  node->tid, node->cost_ns, node->periodTime);

			memset(&info, 0, sizeof(siginfo_t));
			info.si_signo = SIGEXCESS;
			info.si_code = SI_KERNEL;
			info.si_int = SIGEXCESS;

			if(send_sig_info(SIGEXCESS, &info, task)) {
				printk(KERN_ERR "Failed to send SIGEXCESS to thread %d", task->pid);
			}
		}

		node->actively_running = false;
	} while (0);

	unlockScheduleLL();
}

void  rtesScheduleTask(struct task_struct *task) {
	struct threadNode *node;
	struct timespec cur;

	if (task == NULL) return;

	lockScheduleLL();

	do {
		node = findThreadInScheduleLL(task->pid);
		if (node == NULL) break;
		if (node->actively_running) break; // Don't double schedule

		getrawmonotonic(&cur);
		node->prev_schedule = timespec_to_ns(&cur);
		task->heartbeats = 0;
		node->actively_running = true;
	} while (0);

	unlockScheduleLL();
}


SYSCALL_DEFINE4(set_reserve, pid_t, tid, struct timespec*, C , struct timespec*, T , int, cpuid) {
	int output = 0;
	struct threadNode *node;
	struct timespec c,t;
	struct cpumask cpumask;

	if(cpuid < 0 && cpuid > 3) {
		printk(KERN_INFO "CPU ID does not exist!\n");
		return EINVAL;
	}

	if (!head_was_init) {
		threadHead_init();
	}

	if(tid == 0) {
		tid = current->pid;
	}

	if (!access_ok(VERIFY_READ, C, sizeof(struct timespec)) || !access_ok(VERIFY_READ, T, sizeof(struct timespec))) {
		printk(KERN_INFO "Invalid user space pointers!\n");
		return -EFAULT;
	}

	// Copy from user space
	if (copy_from_user(&c, C, sizeof(struct timespec))) {
		printk(KERN_INFO "Error in copying C!\n");
		return -EFAULT;
	}

	if(copy_from_user(&t, T, sizeof(struct timespec))) {
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

			hrtimer_init(&node->high_res_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
			node->high_res_timer.function = restart_period;
			node->next = threadHead.head;
			threadHead.head = node;
			amountReserved++;
		} else {
			printk(KERN_DEBUG "Modifying an existing reservation");
		}

		node->C = c;
		node->T = t;
		node->tid = tid;
		node->cpuid = cpuid;
		node->periodDuration = timespec_to_ktime(t);
		node->cost_ns = timespec_to_ns(&c);
		node->periodTime = 0;
		node->prev_schedule = 0;
		node->actively_running = false;
		hrtimer_start(&node->high_res_timer, node->periodDuration, HRTIMER_MODE_ABS);

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

			hrtimer_cancel(&loopedThread->high_res_timer);
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


