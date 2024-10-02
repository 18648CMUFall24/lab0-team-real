#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h> 
#include <linux/slab.h>


#define BUFFER_SIZE    1024
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
    size_t buffer_size;
    size_t  fileSize;
    char *buffer;

};


static struct psdev_device_data *characterDriverDevice;
dev_t devNumber;

static int psdev_open(struct inode *inode, struct file *file)
{
    //not sure if have to malloc this device
    
    struct psdev_device_data *device  = container_of(inode->i_cdev, struct psdev_device_data, cdev);


    if(device->is_open)
    {
        return -EBUSY;
    }
    
    //allocate memory for the buffer
    device->buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!device->buffer) {
        printk(KERN_INFO "psdev: Failed to allocate memory for buffer\n");
        return -ENOMEM;  // Return an error if buffer allocation fails
    }

    device->buffer_size = BUFFER_SIZE;
    device->fileSize = 0;  
    device->is_open = 1;
    file->private_data = device;
    printk(KERN_INFO "Device opened\n");
    return 0;

}

static int psdev_release(struct inode *inode, struct file *file)
{
    struct psdev_device_data *device = container_of(inode->i_cdev, struct psdev_device_data, cdev);
    device->is_open = 0;
    kfree(device->buffer);
    printk(KERN_INFO "Device closed\n");
    return 0;
}

static long psdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return -1;
}

static ssize_t psdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    struct psdev_device_data *device = file->private_data;
    size_t remainingSize = device->fileSize - *offset;
    ssize_t readSize = count;
    
    //check to see if at the end of file
    if(remainingSize <= 0)
    {
        return 0;
    }

    //check to see if data requested is less then required
    if((count > remainingSize))
    {
        readSize = remainingSize;
    }
    

    // Copy data from kernel space (device buffer) to user space
    if (copy_to_user(buf, device->buffer + *offset, readSize)) {
        return -EFAULT;  // Return error if copy fails
    }

    //increment the offset
    *offset +=readSize;

    return readSize;
}

static ssize_t psdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{   
    struct psdev_device_data *device = file->private_data;
    size_t remainingSize = device->buffer_size - device->fileSize;
    ssize_t writeSize = count;
    
    //check to see if there enough space
    if(count > remainingSize)
    {
        writeSize = remainingSize;
    }

    // Copy data from user space to the device buffer
    if (copy_from_user(device->buffer + device->fileSize, buf, writeSize)) {
        return -EFAULT;
    }
    

    device->fileSize += writeSize;

    return writeSize;
}



static int psdev_init(void)
{
    int allocRet;

    //allocate major number
    allocRet = alloc_chrdev_region(&devNumber, 0,1,"psdev");

    if(allocRet != 0)
    {
        return allocRet;
    }

     //Initialize the device
    cdev_init(&characterDriverDevice->cdev, &psdev_fops);
    characterDriverDevice->is_open = 0;

    //Add the chacater driver
    cdev_add(&characterDriverDevice->cdev,devNumber,1);


    printk(KERN_INFO "Character device registered with major number %d\n", MAJOR(devNumber));
    return 0;
    
}

static void psdev_exit(void)
{
    cdev_del(&characterDriverDevice->cdev);
    printk(KERN_INFO "Character device unregistered %d\n", MAJOR(devNumber));
}



MODULE_LICENSE("Dual BSD/GPL");
module_init(psdev_init);
module_exit(psdev_exit);
