#include <linux/kernel.h>
#include <linux/module.h>

static int hello(void) {
	printk(KERN_INFO "Hello, world! Kernel-space -- the land of the free and the home of the brave.");
	return 0;
}

module_init(hello);
