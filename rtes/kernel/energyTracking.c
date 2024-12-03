#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/rtes_framework.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/cpufreq.h>
#include <linux/syscalls.h>

static struct kobject *config_kobject;
static struct kobject *task_kobject;
static bool energyMonitor = false;
char Power[64] = "0";
static unsigned long Freq = 0;
char TotalEnergy[64] = "0";


void energyCalc_init(void)
{
    // unsigned long freq_khz = cpufreq_quick_get(raw_smp_processor_id());
    // char frequencyKhzString[16];   // Store frequency in kHz as string
    // char frequencyMhzString[16] = {};   // Store converted frequency in MHz as string
    char temp_result1[64] = {};
    char temp_result2[64] = {};
    char constant_k[64] = "0.00442";  // κ value (constant for power model)
    char constant_beta[64] = "25.72"; // β value (constant for power model)
    char constant_intermediate1[64] = "102329.2992";
    int ret;


   unsigned long freq_khz = cpufreq_quick_get(raw_smp_processor_id());
   
    //convert to Mhz
    printk(KERN_INFO "Before calclated is %lu!\n",freq_khz);
	Freq = div64_u64(freq_khz,1000);


    // Step 5: Compute power P(f) = κ * f^1.67 + β
    // Multiply κ (constant_k) by f^1.67 (intermediate1)
    ret = sys_calc(constant_k, constant_intermediate1, '*', temp_result1);
    if (ret < 0) {
        printk(KERN_INFO "Failed to compute κ * f^1.67\n");
        return;
    }

    // Add β (constant_beta) to get total power: P(f) = κ * f^1.67 + β
    ret = sys_calc(temp_result1, constant_beta, '+', temp_result2);
    if (ret < 0) {
        printk(KERN_INFO "Failed to compute total power\n");
        return;
    }

    //intermediate1` now holds the final power value in mW
    printk(KERN_INFO "Total power consumption: %s mW\n", temp_result2);
    strcpy(Power,temp_result2);

    
}
void energyCalc(struct threadNode *task)
{
    unsigned long elapsed_time;
    char elapsedTimeString[10];
    char energyConsumption[64] = {};
    char TotalenergyConsumption[64] = {};
    int ret;

    if(energyMonitor)
    {
        elapsed_time = ktime_to_ms(ktime_sub(ktime_get(), task->startTimer));
        sprintf(elapsedTimeString, "%lu", elapsed_time);

        printk(KERN_INFO "Elapsed time has been %lu", elapsed_time);
        //Calculate Energy consumption for a thread
        ret = sys_calc(Power, elapsedTimeString, '*', energyConsumption);
        if (ret < 0) {
            printk(KERN_INFO "Failed To calculate Energy Conumption for thread %dn",task->tid);
            return;
        }
        printk(KERN_INFO "Energy consumption for thread %d is: %s mW\n", task->tid, energyConsumption);
        strcpy(task->energyData.energy,energyConsumption);

        //increment total energy consumption of the total system!
        ret = sys_calc(TotalEnergy, energyConsumption, '+', TotalenergyConsumption);
        if (ret < 0) {
            printk(KERN_INFO "Failed to add to toal energy with %s for %dn",TotalEnergy, task->tid);
            return;
        }
        printk(KERN_INFO "Total Energy consumption is: %s mW\n", TotalenergyConsumption);
        strcpy(TotalEnergy,TotalenergyConsumption);
        

    }
	
}

//Total power consumption sysfs File 
static ssize_t power_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    //unsigned int freq_mhz = get_cpu_freq_mhz();
   // unsigned long power_mw = 
    return sprintf(buf, "%s\n", Power);
}
static struct kobj_attribute power_attribute =__ATTR(power, 0660, power_show, NULL);

// processor frequency sysfs File 
static ssize_t freq_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    return sprintf(buf, "%lu\n", Freq);
}
static struct kobj_attribute frequency_attribute =__ATTR(freq, 0660, freq_show, NULL);


//total energy consumed sysfs File
static ssize_t energy_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    int count = 0;
    char dataReset[64] = "0";
    if(energyMonitor)
    {
        
        count = sprintf(buf, "%s\n", TotalEnergy);
        strcpy(TotalEnergy,dataReset);
        return count;
    }
    else
    {
        return sprintf(buf, "Energy Monitor not enabled!\n");
    }
}
static struct kobj_attribute energy_attribute =__ATTR(energy, 0660, energy_show, NULL);


//total energy consumed sysfs File
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
    count = sprintf(buf, "%s\n",loopedThread->energyData.energy);
    return count;
}
static struct kobj_attribute taskenergy_attribute =__ATTR(energy, 0660, taskenergy_show, NULL);

//Setting or clearing the energy monitoring functionality
static ssize_t config_energy_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    char dataReset[64] = "0";
    if (buf[0] == '1') {
        energyMonitor = true;
        // Start data collection for threads with active reservations
    } else if (buf[0] == '0') {
        energyMonitor = false;
        strcpy(TotalEnergy,dataReset);
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
    char dataReset[64] = "0";

    snprintf(data, 16, "%d", thread->tid);

    //initializing the data in the energy
    strcpy(thread->energyData.energy,dataReset);


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
