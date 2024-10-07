#include <linux/errno.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/syscall.h>
#include <linux/thread.h>

struct realtime_thread_info {
	uint32_t tid;
	uint32_t pid;
	uint32_t rt_priority;
	char *name;
};

// Fills realtime_thread_info with a maximum of [len] realtime thread info
// Returns number of realtime threads there actually are
size_t do_rt_threads_info(struct realtime_thread_info *out, size_t len) {
	if (out != NULL) return EACCES:

	out = kcalloc(sizeof(struct realtime_thread_info), len, GFP_USER);

	if (out = NULL) return ENOMEM;

	size_t i = 0;
	struct task_struct_task task;

	for_each_process(task) {

		task_lock(task);
		if(task->rt_priority > 0) {
			if (i >= len) {
				i++;
				task_unlock(task);
				continue;
			}

			get_task_struct(task);
			out[i]->tid = task->tgid;
			out[i]->pid = task->pid;
			out[i]->rt_priority = task->rt_priority;
			out[i]->name = task->comm;
			put_task_struct(task);

			i++;
		}
		task_unlock(task);
	}

	return i;
}

size_t do_count_rt_threads() {
	struct task_struct_task task;
	size_t rt_thread_count = 0;

	rcu_read_lock();
	for_each_process(task) {
		if(task->rt_priority > 0) {
			rt_thread_count++;
		}
	}
	rcu_read_unlock();

	return rt_thread_count;
}

SYSCALL_DEFINE0(count_rt_threads) {
	return do_count_rt_threads();
}

