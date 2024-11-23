#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/rtes_framework.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>



// Reading 'enabled' attribute - shows if monitoring is active or not
static ssize_t reservationStatus_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    struct threadNode *loopedThread = threadHead.head;
    struct task_struct *task = NULL;
    int len = 0;

    if(loopedThread != NULL)
    {
        len += sprintf(buf + len, "TID\tPID\tPRIO\tCPU\tNAME \n");
        while(loopedThread != NULL)
        {
            rcu_read_lock();
            task = find_task_by_vpid(loopedThread->tid);
            

            printk(KERN_INFO "tid is: %d\n",loopedThread->tid); 
            if (task)
            {
                // Get the CPU affinity and other details
                len += sprintf(buf + len, "%d\t%d\t%d\t%d\t%s \n",
                           loopedThread->tid,             // Thread ID
                           task_tgid_nr(task),            // Process ID
                           task->rt_priority,             // Real-time Priority
                           loopedThread->cpuid,           // CPU ID
                           task->comm);                   // Command Name
            }
            else
            {
                printk(KERN_INFO "Thread with TID= %d not found\n", loopedThread->tid);
            }
            
            rcu_read_unlock();
            loopedThread = loopedThread->next;
        }

        return len;
    }
    else
    {
        return sprintf(buf, "No Active Tasks in the Reservation!\n");
    }

    
}

static struct kobj_attribute reservationStatus_attribute =__ATTR(reserves, 0660, reservationStatus_show, NULL);

// Initialize sysfs entry at kernel boot
void reservationStatus_init(void) 
{

    int error = 0;
    error = sysfs_create_file(rtes_kobject, &reservationStatus_attribute.attr);
    if(error)
    {
        printk(KERN_INFO "Unable to create the reservation status file!\n"); 
    }

}


void reservationStatus_exit(void) 
{
   sysfs_remove_file(rtes_kobject, &reservationStatus_attribute.attr);
}