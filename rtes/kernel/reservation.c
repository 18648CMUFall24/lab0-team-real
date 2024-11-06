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


static size_t amountReserved;
struct rtesThreadHead threadHead;
static bool head_was_init;

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
	ktime_t elapsed_time;
	struct threadNode *task = container_of(timer, struct threadNode, high_res_timer);
	char periodString[10];
	char executeString[10];
	char calcResult[10] = {};
	int ret;

	hrtimer_forward_now(timer, task->periodDuration);
	

	getrawmonotonic(&cur);
	task->prev_schedule = timespec_to_ns(&cur);

	if(monitoring_active)
	{

		elapsed_time = ktime_sub(ktime_get(), task->startTimer);

		//converting to string
		sprintf(periodString, "%llu", div64_u64(ktime_to_ns(task->periodDuration),1000000));
		sprintf(executeString, "%llu",div64_u64(task->periodTime,1000000));
		printk(KERN_INFO "Period is: %s\n", periodString);
		printk(KERN_INFO "Cost is: %s\n", executeString);

		//Calculate the utilization
		ret = sys_calc(executeString,periodString,'/',calcResult);
		printk(KERN_INFO "calculated utilization is: %s\n", calcResult);

		if (ret != 0) {
        	printk(KERN_ERR "sys_calc failed with error: %d\n", ret);
		}
		else
		{

			strncpy(task->utilization, calcResult, sizeof(task->utilization));

		}



		if(BUFFER_SIZE - task->offset < 20)
		{
			printk(KERN_INFO "Buffer full with offset: %d\n", task->offset);
		}
		else
		{
			task->offset += sprintf(task->dataBuffer+task->offset,"%llu %s\n",ktime_to_ms(elapsed_time),task->utilization);

			printk(KERN_INFO "offset increased: %d\n", task->offset);
			printk(KERN_INFO "Time passed: %llu\n", ktime_to_ms(elapsed_time));
			//printk(KERN_INFO "Utilization: %s\n", task->dataBuffer);
		}

	}
	else
	{

		memset(task->dataBuffer,0,BUFFER_SIZE);
		task->offset = 0;
	}

	task->periodTime = 0;
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

		node->actively_running = true;
	} while (0);

	unlockScheduleLL();
}


SYSCALL_DEFINE4(set_reserve, pid_t, tid, struct timespec*, C , struct timespec*, T , int, cpuid) {
	int output = 0;
	struct threadNode *node;
	struct timespec c,t;
	struct cpumask cpumask;
	char periodString[10];
	char executeString[10];
	char calcResult[10] = {};
	int ret;
	bool exists = false;

	if(cpuid < 0 || cpuid > 3) {
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
			exists = true;
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

		//converting to string
		sprintf(periodString, "%llu", ktime_to_ms(node->periodDuration));
		sprintf(executeString, "%llu", ktime_to_ms(timespec_to_ktime(c)));

		//Calculate the utilization
		ret = sys_calc(executeString,periodString,'/',calcResult);
		if (ret != 0) {
        	printk(KERN_ERR "sys_calc failed with error: %d\n", ret);
		}
		else
		{
			strncpy(node->utilization, calcResult, sizeof(node->utilization));

		}

		printk(KERN_INFO "Utilization is: %s\n", node->utilization);

		//Creating thread utilziation file if nodes is not already exist
		if(!exists)
		{
			createThreadFile(node);
		}

		//start the timer
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
			
			//remove the thread file utilization 
			removeThreadFile(loopedThread);
			
			//cancel timer
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


