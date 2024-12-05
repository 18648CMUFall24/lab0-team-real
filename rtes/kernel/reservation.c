#include <linux/kernel.h>
#include <linux/rtes_framework.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/pid.h>
#include <linux/slab.h> 
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/signal.h>
#include <asm/signal.h>
#include <asm/siginfo.h>

static size_t amountReserved; static struct rtesThreadHead threadHead;
static bool head_was_init = false;

void debugPrints(void);

void threadHead_init(void) {
	threadHead.head = NULL;
	spin_lock_init(&threadHead.mutex);
	head_was_init = true;
	threadHead.need_housekeeping = false;
	amountReserved = 0;
}

bool rtes_head_is_init() {
	return head_was_init;
}

bool rtes_needs_housekeeping() {
	return threadHead.need_housekeeping;
}

void rtes_done_housekeeping() {
	threadHead.need_housekeeping = false;
}

// Requires locked schedule ll
struct threadNode *getFirstThreadNode() {
	return threadHead.head;
}

void lockScheduleLL() {
	if (head_was_init) 
		spin_lock_irqsave(&threadHead.mutex, threadHead.flags);
}

void unlockScheduleLL() {
	if (head_was_init) 
		spin_unlock_irqrestore(&threadHead.mutex, threadHead.flags);
}

void pause_timer(struct threadNode *task) {
	printk(KERN_NOTICE "pause_timer");
	task->period_remaining_time = hrtimer_get_remaining(&task->cost_timer);
	hrtimer_cancel(&task->cost_timer);
}

void resume_timer(struct threadNode *task) {
	printk(KERN_NOTICE "resume_timer");
	hrtimer_start(&task->cost_timer, task->period_remaining_time, HRTIMER_MODE_REL);
}

static enum hrtimer_restart end_of_reserved_time(struct hrtimer *timer) {
	struct threadNode *task; 

	printk(KERN_NOTICE "end_of_reserved_time");
	task = container_of(timer, struct threadNode, cost_timer);
	task->state = MAKE_SUSPEND;
	task->task->state = TASK_UNINTERRUPTIBLE;
	threadHead.need_housekeeping = true;

	set_tsk_need_resched(current);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart restart_period(struct hrtimer *timer) {
	ktime_t elapsed_time;
	struct calc_data cost, period, util;
	int ret;
	struct threadNode *task; 

	printk(KERN_NOTICE "restart_period");
	task = container_of(timer, struct threadNode, period_timer);

	
	energyCalc(task);


	if(monitoring_active) {
		do {
			elapsed_time = ktime_sub(task->periodDuration, task->period_remaining_time);

			cost.whole = (u16) ktime_to_ms(elapsed_time);
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

	if (task->task->state == TASK_DEAD) {
		put_task_struct(task->task);
		task->state = DEAD;
		threadHead.need_housekeeping = true;
		return HRTIMER_NORESTART;
	}

	task->period_remaining_time = task->costDuration;
	task->state = (task->state == RUNNABLE) ? RUNNABLE : MAKE_RUNNABLE;
	threadHead.need_housekeeping = (task->state == MAKE_RUNNABLE);
	wake_up_process(task->task);

	hrtimer_forward_now(timer, task->periodDuration);

	set_tsk_need_resched(current);
	return HRTIMER_RESTART;
}

void rtesDescheduleTask(struct task_struct *task) {
	struct threadNode *node;

	if (task == NULL) return;

	lockScheduleLL();
	do {
		node = findThreadInScheduleLL(task->pid);
		if (node == NULL) break;
		printk(KERN_NOTICE "rtesDescheduleTask");

		if (!(node->actively_running)) break; // Don't double deschedule
		node->actively_running = false;

		// Accumulate time running this period
		pause_timer(node);
		printk(KERN_ERR "[KERN] Descheduling task %d. Time Remaining: %lld", 
			node->tid, ktime_to_us(node->period_remaining_time));

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
		printk(KERN_NOTICE "rtesScheduleTask");


		if (node->actively_running) break; // Don't double schedule
		node->actively_running = true;
		
		printk(KERN_ERR "[KERN] Scheduling task %d. Time Remaining: %lld", 
			node->tid, ktime_to_us(node->period_remaining_time));

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
	struct pid *pid;
	struct task_struct *task = NULL;

	int ret;
	bool exists = false;

	printk(KERN_NOTICE "sys_set_reserve");

	if (cpuid < 0 || cpuid > 3) {
		printk(KERN_INFO "CPU ID does not exist!\n");
		return EINVAL;
	}

	if (!head_was_init) { 
		threadHead_init(); 
	}

	if (tid == 0) { 
		tid = current->pid; 
		task = current;
	}

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



	if (!task) {
		rcu_read_lock();
		pid = find_vpid(tid); // Get the `pid` struct
		if (pid) {
			task = pid_task(pid, PIDTYPE_PID); // Get task from `pid`
			if (task) {
				get_task_struct(task); // Increment reference count
			} else {
				printk(KERN_INFO "Couldn't find task!\n");
			}
		}
		rcu_read_unlock();
	}

	if (!task) {
		return -EFAULT;
	}

	// converting to string
	cost.whole = ktime_to_ms(timespec_to_ktime(c));
	cost.decimal = 0;
	cost.negative = false;
	period.whole = ktime_to_ms(timespec_to_ktime(t));
	period.decimal = 0;
	period.negative = false;
	util.whole = 0;
	util.decimal = 0;
	util.negative = false;

	// Calculate the utilization
	printk(KERN_INFO "Cost = %c%hd.%hd", 
		cost.negative ? '-':' ', cost.whole, cost.decimal);
	printk(KERN_INFO "Period = %c%hd.%hd", 
		period.negative ? '-':' ', period.whole, period.decimal);
	ret = structured_calc(cost, period, '/', &util);
	if (ret != 0 || util.negative || (util.whole == 1 && util.decimal != 0) || util.whole > 1) {
		printk(KERN_ERR "calc failed with error: %d\n", ret);
		printk(KERN_ERR "Util = %c%d.%d", util.negative ? '-':' ', util.whole, util.decimal);
		return -EINVAL;
	}
	printk(KERN_INFO "Worst case utilization is: %d.%d\n", util.whole, util.decimal);

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
			node->state = RUNNABLE;
			threadHead.head = node;
			amountReserved++;
		} else {
			printk(KERN_DEBUG "Modifying an existing reservation");
			exists = true;
		}

		node->C = c;
		node->T = t;
		node->task = task;
		node->tid = tid;
		node->periodDuration = timespec_to_ktime(t);
		node->costDuration = timespec_to_ktime(c);
		node->cost_ns = timespec_to_ns(&c);
		node->actively_running = (tid == current->pid);
		node->period_remaining_time = node->costDuration;
		
		memset(node->dataBuffer,0,BUFFER_SIZE);
		node->offset = 0;


		// Creating thread utilziation file if nodes is not already exist
		if(!exists) {
			createThreadFile(node);
		}
		

		if(cpuid == -1)
		{
			node->cpuid = insert_task(node);
			if(node->cpuid == -1)
			{
				printk(KERN_INFO "No active reservations!\n");
				kfree(node);
				output = -EBUSY;
				break;
			}
			printk(KERN_INFO "was able to insert task in cpu %d!\n", node->cpuid);
		}
		else
		{
			node->cpuid = cpuid;
		}
		//Setting up CPU affinity using syscall for sched_setaffinity
		cpumask_clear(&cpumask);
		cpumask_set_cpu(node->cpuid, &cpumask);

		//bin TID against a specific CPU
		if(sched_setaffinity(node->tid, &cpumask) == -1) {
			printk(KERN_INFO "Error in setting cpu!\n");
			return -1;
		}

		// start the timer
		hrtimer_start(&node->period_timer, node->periodDuration, HRTIMER_MODE_ABS);
		
		if (node->actively_running) {
			hrtimer_start(&node->cost_timer, node->costDuration, HRTIMER_MODE_REL);
		}

	} while (0);
	debugPrints();
	print_buckets();

	// threadHead.need_housekeeping = true;
	unlockScheduleLL();

	return output;
}

SYSCALL_DEFINE1(cancel_reserve, pid_t, tid) 
{
	struct pid *pid;
	struct task_struct *task = NULL;
	int output;

	printk(KERN_NOTICE "sys_cancel_reserve");

	// If tid is 0, use current thread's pid
	if (tid == 0) {
		tid = current->pid;
	}

	lockScheduleLL();
	output = removeThreadInScheduleLL(tid);
	debugPrints();
	print_buckets();
	unlockScheduleLL();


	rcu_read_lock();
	pid = find_vpid(tid); // Get the `pid` struct
	if (pid) {
		task = pid_task(pid, PIDTYPE_PID); // Get task from `pid`
		if (task) {
			put_task_struct(task); // Decrement reference count
		}
	}
	rcu_read_unlock();

	return output;
}

SYSCALL_DEFINE0(end_job) {
	struct threadNode *task;
	long output = EFAULT;

	printk(KERN_NOTICE "sys_end_job");

	lockScheduleLL();
	do {
		task = findThreadInScheduleLL(current->pid);
		if (task) {
			if (!(task->actively_running)) break; // Don't double deschedule
			pause_timer(task);
			printk(KERN_ERR "Called end_job from task %d", task->tid);

			task->actively_running = false;
			task->state = MAKE_SUSPEND;

			threadHead.need_housekeeping = true;
			output = 0;

		}
	} while(0);
	unlockScheduleLL();

	set_tsk_need_resched(current);

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

	printk(KERN_NOTICE "removeThreadInScheduleLL");

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
			
			remove_task_from_bucket(loopedThread->tid, loopedThread->cpuid);

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

	printk(KERN_NOTICE "debugPrints");

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
		 (unsigned long long)ktime_to_us(loopedThread->costDuration)
		 );

		loopedThread = loopedThread->next;
	}
}


