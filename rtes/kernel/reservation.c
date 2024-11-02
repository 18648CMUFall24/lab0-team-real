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
};

static struct rtesThreadHead threadHead;
static bool head_was_init = false;

//int amountReserved;


void threadHead_init(void) {
	threadHead.head = NULL;
	spin_lock_init(&threadHead.mutex);
	head_was_init = true;
	//amountReserved = 0;
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

void rtesDescheduleTask(struct task_struct *task) {
	struct threadNode *task_node;
	ktime_t rem, delta;
	siginfo_t info;

	if (task == NULL) return;

	lockScheduleLL();
	task_node = findThreadInScheduleLL(task);
	if (task_node == NULL) {
		unlockScheduleLL();
		return;
	}

	rem = hrtimer_get_remaining(&task_node->high_res_timer);
	delta = ktime_sub(task_node->prev_schedule, rem);
	task_node->periodTime += ktime_to_us(delta);
	if (task_node->periodTime > task_node->cost_us) {
		memset(&info, 0, sizeof(siginfo_t));
		info.si_signo = SIGEXCESS;
		info.si_code = SI_KERNEL;
		info.si_int = SIGEXCESS;
		if (send_sig_info(SIGEXCESS, &info, task) < 0) {
			printk(KERN_WARNING "Failed to send SIGEXCESS to thread %d", task_node->tid);
		}
		
		printk(KERN_ERR "Thread %d exceeded reserved time utilization", task_node->tid);
	}
	unlockScheduleLL();
}

void rtesScheduleTask(struct task_struct *task) {
	struct threadNode *task_node;

	if (task == NULL) return;

	lockScheduleLL();
	task_node = findThreadInScheduleLL(task);
	if (task_node == NULL) {
		unlockScheduleLL();
		return;
	}

	task_node->prev_schedule = hrtimer_get_remaining(&task_node->high_res_timer);
	unlockScheduleLL();
}


SYSCALL_DEFINE4(set_reserve, pid_t, tid, struct timespec*, C , struct timespec*, T , int, cpuid) 
{
	struct threadNode *new_node;
	struct timespec c,t;
	struct cpumask cpumask;
	struct threadNode *lookThread = NULL;

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

	if (!access_ok(VERIFY_READ, C, sizeof(struct timespec)) || !access_ok(VERIFY_READ, T, sizeof(struct timespec))) 
	{
		printk(KERN_INFO "Invalid user space pointers!\n");
		return -EFAULT;
	}

	// Copy from user space
	if (copy_from_user(&c, C, sizeof(struct timespec)))
	{
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

	lookThread = findThreadInScheduleLL(tid);


	if(lookThread != NULL)
	{
		lookThread->C = c;
		lookThread->T = t;
		lookThread->tid = tid;
		lookThread->cpuid = cpuid;
		lookThread->periodDuration = timespec_to_ktime(t);
		lookThread->cost_us = ktime_to_us(timespec_to_ktime(c));
		lookThread->periodTime = 0;
		hrtimer_start(&new_node->high_res_timer, new_node->periodDuration, HRTIMER_MODE_ABS);

		hrtimer_init(&new_node->high_res_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
		new_node->high_res_timer.function = &restart_period;
		printk(KERN_INFO "Updated existing thread!\n");
	}
	else
{
		new_node = kmalloc(sizeof(struct threadNode), GFP_KERNEL);
		if(!new_node) {
			printk(KERN_INFO "Error in malloc!\n");
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
		hrtimer_start(&new_node->high_res_timer, new_node->periodDuration, HRTIMER_MODE_ABS);

		//setting the thread head and its next
		new_node->next = threadHead.head;
		threadHead.head = new_node; // Insert into linked list
		//amountReserved++;
	}

	// if(amountReserved >= 1)
	// {
	// 	struct threadNode *loopedThread = threadHead.head;
	// 	printk(KERN_INFO "amount reserved is %d\n", amountReserved);
	// 	printk(KERN_INFO "Thread ID: %lld, CPU ID: %lld, Period Duration: %llu, Cost: %llu\n",
    //    			(long long)loopedThread->tid,
    //    			(long long)loopedThread->cpuid,
    //    			(unsigned long long)ktime_to_us(loopedThread->periodDuration),
    //    			(unsigned long long)loopedThread->cost_us);

	// }
	// if(amountReserved >= 2)
	// {
	// 	struct threadNode *loopedThread = threadHead.head->next;
	// 	printk(KERN_INFO "amount reserved is %d\n", amountReserved);
	// 	printk(KERN_INFO "Thread ID: %lld, CPU ID: %lld, Period Duration: %llu, Cost: %llu\n",
    //    			(long long)loopedThread->tid,
    //    			(long long)loopedThread->cpuid,
    //    			(unsigned long long)ktime_to_us(loopedThread->periodDuration),
    //    			(unsigned long long)loopedThread->cost_us);
	// }
	// if(amountReserved >= 3)
	// {
	// 	struct threadNode *loopedThread = threadHead.head->next->next;
	// 	printk(KERN_INFO "amount reserved is %d\n", amountReserved);
	// 	printk(KERN_INFO "Thread ID: %lld, CPU ID: %lld, Period Duration: %llu, Cost: %llu\n",
    //    			(long long)loopedThread->tid,
    //    			(long long)loopedThread->cpuid,
    //    			(unsigned long long)ktime_to_us(loopedThread->periodDuration),
    //    			(unsigned long long)loopedThread->cost_us);
	// }
	// if(amountReserved >= 4)
	// {
	// 	struct threadNode *loopedThread = threadHead.head->next->next->next;
	// 	printk(KERN_INFO "amount reserved is %d\n", amountReserved);
	// 	printk(KERN_INFO "Thread ID: %lld, CPU ID: %lld, Period Duration: %llu, Cost: %llu\n",
    //    			(long long)loopedThread->tid,
    //    			(long long)loopedThread->cpuid,
    //    			(unsigned long long)ktime_to_us(loopedThread->periodDuration),
    //    			(unsigned long long)loopedThread->cost_us);
	// }
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
	//amountReserved--;

	// if(amountReserved == 0)
	// {
	// 	printk(KERN_INFO "Empty Linked List!\n");
	// }
	// else
	// {
	// 	if(amountReserved >= 1)
	// 	{
	// 		struct threadNode *loopedThread = threadHead.head;
	// 		printk(KERN_INFO "amount reserved is %d\n", amountReserved);
	// 		printk(KERN_INFO "Thread ID: %lld, CPU ID: %lld, Period Duration: %llu, Cost: %llu\n",
	// 				(long long)loopedThread->tid,
	// 				(long long)loopedThread->cpuid,
	// 				(unsigned long long)ktime_to_us(loopedThread->periodDuration),
	// 				(unsigned long long)loopedThread->cost_us);

	// 	}
	// 	if(amountReserved >= 2)
	// 	{
	// 		struct threadNode *loopedThread = threadHead.head->next;
	// 		printk(KERN_INFO "amount reserved is %d", amountReserved);
	// 		printk(KERN_INFO "Thread ID: %lld, CPU ID: %lld, Period Duration: %llu, Cost: %llu\n",
	// 				(long long)loopedThread->tid,
	// 				(long long)loopedThread->cpuid,
	// 				(unsigned long long)ktime_to_us(loopedThread->periodDuration),
	// 				(unsigned long long)loopedThread->cost_us);
	// 	}
	// 	if(amountReserved >= 3)
	// 	{
	// 		struct threadNode *loopedThread = threadHead.head->next->next;
	// 		printk(KERN_INFO "amount reserved is %d\n", amountReserved);
	// 		printk(KERN_INFO "Thread ID: %lld, CPU ID: %lld, Period Duration: %llu, Cost: %llu\n",
	// 				(long long)loopedThread->tid,
	// 				(long long)loopedThread->cpuid,
	// 				(unsigned long long)ktime_to_us(loopedThread->periodDuration),
	// 				(unsigned long long)loopedThread->cost_us);
	// 	}
	// 	if(amountReserved >= 4)
	// 	{
	// 		struct threadNode *loopedThread = threadHead.head->next->next->next;
	// 		printk(KERN_INFO "amount reserved is %d\n", amountReserved);
	// 		printk(KERN_INFO "Thread ID: %lld, CPU ID: %lld, Period Duration: %llu, Cost: %llu\n",
	// 				(long long)loopedThread->tid,
	// 				(long long)loopedThread->cpuid,
	// 				(unsigned long long)ktime_to_us(loopedThread->periodDuration),
	// 				(unsigned long long)loopedThread->cost_us);
	// 	}
	//}
	unlockScheduleLL();

	return output;
}


// Requires locking struct
struct threadNode *findThreadInScheduleLL(struct task_struct *task){
	struct threadNode *loopedThread;
	pid_t tid;

	loopedThread = threadHead.head;
	tid = task->pid;

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
