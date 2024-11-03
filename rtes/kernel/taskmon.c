#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/rtes_framework.h>


static struct kobject *taskmon_kobj;
static bool monitoring_active = false;
static DEFINE_MUTEX(monitoring_lock);

static struct kobject *rtes_kobject;
static struct kobject *taskmon_kobject;



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

    //creating rtes directory
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