#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/rtes_framework.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>

//Total power consumption sysfs File 
static ssize_t power_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    return sprintf(buf, "No Active Tasks in the Reservation!\n");
}
static struct kobj_attribute power_attribute =__ATTR(power, 0660, power_show, NULL);

// processor frequency sysfs File 
static ssize_t freq_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    return sprintf(buf, "No Active Tasks in the Reservation!\n");
}
static struct kobj_attribute frequency_attribute =__ATTR(freq, 0660, freq_show, NULL);


//total energy consumed sysfs File
static ssize_t energy_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    return sprintf(buf, "No Active Tasks in the Reservation!\n");
}
static struct kobj_attribute energy_attribute =__ATTR(energy, 0660, energy_show, NULL);



// Create sysfs files for 
void energyTracking_init(void) 
{

    //Frequency Attribute sys file creation
    int error = 0;
    error = sysfs_create_file(rtes_kobject, &frequency_attribute.attr);
    if(error)
    {
        printk(KERN_INFO "Unable to create the frequency file!\n"); 
    }

    //Power Attribute sys file creation
    error = sysfs_create_file(rtes_kobject, &power_attribute.attr);
    if(error)
    {
        printk(KERN_INFO "Unable to create the frequency file!\n"); 
    }

    //Energy Attribute sys file creation
    error = sysfs_create_file(rtes_kobject, &energy_attribute.attr);
    if(error)
    {
        printk(KERN_INFO "Unable to create the frequency file!\n"); 
    }




}


void energyTracking_exit(void) 
{
   sysfs_remove_file(rtes_kobject, &frequency_attribute.attr);
}
