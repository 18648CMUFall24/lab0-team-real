#include <linux/init.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>

#include <asm/unistd.h>
#include <asm/pgtable.h>

unsigned long *syscall_table;

asmlinkage long (*original_sys_exit_group)(int error_code);

extern unsigned long kallsyms_lookup_name(const char *name);


//Temporary System exit group call
asmlinkage long temp_sys_exit_group(int error_code)
{
    printk(KERN_ALERT "Entered the temp sys call!\n");
    return original_sys_exit_group(error_code);
}

static int cleanup_init(void)
{
    // Use kallsyms_lookup_name to find the sys_call_table
    syscall_table = (unsigned long *)kallsyms_lookup_name("sys_call_table");
    
    if (syscall_table == NULL) {
        printk(KERN_ERR "Syscall table not found!\n");
        return -1;
    }

    // Save the original sys_exit_group and replace it
    original_sys_exit_group = (void *)syscall_table[__NR_exit_group];
    
    // Cast function pointer to unsigned long when assigning to syscall_table
    syscall_table[__NR_exit_group] = (unsigned long)temp_sys_exit_group;

    printk(KERN_ALERT "Cleanup: Module loaded!\n");
    return 0;
}

static void cleanup_exit(void)
{
    // Restore the original sys_exit_group
    syscall_table = (unsigned long *)kallsyms_lookup_name("sys_call_table");
    
    if (syscall_table == NULL) {
        printk(KERN_ERR "Syscall table not found!\n");
        return;
    }

    // Cast function pointer to unsigned long when restoring original syscall
    syscall_table[__NR_exit_group] = (unsigned long)original_sys_exit_group;

    printk(KERN_ALERT "Cleanup: Module unloaded!\n");
}

module_init(cleanup_init);
module_exit(cleanup_exit);

MODULE_LICENSE("Dual BSD/GPL");
