#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h> 

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


static struct psdev_device_data *characterDriverDevice;
dev_t devNumber;

static int psdev_open(struct inode *inode, struct file *file)
{
    struct psdev_device_data *device = container_of(inode->i_cdev, struct psdev_device_data, cdev);
    
    if(device->is_open)
    {
        return -EBUSY;
    }
    
    device->is_open = 1;
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
    return -1;
}

static ssize_t psdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    return -1;
}

static ssize_t psdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    return -1;
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
