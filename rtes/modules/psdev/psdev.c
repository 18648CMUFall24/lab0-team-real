#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h> 
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/rcupdate.h>  // For RCU lockin
#include <linux/mutex.h>


#define BUFFER_SIZE    4096
#define DEIVCE_AMOUNT  16 

//File Operation 
static int psdev_open(struct inode *inode, struct file *file);
static int psdev_release(struct inode *inode, struct file *file);
static long psdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t psdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t psdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);


 const struct file_operations psdev_fops = {
    .owner      = THIS_MODULE,
    .open       = psdev_open,
    .release    = psdev_release,
    .unlocked_ioctl = psdev_ioctl,
    .read       = psdev_read,
    .write       = psdev_write
};


struct psdev_device_data{
    struct cdev cdev;
    struct mutex lock;
    int is_open;  //Flag to indicate when the device is open
};


static struct psdev_device_data characterDriverDevice[DEIVCE_AMOUNT];

dev_t devNumber;

static int psdev_open(struct inode *inode, struct file *file)
{
    //not sure if have to malloc this device
    
    struct psdev_device_data *device;
    int minor = iminor(inode);

    device = &characterDriverDevice[minor];

    mutex_lock(&device->lock);

    if(device->is_open)
    {
        mutex_unlock(&device->lock);
        return -EBUSY;
    }
    
    device->is_open = 1;
    file->private_data = device;

    mutex_unlock(&device->lock);
    printk(KERN_INFO "Device number %d opened\n", minor);
    return 0;

}

static int psdev_release(struct inode *inode, struct file *file)
{
    struct psdev_device_data *device = file->private_data;
    int minor = iminor(inode);

    mutex_lock(&device->lock);
    device->is_open = 0;
    mutex_unlock(&device->lock);
    printk(KERN_INFO "Device number %d closed\n", minor);
    return 0;
}

static long psdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return -ENOTSUPP;
}

static ssize_t psdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    struct task_struct *task;
    ssize_t length = 0;
    char *buffer; 
    size_t remainingSize = 0;
    ssize_t readSize = count;
    
    // Check if offset is beyond buffer size
    if (*offset >= BUFFER_SIZE) {
        //printk(KERN_INFO "Offset exceeds buffer size\n");
        buf[0] = '\0';
        return 0;
    }

    buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if(!buffer)
    {
        return -ENOMEM;
    }

    length = length + snprintf(buffer+length,BUFFER_SIZE-length, "tid\tpid\tpr\tname \n");

    //printk(KERN_INFO "count is %d and Offset value: %lld\n",count, (long long)*offset);

    //lock to read the task list
    rcu_read_lock();

    //go through each
    for_each_process(task)
    {
        if(task->rt_priority > 0)
        {
            length = length + snprintf(buffer+length,BUFFER_SIZE-length, "%d\t%d\t%d\t%s \n",
                                    task->tgid, task->pid,task->rt_priority, task->comm);
        }

    }
    
    //set the null character 
    if(length < BUFFER_SIZE)
    {
        buffer[length] = '\0';
    }
    else
    {
        buffer[BUFFER_SIZE-1] = '\0';
    }
    //unlock since done reading the task list
    rcu_read_unlock();

    //copy only specific points from buffer
    remainingSize = length - *offset;

    //check to see if at the end of file or there is no data
    if(remainingSize <= 0)
    {
        kfree(buffer);
        return 0;
    }

    //check to see if data requested is less then required
    if((count > remainingSize))
    {
        readSize = remainingSize;
    }

    // Copy data from kernel space (device buffer) to user space
    if(copy_to_user(buf, buffer + *offset, readSize))
    {
        return -EFAULT;  // Return error if copy fails
    }

    //increment the offset
    if(*offset+readSize > BUFFER_SIZE )
    {
        *offset = BUFFER_SIZE;
    }
    else
    {
        *offset +=readSize;
    }

    kfree(buffer);
    return readSize;
}

static ssize_t psdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{   

    return -ENOTSUPP;
}


//Device Node 
static int __init psdev_init(void)
{
    int allocRet;
    int loop;
    //allocate major number
    allocRet = alloc_chrdev_region(&devNumber, 0,DEIVCE_AMOUNT,"psdev");

    if(allocRet != 0)
    {
        return allocRet;
    }

    for(loop=0; loop < DEIVCE_AMOUNT; loop++)
    {
        //Initialize the device
        cdev_init(&characterDriverDevice[loop].cdev, &psdev_fops);
        
        characterDriverDevice[loop].is_open = 0;

        mutex_init(&characterDriverDevice[loop].lock);

        //Add the chacater driver
        cdev_add(&characterDriverDevice[loop].cdev,MKDEV(MAJOR(devNumber), loop),1);


        
        printk(KERN_INFO "Character device registered with major number %d and minor number %d\n", MAJOR(devNumber),loop);
    }
    
    return 0;
    
}

static void __exit psdev_exit(void)
{
    int loop;
    for (loop = 0; loop < DEIVCE_AMOUNT; loop++)
    {
        cdev_del(&characterDriverDevice[loop].cdev);
        
    }

    unregister_chrdev_region(devNumber, DEIVCE_AMOUNT);
    printk(KERN_INFO "Devices unregistered\n");
}



MODULE_LICENSE("Dual BSD/GPL");
module_init(psdev_init);
module_exit(psdev_exit);
