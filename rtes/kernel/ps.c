#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/threads.h>
#include <linux/uaccess.h>

struct realtime_thread_info {
	uint32_t tid;
	uint32_t pid;
	uint32_t rt_priority;
	char name[TASK_COMM_LEN];
};

// Fills realtime_thread_info with a maximum of [len] realtime thread info
// Returns number of realtime threads there actually are
// Retunrns a negative number upon failure
int32_t do_rt_threads_info(struct realtime_thread_info __user *out, size_t len) {
	struct task_struct *task;
	struct realtime_thread_info rt_pinfo;
	size_t count = 0;

	if (out == NULL) return -EACCES;
	if (len == 0) return -EINVAL;

	rcu_read_lock();
	for_each_process(task) {

		if(task->rt_priority > 0) {
			if (count >= len) {
				count++;
				continue;
			}

			rt_pinfo.tid = task->tgid;
			rt_pinfo.pid = task->pid;
			rt_pinfo.rt_priority = task->rt_priority;
			get_task_comm(rt_pinfo.name, task);

			if (copy_to_user(&out[count], &rt_pinfo, sizeof(rt_pinfo))) {
				rcu_read_unlock();
				return -EFAULT;
			}

			count++;
		}
	}
	rcu_read_unlock();

	return count;
}

SYSCALL_DEFINE2(list_rt_threads, struct realtime_thread_info __user*, out, size_t, len) {
	return do_rt_threads_info(out, len);
}

size_t do_count_rt_threads(void) {
	struct task_struct *task;
	size_t rt_thread_count = 0;

	rcu_read_lock();
	for_each_process(task) {
		task_lock(task);
		get_task_struct(task);
		if(task->rt_priority > 0) {
			rt_thread_count++;
		}
		put_task_struct(task);
		task_unlock(task);
	}
	rcu_read_unlock();

	return rt_thread_count;
}

SYSCALL_DEFINE0(count_rt_threads) {
	return do_count_rt_threads();
}

