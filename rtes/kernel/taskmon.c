#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/rtes_framework.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>


#define BUFFER_SIZE    4096

static struct kobject *taskmon_kobj;
static bool monitoring_active = false;
static DEFINE_MUTEX(monitoring_lock);

static struct kobject *rtes_kobject;
static struct kobject *taskmon_kobject;
static struct kobject *util_kobject;

// Function to show utilization for task per data point
static ssize_t util_file_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    return sprintf(buf, "return data!\n");
    return 0;
}

// Define sysfs attribute for utilization
static struct kobj_attribute utilization_attribute = __ATTR(utilization, 0664, util_file_show, NULL);


//Create Thread Virtual File for the thread
int createThreadFile(struct threadNode *thread)
{
    int error = 0;
    char tid_name[16];

    // Create a name for the kobject using the tid
    snprintf(tid_name, sizeof(tid_name), "%d", thread->tid);

    //Create a kobject for the thread under util directory
    thread->thread_obj = kobject_create_and_add(tid_name, util_kobject);
    if (!thread->thread_obj) {
        return -ENOMEM;
    }


    // Add the utilization file to the thread's kobject
    error = sysfs_create_file(thread->thread_obj, &utilization_attribute.attr);
    if (error) {
        kobject_put(thread->thread_obj); // Clean up on failure
        return -1;
    }


    return 0;
}

int removeThreadFile(struct threadNode  *thread)
{
     // Remove the sysfs file associated with the kobject
    sysfs_remove_file(thread->thread_obj, &utilization_attribute.attr);
    // Remove the kobject
    kobject_put(thread->thread_obj);
    
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