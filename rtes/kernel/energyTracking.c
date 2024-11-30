#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/rtes_framework.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>


static struct kobject *config_kobject;
static struct kobject *task_kobject;
bool energyMonitor = false;

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


//total energy consumed sysfs File
static ssize_t taskenergy_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    return sprintf(buf, "No Active Tasks in the Reservation!\n");
}
static struct kobj_attribute taskenergy_attribute =__ATTR(energy, 0660, taskenergy_show, NULL);

//Setting or clearing the energy monitoring functionality
static ssize_t config_energy_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    if (buf[0] == '1') {
        energyMonitor = true;
        // Start data collection for threads with active reservations
    } else if (buf[0] == '0') {
        energyMonitor = false;
        // Stop data collection and cleanup
        
    }

    return count;
}
//Showing the status of the energy monitoring
static ssize_t config_energy_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    if(energyMonitor)
    {
        return sprintf(buf, "%d\n", 1);
    }
    else
    {
        return sprintf(buf, "%d\n", 0);
    }
}
static struct kobj_attribute config_energy_attribute = __ATTR(energy, 0664, config_energy_show, config_energy_store);


//To create the energy task file per thread 
int createEnergyThreadFile(struct threadNode *thread)
{
    int error = 0;
    char data[16];

     snprintf(data, 16, "%d", thread->tid);

    //creating rtes directory in the rtes/tasks directory
    thread->energyData.pidFile = kobject_create_and_add(data, task_kobject);

    //Check if directory of the tid can be made
    if(!thread->energyData.pidFile)
    {
        printk(KERN_INFO "Unable to create the task energy tid file %s!\n",data);
        return -1;
    }

    error = sysfs_create_file(thread->energyData.pidFile, &taskenergy_attribute.attr);
    if(error)
    {
        printk(KERN_INFO "Unable to create the task energy file %s!\n",data);
        return -1;
    }

    return 0;

}

//To remove the energy task file per requested thread 
int removeEnergyThreadFile(struct threadNode *thread)
{

    sysfs_remove_file(thread->energyData.pidFile, &frequency_attribute.attr);
    kobject_put(thread->energyData.pidFile);


    return 0;
}

// Create sysfs energy files for the system
void energyTracking_init(void) 
{

    //Frequency Attribute sys file creation
    int error = 0;
    if(rtes_kobject != NULL)
    {
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

        //creating the tasks directory
        task_kobject = kobject_create_and_add("tasks", rtes_kobject);
        if(!task_kobject)
        {
            printk(KERN_INFO "Unable to create the Tasks Directory\n"); 
        }

        //Create the config directory
        config_kobject = kobject_create_and_add("config", rtes_kobject);
        if(!config_kobject)
        {
            printk(KERN_INFO "Unable to create the Config Directory\n");
            return; 
        }

         //Create the config energy file
        error = sysfs_create_file(config_kobject, &config_energy_attribute.attr);
        if(error)
        {
            printk(KERN_INFO "Unable to create the config energy file!\n"); 
        }
        
    }



}


//Rmove the sysfs energy file for the system
void energyTracking_exit(void) 
{
    sysfs_remove_file(rtes_kobject, &frequency_attribute.attr);
    sysfs_remove_file(rtes_kobject, &power_attribute.attr);
    sysfs_remove_file(rtes_kobject, &energy_attribute.attr);
    sysfs_remove_file(rtes_kobject, &config_energy_attribute.attr);
    kobject_put(config_kobject);

}
