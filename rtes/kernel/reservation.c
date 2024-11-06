#include <linux/kernel.h>
#include <linux/rtes_framework.h>
#include <linux/errno.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/spinlock.h>
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

void lockScheduleLL() {
	if (head_was_init) 
		spin_lock_irqsave(&threadHead.mutex, threadHead.flags);
}

void unlockScheduleLL() {
	if (head_was_init) 
		spin_unlock_irqrestore(&threadHead.mutex, threadHead.flags);
}

static enum hrtimer_restart restart_period(struct hrtimer *timer) {
	struct threadNode *task = container_of(timer, struct threadNode, high_res_timer);
	hrtimer_forward_now(timer, task->periodDuration);
	task->periodTime = 0;
	printk(KERN_CRIT "Restarted timer for task %d", task->tid);
	return HRTIMER_RESTART;
}

void rtesDescheduleTask(struct task_struct *task) {
	struct threadNode *task_node = NULL;
	struct timespec cur;
	u64 cur_ns, delta;
	siginfo_t info;

	if (task == NULL) return;

	lockScheduleLL();
	task_node = findThreadInScheduleLL(task->pid);
	if (task_node == NULL) {
		unlockScheduleLL();
		return;
	}

	getrawmonotonic(&cur);
	cur_ns = timespec_to_ns(&cur);
	printk(KERN_CRIT "DESCHEDULED %d @ %lld\n", task->pid, cur_ns);
	delta = cur_ns - task_node->prev_schedule;
	task_node->prev_schedule = cur_ns;
	task_node->periodTime += delta;

	if (task_node->periodTime > task_node->cost_us) {
		memset(&info, 0, sizeof(siginfo_t));
		info.si_signo = SIGEXCESS;
		info.si_code = SI_KERNEL;
		info.si_int = SIGEXCESS;
		if (send_sig_info(SIGEXCESS, &info, task) < 0) {
			printk(KERN_ERR "Failed to send SIGEXCESS to thread %d", task_node->tid);
		}

		printk(KERN_INFO "Thread %d exceeded reserved time (%lld us) utilization and has run for %lld us", task_node->tid, task_node->cost_us, task_node->periodTime);
	}
}

void rtesScheduleTask(struct task_struct *task) {
	struct threadNode *task_node;
	struct timespec cur;

	if (task == NULL) return;

	lockScheduleLL();
	task_node = findThreadInScheduleLL(task->pid);

	if (task_node != NULL) {
		getrawmonotonic(&cur);
		task_node->prev_schedule = timespec_to_ns(&cur);
		printk(KERN_CRIT "SCHEDULED %d @ %lld\n", task->pid, task_node->prev_schedule);
	}

	unlockScheduleLL();
}


SYSCALL_DEFINE4(set_reserve, pid_t, tid, struct timespec*, C , struct timespec*, T , int, cpuid) 
{
	struct timespec c,t,cur;
	struct cpumask cpumask;
	struct threadNode *lookThread = NULL;

	if(cpuid < 0 || cpuid > 3) {
		printk(KERN_INFO "CPU ID does not exist!\n");
		return EINVAL;
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


	printk(KERN_CRIT "Trying to reserve task %d (%lld, %lld)", tid, c, t);
	// Setting up CPU affinity using syscall for sched_setaffinity
	cpumask_clear(&cpumask);
	cpumask_set_cpu(cpuid, &cpumask);

	//bin TID against a specific CPU
	if(sched_setaffinity(tid, &cpumask) == -1) {
		printk(KERN_INFO "Error in setting cpu!\n");
		return -1;
	}

	if (!head_was_init) {
		threadHead_init();
	}

	lockScheduleLL();
	lookThread = findThreadInScheduleLL(tid);

	if (lookThread == NULL) {
		lookThread = kmalloc(sizeof(struct threadNode), GFP_KERNEL);
		if(!lookThread) {
			printk(KERN_INFO "Error in malloc!\n");
			unlockScheduleLL();
			return -ENOMEM;
		}

		hrtimer_init(&lookThread->high_res_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
		lookThread->high_res_timer.function = &restart_period;

		//setting the thread head and its next
		lookThread->next = threadHead.head;
		threadHead.head = lookThread; // Insert into linked list
		
		amountReserved++;
	}


	//set the nodes with its parameters
	lookThread->C = c;
	lookThread->T = t;
	lookThread->tid = tid;
	lookThread->cpuid = cpuid;
	lookThread->periodDuration = timespec_to_ktime(t);
	lookThread->cost_us = ktime_to_ns(timespec_to_ktime(c));
	lookThread->periodTime = 0;
	getrawmonotonic(&cur);
	lookThread->prev_schedule = timespec_to_ns(&cur);
	hrtimer_start(&lookThread->high_res_timer, lookThread->periodDuration, HRTIMER_MODE_ABS);


	debugPrints();
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
	amountReserved--;

	debugPrints();

	unlockScheduleLL();

	return output;
}


// Requires locking struct
struct threadNode *findThreadInScheduleLL(pid_t tid){
	struct threadNode *loopedThread;

	loopedThread = threadHead.head;

	while(loopedThread != NULL) {
		if (loopedThread->tid == tid) {
			// printk(KERN_NOTICE "Found thread %d!\n", tid); 
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
		 (unsigned long long)loopedThread->cost_us);
		loopedThread = loopedThread->next;
	}
}

