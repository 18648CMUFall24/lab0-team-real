#include <linux/rtes_framework.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/time.h>

#define MAX_PROCESSORS 4

const u16 UTIL_TABLE[] = {0, 1000, 828, 780, 757, 743, 735, 729, 724, 721, 718};
const u16 MAX_UTIL = 693;

enum fit_algo {FF, NF, BF, WF, PA, LST};

struct bucket_task_ll {
	struct bucket_task_ll *next;
	pid_t tid;
	u32 cost;
	u32 period;
	u16 util;
};

struct bucket_info {
	struct bucket_task_ll *first_task;
	u16 running_util;
	u32 num_tasks;
	bool processorOn;
};

static struct bucket_info buckets[MAX_PROCESSORS] = {};
static enum fit_algo algo = FF;

bool increase_bucket_count(void) {
	u8 i;
	// Handle initialization
	if (!buckets[0].processorOn) {
		buckets[0].processorOn = true;
		return true;
	}

	for(i = 1; i < MAX_PROCESSORS; i++) {
		if (!buckets[i].processorOn) {
			printk(KERN_INFO "Turning on processor %d", i);
			buckets[i].processorOn = true;
			return true;
		}
	}

	return false;
}

void turnOffUnusedProcessors(void) {
	u8 i;
	for(i = 1; i < MAX_PROCESSORS; i++) {
		if (buckets[i].num_tasks == 0 && buckets[i].processorOn) {
			printk(KERN_ERR "Processsor %d is on with no rt processes running...Turning off", i);
			buckets[i].processorOn = false;
		}
	}
}


bool check_util_PA(u32 util) {
	return util <= 1000;
}

bool utilization_bound_test(u32 util, u32 tasks) {
	// Greater than 100% util isn't allowed
	if (util > 1000) {return false;}

	// Due to rounding error sometimes 0% util will exist which is fine
	if (util == 0) { return true; }

	if (tasks == 0) {
		// No tasks == No Util
		return util == 0;
	} else if (tasks == 1) {
		return util <= 1000;
	} else {
		if (tasks > 10) {
			// Hedging tests won't test edge cases like 11 tasks and 70% util
			// Bounding more than 10 tasks to 0.693
			return util<= MAX_UTIL;
		} else {
			// Actual checking on precomputed bounds for less than 10 tasks
			return util <= UTIL_TABLE[tasks];
		}
	}
}

u32 rt_ceil_div(u32 rt, u32 period) {
	int out = rt / period;
	if (rt % period) out++;
	return out;
}

u32 rt_accumulate_init(struct bucket_info *bucket, struct bucket_task_ll *task) {
	struct bucket_task_ll *search;
	u32 a;

	search = bucket->first_task;
	a = 0;

	while (search && search != task && search->period <= task->period) {
		a += search->cost;
		search = search->next;
	}

	return a;
}

u32 rt_accumulate(struct bucket_info *bucket, struct bucket_task_ll *task, u32 old_a) {
	struct bucket_task_ll *search;
	u32 a; 

	search = bucket->first_task;
	a = task->cost;

	while (search && search != task && search->period <= task->period) {
		a += rt_ceil_div(old_a, search->period) * search->cost;
		search = search->next;
	}

	return a;
}

bool response_time_test(struct bucket_info *bucket, struct bucket_task_ll *task) {
	// Assume response_time_test passes for all tasks in bucket
	u32 new, old;
	struct bucket_task_ll *search;
	
	// Phase 1 - Check it's possible to admit the new task
	new = rt_accumulate_init(bucket, task);
	do {
		if (new > task->period) { return false; }
		old = new;
		new = rt_accumulate(bucket, task, old);
	} while (new != old);


	// Phase 2 - Check all old tasks still are schedulable with new task
	search = bucket->first_task;
	while (search) {
		new = rt_accumulate_init(bucket, search);
		do {
			if (new > search->period) { return false; }
			old = new;
			new = rt_accumulate(bucket, task, old);
			if (search->period < task->period) {
				new += rt_ceil_div(old, task->period) * task->cost;
			}
		} while (new != old);
		
		search = search->next;
	}

	return true;
}

bool check_util(struct bucket_info *bucket, struct bucket_task_ll *task) {
	u32 new_num_tasks; 
	u16 new_util; 
	bool valid; 

	new_util = bucket->running_util + task->util;
	new_num_tasks = bucket->num_tasks + 1;
	valid = false;

	if (new_util > 1000) return false;

	if (utilization_bound_test(new_util, new_num_tasks)) {
		return true;
	} else if (response_time_test(bucket, task)){
		printk(KERN_INFO "Resorting to response time test to schedule task %d", task->tid);
		return true;
	} else {
		return false;
	}
}

s8 find_FF(struct bucket_task_ll *task) {
	s8 i = 0;
	for(i = 0; i < MAX_PROCESSORS; i++) {
		if (buckets[i].processorOn && check_util(&buckets[i], task)) {
			return i;
		} 
	}
	return -1;
}

s8 find_NF(struct bucket_task_ll *task) {
	static u32 next_bucket = 0;
	s8 i;
	for(i = 0; i < MAX_PROCESSORS; i++) {
		if (buckets[next_bucket].processorOn && check_util(&buckets[next_bucket], task)) {
			return  next_bucket;
		} else {
			next_bucket = (next_bucket + 1) % MAX_PROCESSORS;
		}
	}

	return -1;
}

s8 find_BF(struct bucket_task_ll *task) {
	s16 best_utilization = -1;
	s8 best_utilized_bucket = -1;
	s8 i;

	for(i = 0; i < MAX_PROCESSORS; i++) {
		if ((s16) buckets[i].running_util > best_utilization) {
			if (buckets[i].processorOn && check_util(&buckets[i], task)) {
				best_utilized_bucket = i;
				best_utilization = buckets[i].running_util;
			} 
		}
	}

	return best_utilized_bucket;
}

s8 find_WF(struct bucket_task_ll *task) {
	s8 least_utilized_bucket = -1;
	s16 least_utilization = 1001;
	s8 i;

	for(i = 0; i < MAX_PROCESSORS; i++) {
		if (buckets[i].processorOn && buckets[i].running_util < least_utilization) {
			least_utilized_bucket = i;
			least_utilization = buckets[i].running_util;
		}
	}

	if (check_util(&buckets[least_utilized_bucket], task)) {
		return least_utilized_bucket;
	} else {
		return -1;
	}
}

s8 find_PA(struct bucket_task_ll *task) {
	struct bucket_task_ll *tmp;
	u16 computed_util;
	s8 i;
	bool valid;

	for(i = 0; i < MAX_PROCESSORS; i++) {
		if (!buckets[i].processorOn) continue;
		tmp = buckets[i].first_task;
		valid = true;
		while(tmp) {
			if (tmp->period > task->period) {
				valid = (tmp->period % task->period == 0);
			} else {
				valid = (task->period % tmp->period == 0);
			}

			if (!valid) break;
			tmp = tmp->next;
		}

		computed_util = task->util + buckets[i].running_util;
		if (valid && check_util_PA(computed_util)) {
			return i;
		}
	}

	return -1;
}

s8 find_LST(struct bucket_task_ll *task) {
	increase_bucket_count();
	return find_WF(task);
}

// Finds a bucket 
// Upon failure returns a negative number
s8 find_bucket(struct bucket_task_ll *task) {
	switch (algo) {
		case FF: return find_FF(task);
		case NF: return find_NF(task);
		case BF: return find_BF(task);
		case WF: return find_WF(task);
		case PA: return find_PA(task);
		case LST: return find_LST(task);
		default:
			printk(KERN_ERR "Somehow trying to use invalid insertion algorithm");
			return -1;
	}
}

// Must already be validated
void add_task_to_bucket(struct bucket_info *bucket, struct bucket_task_ll *task) {
	if (bucket == NULL) return;

	if (bucket->first_task == NULL || task->period < bucket->first_task->period) {
		task->next = bucket->first_task;
		bucket->first_task = task;
	} else {
		struct bucket_task_ll *prev = bucket->first_task;
		struct bucket_task_ll *search = bucket->first_task;
		while (search && search->period <= task->period) {
			prev = search;
			search = search->next;
		}
		prev->next = task;
		task->next = search;
	}

	// TODO: Pin tid to processor
	
	bucket->num_tasks++;
	bucket->running_util += task->util;
	turnOffUnusedProcessors();
}

// Task must be in bucket
void remove_task_from_bucket(pid_t tid, u8 bucket_no) {
	struct bucket_info *bucket;
	struct bucket_task_ll *prev, *search;

	if (bucket_no >= MAX_PROCESSORS) return;

	bucket = &buckets[bucket_no];

	prev = bucket->first_task;
	search = bucket->first_task;

	if (prev->tid == tid) {
		bucket->first_task = buckets->first_task->next;
	}

	while (search && search->tid != tid) {
		prev = search;
		search = search->next;
	}

	if (search && (search->tid == tid)) {
		prev->next = search->next;
		bucket->running_util -= search->util;
		bucket->num_tasks--;
	} else {
		printk(KERN_ERR "Failed to remove task from processor, are you sure it's in the right bucket?");
	}

	turnOffUnusedProcessors();
}

// Returns processor task was assigned into
// Upon failure returns a negative number
s8 insert_task(struct threadNode *task) {
	struct bucket_task_ll *newTask;
	s8 bucket;

	newTask = (struct bucket_task_ll *) kmalloc(sizeof(struct bucket_task_ll), GFP_KERNEL); 
	newTask->tid = task->tid;
	newTask->cost = timespec_to_ns(&task->C);
	newTask->period = timespec_to_ns(&task->T);

	newTask->util = (u16) ((newTask->cost * 1000) / newTask->period);
	bucket = find_bucket(newTask);
	if (bucket < 0) {
		if (!increase_bucket_count()) {
			return -1;
		}
		bucket = find_bucket(newTask);
	}

	if (bucket >= 0 && bucket < MAX_PROCESSORS) {
		add_task_to_bucket(&buckets[bucket], newTask);
	} else {
		printk(KERN_ERR "Failed to insert task %d", newTask->tid);
	}

	// Syscall to assign task to bucket here
	return bucket;
}

void print_buckets() {
	struct bucket_task_ll *tmp;
	s8 i;
	for(i = 0; i < MAX_PROCESSORS; i++) {
		printk(KERN_INFO"[%d] %d Tasks: %d total util\n", i, buckets[i].num_tasks, buckets[i].running_util);
		tmp = buckets[i].first_task;
		while (tmp) {
			printk(KERN_INFO "\t[%d]: (%d, %d) util .%d\n", tmp->tid, tmp->cost, tmp->period, tmp->util);
			tmp = tmp->next;
		}
	}
}

/*
int main() {
	struct bucket_task_ll A = {.tid=0, .cost=800, .period=1000, .util=0, .next=NULL};
	struct bucket_task_ll B = {.tid=1, .cost=500, .period=1000, .util=0, .next=NULL};
	struct bucket_task_ll C = {.tid=2, .cost=700, .period=1000, .util=0, .next=NULL};
	struct bucket_task_ll D = {.tid=3, .cost=600, .period=1000, .util=0, .next=NULL};
	struct bucket_task_ll E = {.tid=4, .cost=200, .period=1000, .util=0, .next=NULL};
	struct bucket_task_ll F = {.tid=5, .cost=400, .period=1000, .util=0, .next=NULL};
	struct bucket_task_ll G = {.tid=6, .cost=100, .period=1000, .util=0, .next=NULL};

	algo = FF;

	// print_buckets();
	insert_task(&A);
	// print_buckets();
	insert_task(&B);
	// print_buckets();
	insert_task(&C);
	// print_buckets();
	insert_task(&D);
	// print_buckets();
	insert_task(&E);
	// print_buckets();
	insert_task(&F);
	// print_buckets();
	insert_task(&G);
	print_buckets();

	return 0;
}
*/
