#include <linux/rtes_framework.h>
#include <linux/errno.h>
#include <linux/syscalls.h>


SYSCALL_DEFINE4(set_reserve, pid_t, tid, struct timespec*, C , struct timespec*, T , int, cpuid) 
{
	return 1;
}

SYSCALL_DEFINE1(cancel_reserve, pid_t, tid) 
{
	return 1;
}