#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/rtes_framework.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/syscalls.h>

static struct kobject *config_kobject;
static struct kobject *task_kobject;
static ktime_t monitorTimer;
static bool energyMonitor = false;
static 
unsigned long Power = 0;   //In mWatts
static unsigned long Freq = 0;   //In MHz
unsigned long TotalEnergy = 0;  //In mJ
unsigned long prevEnergy = 0;
unsigned long calcEnergy = 0;
static unsigned long PowerTable[12] = {28086, 35072, 57053, 100036, 156019, 240038, 311073, 377031, 478002,
                                       556005, 638099, 726070};
/*
28.86 mW
35.72 mW
57.53 mW
100.36 mW
156.19 mW
240.38 mW
311.73 mW
377.31 mW
478.02 mW
556.05 mW
638.99 mW
726.70 mW
*/

void energyCalc_init(void)
{
    struct cpufreq_frequency_table *pos, *driver_freq_table;
    struct cpufreq_policy *policy;
    unsigned long freq_khz = cpufreq_quick_get(raw_smp_processor_id());
    int tableIndex = 0;
    

    Freq = div64_u64(freq_khz,1000);
    // Get the policy for CPU 0
    policy = cpufreq_cpu_get(0);
    if (!policy) {
        printk(KERN_INFO "Failed to retrieve policy for CPU 0.\n");
        return;
    }

    // Retrieve the frequency table from the driver via driver_data
    driver_freq_table = cpufreq_frequency_get_table(policy->cpu);
    if (!driver_freq_table) {
        printk(KERN_INFO "No frequency table found for CPU 0.\n");
        cpufreq_cpu_put(policy);
        return;
    }

    // Iterate over the valid frequency table entries
    printk(KERN_INFO "Printing all valid frequencies in the driver frequency table:\n");
    for (pos = driver_freq_table; pos->frequency != CPUFREQ_TABLE_END; pos++) {
        if (pos->frequency == CPUFREQ_ENTRY_INVALID)
            continue;

        if(freq_khz == pos->frequency)
        {
            printk(KERN_INFO "Frequency: %u kHz\n", pos->frequency);
            Power = PowerTable[tableIndex];
            Power = div64_u64(Power,1000);
            
            break;
        }
        tableIndex++;
        //printk(KERN_INFO "Frequency: %u kHz\n", pos->frequency);
    }
    printk(KERN_INFO "Finished printing frequencies.\n");

    cpufreq_cpu_put(policy); // Release the policy after use
    return;
    
}
void energyCalc(struct threadNode *task)
{
    unsigned long elapsed_time;
    unsigned long elapsed_timeMonitor;
    unsigned long elapsed_timeMonitor_seconds;
    unsigned long elapsed_time_seconds;
    unsigned long diff;

    if(energyMonitor)
    {
        elapsed_time = ktime_to_ms(ktime_sub(ktime_get(), task->startTimer));
        
        //Elapsed time since montior reset
        elapsed_timeMonitor = ktime_to_ms(ktime_sub(ktime_get(), monitorTimer));

        //Convert to watt
        elapsed_time_seconds = div64_u64(elapsed_time,1000);
        elapsed_timeMonitor_seconds =div64_u64(elapsed_timeMonitor,1000);
        //printk(KERN_INFO "Thread ran: %lu kHz\n", elapsed_time_seconds);
        //printk(KERN_INFO "Monitor session ran: %lu kHz\n", elapsed_timeMonitor_seconds);

        
        //set Previous energy
        prevEnergy = calcEnergy;

        //calculate energy
        task->energyData.energy = elapsed_time_seconds * Power;
        calcEnergy = elapsed_timeMonitor_seconds * Power;
        //printk(KERN_INFO "Energy calculated for thread is: %lu mJ\n", task->energyData.energy);

        //calculate the difference 
        diff = calcEnergy - prevEnergy;
        //printk(KERN_INFO "Energy difference for thread is: %lu mJ\n", diff);
    
        //Increment Total Energy
        TotalEnergy += diff;

    }
    else
    {
        TotalEnergy = 0;
    }
	
}

//Total power consumption sysfs File 
static ssize_t power_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    //unsigned int freq_mhz = get_cpu_freq_mhz();
   // unsigned long power_mw = 
    return sprintf(buf, "%lu\n", Power);
}
static struct kobj_attribute power_attribute =__ATTR(power, 0660, power_show, NULL);

// processor frequency sysfs File 
static ssize_t freq_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    return sprintf(buf, "%lu\n", Freq);
}
static struct kobj_attribute frequency_attribute =__ATTR(freq, 0660, freq_show, NULL);


static ssize_t  energy_reset(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    
    monitorTimer = ktime_get();
    TotalEnergy = 0;
    calcEnergy = 0;
    return count; 
}


//total energy consumed sysfs File
static ssize_t energy_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    int count = 0;
    if(energyMonitor)
    {
        
        count = sprintf(buf, "%lu\n", TotalEnergy);
        TotalEnergy = 0;
        return count;
    }
    else
    {
        return sprintf(buf, "Energy Monitor not enabled!\n");
    }
}
static struct kobj_attribute energy_attribute =__ATTR(energy, 0660, energy_show, energy_reset);


//individual energy consumed sysfs File
static ssize_t taskenergy_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    struct threadNode *loopedThread = threadHead.head;
    int extractTid;
    int ret = kstrtoint(kobj->name, 10, &extractTid);
    size_t count = 0;

    lockScheduleLL();
    if (ret != 0) {
        printk(KERN_ERR "Failed to convert kobj name to integer, error: %d\n", ret);
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

    //check if node is found!
    if(loopedThread != NULL)
    {
        //Do Nothing
    }
    else
    {
        unlockScheduleLL();
        return sprintf(buf, "Thread not found!\n");
    }

    unlockScheduleLL();
    if(energyMonitor)
    {
        count = sprintf(buf, "%lu\n",loopedThread->energyData.energy);
    }
    else
    {
        count = sprintf(buf, "Energy Monitor not enabled!\n");
    }
    return count;
}
static struct kobj_attribute taskenergy_attribute =__ATTR(energy, 0660, taskenergy_show, NULL);

//Setting or clearing the energy monitoring functionality
static ssize_t config_energy_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    if (buf[0] == '1') {
        energyMonitor = true;
        monitorTimer = ktime_get();
        // Start data collection for threads with active reservations
    } else if (buf[0] == '0') {
        energyMonitor = false;
        TotalEnergy = 0;
        calcEnergy = 0;
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

    //initializing the data in the energy
    thread->energyData.energy = 0;


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

    monitorTimer = ktime_get();



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
