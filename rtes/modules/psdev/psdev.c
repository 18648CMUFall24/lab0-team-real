#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h> 
#include <linux/slab.h>
#include <linux/sched.h>
//#include <linux/sched/signal.h>
#include <linux/rcupdate.h>  // For RCU lockin


#define BUFFER_SIZE    4096
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
    int is_open;  //Flag to indicate when the device is open
};


static struct psdev_device_data characterDriverDevice;

// sysfs class structure
//static struct class *psdevdev_class = NULL;

dev_t devNumber;

static int psdev_open(struct inode *inode, struct file *file)
{
    //not sure if have to malloc this device
    
    struct psdev_device_data *device  = container_of(inode->i_cdev, struct psdev_device_data, cdev);


    if(device->is_open)
    {
        return -EBUSY;
    }
    
    device->is_open = 1;
    file->private_data = device;
    printk(KERN_INFO "Device opened\n");
    return 0;

}

static int psdev_release(struct inode *inode, struct file *file)
{
    struct psdev_device_data *device = container_of(inode->i_cdev, struct psdev_device_data, cdev);
    device->is_open = 0;
    printk(KERN_INFO "Device closed\n");
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

    buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if(!buffer)
    {
        return -ENOMEM;
    }

    length = length + snprintf(buffer+length,BUFFER_SIZE-length, "tid\tpid\tpr\tname \n");

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
    *offset +=readSize;
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
    printk(KERN_INFO "Hello World 1!");
    //allocate major number
    allocRet = alloc_chrdev_region(&devNumber, 0,1,"psdev");

    printk(KERN_INFO "Data is %d %d\n",allocRet, devNumber);
    if(allocRet != 0)
    {
        return allocRet;
    }

    //Initialize the device
    cdev_init(&characterDriverDevice.cdev, &psdev_fops);
    
    characterDriverDevice.is_open = 0;
    
    //Add the chacater driver
    cdev_add(&characterDriverDevice.cdev,devNumber,1);

    //psdevdev_class = class_create(THIS_MODULE, "psdev");
    //device_create(psdevdev_class, NULL, devNumber, NULL, "psdev");
    
    printk(KERN_INFO "Character device registered with major number %d\n", MAJOR(devNumber));
   
    
    return 0;
    
}

static void __exit psdev_exit(void)
{
    //device_destroy(psdevdev_class,devNumber);
    //class_unregister(psdevdev_class);
    //class_destroy(psdevdev_class);
    cdev_del(&characterDriverDevice.cdev);
    unregister_chrdev_region(devNumber, 1);

    printk(KERN_INFO "Character device unregistered %d\n", MAJOR(devNumber));
}



//MODULE_LICENSE("Dual BSD/GPL");
module_init(psdev_init);
module_exit(psdev_exit);
