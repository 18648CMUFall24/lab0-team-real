#include <linux/rtes_framework.h>
#include <linux/errno.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>


struct threadNode* threadHead = NULL;

SYSCALL_DEFINE4(set_reserve, pid_t, tid, struct timespec*, C , struct timespec*, T , int, cpuid) 
{
	struct threadNode *new_node;
	struct timespec c,t;
	struct cpumask cpumask;

	if(cpuid > 0 && cpuid <= 3)
	{
		return EINVAL;
	}

	if(tid == 0)
	{
		tid = current->pid;
	}

	// Copy from user space
    if (copy_from_user(&c, C, sizeof(struct timespec)) || copy_from_user(&t, T, sizeof(struct timespec))) {
        return -EFAULT;
    }

	new_node = kmalloc(sizeof(struct threadNode), GFP_KERNEL);
	if(!new_node)
	{
		return -ENOMEM;
	}

	//set the nodes with its parameters
	new_node->C = c;
    new_node->T = t;
    new_node->tid = tid;
    new_node->cpuid = cpuid;


	// Setting up CPU affinity using syscall for sched_setaffinity
	cpumask_clear(&cpumask);
    cpumask_set_cpu(cpuid, &cpumask);

	//bin TID against a specific CPU
	if(sched_setaffinity(tid, &cpumask) == -1)
	{
		kfree(new_node);
		return -1;
	}


	//setting the thread head and its next
	new_node->next = threadHead;
    threadHead = new_node; // Insert into linked list



	return 0;
}

SYSCALL_DEFINE1(cancel_reserve, pid_t, tid) 
{
	struct threadNode *loopedThread = threadHead;
    struct threadNode *prev = NULL;

    // If tid is 0, use current thread's pid
    if (tid == 0) {
        tid = current->pid;
    }

	while(loopedThread != NULL)
	{
		if(loopedThread->tid == tid)
		{
			//remove from linked List
			if(prev != NULL)
			{	
				prev->next = loopedThread->next;
			}
			else
			{
				threadHead = loopedThread->next;
			}
			kfree(loopedThread);
			return 0;
		}

		prev = loopedThread;
		loopedThread = loopedThread->next;
	}

	return -1;
}