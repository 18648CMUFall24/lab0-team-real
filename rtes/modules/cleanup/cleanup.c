#include <linux/init.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>


unsigned long *syscall_table = NULL;

asmlinkage long (*orginal_sys_exit_group)(int error_code);


asmlinkage long temp_sys_exit_group(int error_code)
{


    return orginal_sys_exit_group(error_code);
}


static unsigned long **find_syscall_table(void)
{
  unsigned long int offset = PAGE_OFFSET;
  unsigned long **sct;

  while (offset < ULLONG_MAX) {
    sct = (unsigned long **)offset;

    if (sct[__NR_close] == (unsigned long *) sys_close)
      return sct;

    offset += sizeof(void *);
  }

  return NULL;
}

static int cleanup_init(void)
{
    syscall_table = (void **)find_syscall_table();
    if(syscall_table == NULL)
    {
        printk(KERN_ERR "Syscall table not found!\n");
        return -1;
    }

    write_cr0(read_cr0() & ~0x00010000);
    orginal_sys_exit_group = (void *)syscall_table[__NR_exit_group];

    syscall_table[__NR_exit_group] = (unsigned long *)temp_sys_exit_group;    
    write_cr0(read_cr0() | 0x00010000);

    printk(KERN_ALERT "Cleanuo: Module loaded!.\n");
    return 0;
}
static void cleanup_exit(void)
{
    if(syscall_table == NULL)
    {
        printk(KERN_ERR "Syscall table not found!\n");
        return;
    }

    write_cr0(read_cr0() & ~0x00010000);
    syscall_table[__NR_exit_group] = orginal_sys_exit_group;
    write_cr0(read_cr0() | 0x00010000);

    printk(KERN_ALERT "Cleanuo: Module unloaded!.\n");
}
module_init(cleanup_init);
module_exit(cleanup_exit);
MODULE_LICENSE("Dual BSD/GPL");
