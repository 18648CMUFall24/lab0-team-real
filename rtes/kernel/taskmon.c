#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/rtes_framework.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>




static struct kobject *taskmon_kobj;
bool monitoring_active = false;
static DEFINE_MUTEX(monitoring_lock);

static struct kobject *rtes_kobject;
static struct kobject *taskmon_kobject;
static struct kobject *util_kobject;

// Function to show utilization for task per data point
static ssize_t util_file_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    int extractTid;
    struct threadNode *loopedThread = threadHead.head;
    int ret = kstrtoint(attr->attr.name, 10, &extractTid);
    size_t offset = 0;
    size_t count = 0;

    
    lockScheduleLL();
    if(monitoring_active)
    {
        if (ret != 0) {
            printk(KERN_ERR "Failed to convert attr name to integer, error: %d\n", ret);
            unlockScheduleLL();
            return count; // Return the error code if conversion fails
        }

        //Go through to see if the node is in there
        
        while(loopedThread != NULL) {
            if(loopedThread->tid == extractTid)
            {
                break;
            }
            loopedThread = loopedThread->next;
        }
       

        if(loopedThread != NULL)
        {

        }
        else
        {
            unlockScheduleLL();
            return sprintf(buf, "Thread not found!\n");
        }

        offset = loopedThread->offset;
        strncpy(buf, loopedThread->dataBuffer, loopedThread->offset);
        memset(loopedThread->dataBuffer,0,BUFFER_SIZE);
        loopedThread->offset = 0;
        
        unlockScheduleLL();
        return offset;
    }
    else
    {
        unlockScheduleLL();
        return sprintf(buf, "Monitoring not active!\n");
    }
}

//kstrtoint 

//Create Thread Virtual File for the thread
// Create Thread Virtual File for the thread
int createThreadFile(struct threadNode *thread)
{
    int error = 0;
    struct kobj_attribute *threadAtt;

    // Allocate memory for the kobj_attribute
    threadAtt = kzalloc(sizeof(struct kobj_attribute), GFP_KERNEL);
    if (!threadAtt) {
        return -ENOMEM;
    }

    // Create a name for the kobject using the tid
    threadAtt->attr.name = kmalloc(16, GFP_KERNEL);
    if (!threadAtt->attr.name) {
        kfree(threadAtt);
        return -ENOMEM;
    }
    
    snprintf((char *)threadAtt->attr.name, 16, "%d", thread->tid);
    //printk(KERN_INFO "File name: %s\n", threadAtt->attr.name);

    // Set up the kobj_attribute fields
    threadAtt->attr.mode = 0664;
    threadAtt->show = util_file_show;
    threadAtt->store = NULL;

    // Add the utilization file to the thread's kobject
    error = sysfs_create_file(util_kobject, &threadAtt->attr);
    if (error) {
        printk(KERN_INFO "error with number %d\n", error);
        kfree(threadAtt->attr.name);
        kfree(threadAtt);
        return -1;
    }

    // Store a reference to the attribute in the thread structure if needed
    thread->thread_obj = threadAtt;

    return 0;
}

int removeThreadFile(struct threadNode  *thread)
{
    // Remove the sysfs file associated with the kobject
    sysfs_remove_file(util_kobject, &thread->thread_obj->attr);
    // Remove the kobject
    kfree(thread->thread_obj->attr.name);
    kfree(thread->thread_obj);
    
    return 0;
}


// Reading 'enabled' attribute - shows if monitoring is active or not
static ssize_t monitoring_control_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    if(monitoring_active)
    {
        return sprintf(buf, "%d\n", 1);
    }
    else
    {
        return sprintf(buf, "%d\n", 0);
    }

}


// Writing 'enabled' attribute - starts or stops monitoring
static ssize_t monitoring_control_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) 
{
    mutex_lock(&monitoring_lock);
    if (buf[0] == '1') {
        monitoring_active = true;
        // Start data collection for threads with active reservations
    } else if (buf[0] == '0') {
        monitoring_active = false;
        // Stop data collection and cleanup
        
    }

    mutex_unlock(&monitoring_lock);

    return count;
}

// Define the sysfs attribute
static struct kobj_attribute monitoring_control_attr = __ATTR(enabled, 0664, monitoring_control_show, monitoring_control_store);

// Initialize sysfs entry at kernel boot
static int __init taskmon_init(void) {

    int error = 0;

    //creating rtes directory in the sys directory
    rtes_kobject = kobject_create_and_add("rtes", NULL);
    if(!rtes_kobject)
    {
        return -ENOMEM;
    }
    
    //create taskmon directory
    taskmon_kobject = kobject_create_and_add("taskmon", rtes_kobject);
    if(!taskmon_kobject)
    {
        kobject_put(rtes_kobject);
        return -ENOMEM;
    }

    //create utilization file in the directory
    util_kobject = kobject_create_and_add("util", taskmon_kobject);
    if(!util_kobject)
    {
        kobject_put(taskmon_kobject);
        kobject_put(rtes_kobject);
        return -ENOMEM;
    }

    //creating the file
    error = sysfs_create_file(taskmon_kobject, &monitoring_control_attr.attr);
    if(error)
    {
        kobject_put(taskmon_kobject);
        kobject_put(rtes_kobject);
        return error;
    }

    printk(KERN_INFO "Task Monitor sysfs interface initialized at /sys/rtes/taskmon.\n");
    return 0;
}


// Cleanup sysfs entry on shutdown
static void __exit taskmon_exit(void) {

    sysfs_remove_file(taskmon_kobj, &monitoring_control_attr.attr);
    kobject_put(taskmon_kobj);
    kobject_put(rtes_kobject);
    printk(KERN_INFO "Task Monitor sysfs interface unitialzed and deleted at /sys/rtes/taskmon.\n");
}

module_init(taskmon_init);
module_exit(taskmon_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Kernel space task monitoring sysfs interface");