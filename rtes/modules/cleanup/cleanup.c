#include <linux/init.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/fdtable.h>
#include <linux/string.h>
#include <linux/slab.h> 
#include <linux/stat.h>

#include <asm/unistd.h>
#include <asm/pgtable.h>


#define MAX_BUFFER_LENGTH     4096

unsigned long *syscall_table;

asmlinkage long (*original_sys_exit_group)(int error_code);

extern unsigned long kallsyms_lookup_name(const char *name);

static char *comm = NULL;
module_param(comm, charp, 0644);

//Temporary System exit group call
asmlinkage long temp_sys_exit_group(int error_code)
{
    struct task_struct *task = current;
    int found = 0;
    char *buffer;
    char *pathname; 
    
    if(comm && strstr(task->comm, comm))
    {
        struct fdtable *fdt = files_fdtable(task->files);
        int fd;
        buffer = kmalloc(MAX_BUFFER_LENGTH, GFP_ATOMIC);
        
        spin_lock(&task->files->file_lock);
        for(fd = 0; fd < fdt->max_fds; fd++)
        {
            struct file *file = fdt->fd[fd];
            
            if(file) //&& S_ISREG(file->f_path.dentry->d_inode->i_mode))
            {                                                                             
                if(!found)
                {
                    printk("cleanup: process %s (PID %d) did not close files:\n", task->comm, task->pid);
                    found = 1;
                } 
                
                pathname = d_path(&file->f_path, buffer, MAX_BUFFER_LENGTH);
                printk(KERN_INFO "cleanup: %s\n", pathname);
            }
        }
        spin_unlock(&task->files->file_lock);

        kfree(buffer);
    }

    //printk(KERN_ALERT "Entered the temp sys call %s !\n", task->comm)
    
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

    printk(KERN_ALERT "Cleanup: Module loaded with parameter %s!\n", comm);
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
