#include <linux/init.h>
#include <linux/module.h>
MODULE_LICENSE("Dual BSD/GPL");
static int cleanup_init(void)
{
 printk(KERN_ALERT "Hello, world! Kernel-space -- the land of the free and home of the brave.\n");
 return 0;
}
static void cleanup_exit(void)
{
 printk(KERN_ALERT "Goodbye, cruel world\n");
}
module_init(cleanup_init);
module_exit(cleanup_exit);
