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

#define K          4420  // 0.00442 * 1000
#define A          1670  // 1.67 * 1000
#define B          25720 // 25.72 * 1000
#define FIXED_POINT_SCALE 1000  // Scaling factor

static struct kobject *config_kobject;
static struct kobject *task_kobject;
static bool energyMonitor = false;
static unsigned long Power = 0;
static unsigned long Freq = 0;


// Fixed-point multiplication
unsigned long fixed_point_mul(unsigned long a, unsigned long b) {
    return (a * b) / FIXED_POINT_SCALE;
}

// Fixed-point division
unsigned long fixed_point_div(unsigned long a, unsigned long b) {
    return (a * FIXED_POINT_SCALE) / b;
}

// Exponentiation for f^alpha (approximated with integer exponentiation)
unsigned long fixed_point_pow(unsigned long base, unsigned long exponent) {
    unsigned long result = FIXED_POINT_SCALE;  // Start with 1 in fixed-point
    unsigned long temp_base = base;

    while (exponent > 0) {
        if (exponent % 2 == 1) {
            result = fixed_point_mul(result, temp_base);
        }
        temp_base = fixed_point_mul(temp_base, temp_base);  // Square the base
        exponent /= 2;
    }

    return result;
}

void energyCalc_init(void)
{
    unsigned long freq_khz = cpufreq_quick_get(raw_smp_processor_id());
    unsigned long freq_to_alpha;

   
    //convert to Mhz
    printk(KERN_INFO "Before calclated is %lu!\n",freq_khz);
	Freq = div64_u64(freq_khz,1000);
    printk(KERN_INFO "Frequency calclated is %lu!\n",Freq);

    // Compute f^alpha (where alpha is 1.67 in fixed-point form as A)
    freq_to_alpha = fixed_point_pow(Freq, A);  // freq_mhz^1.67

    // Calculate P(f) = K * f^alpha + B
    Power = fixed_point_mul(K, freq_to_alpha);  // K * f^alpha
    Power = fixed_point_div(Power, FIXED_POINT_SCALE);  // Scale down after multiplication
		
    Power += B;  // Add B to the result

	
}
void energyCalc(struct threadNode *task)
{
	
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
