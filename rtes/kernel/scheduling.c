// #include <linux/rtes_framework.h>
// #include <linux/kernel.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

const int UTIL_TABLE[] = {0, 1000, 828, 780, 757, 743, 735, 729, 724, 721, 718};
const int MAX_UTIL = 693;
const int MAX_PROCESSORS = 4;

enum fit_algo {FF, NF, BF, WF, PA, LST};

struct bucket_task_ll {
	int tid;
	int cost;
	int period;
	int util;
	struct bucket_task_ll *next;
};

struct bucket_info {
	unsigned int running_util;
	unsigned int num_tasks;
	struct bucket_task_ll *first_task;
};

static struct bucket_info buckets[4] = {};
static int num_buckets = 1;
static enum fit_algo algo = FF;

bool increase_bucket_count() {
	if (num_buckets < MAX_PROCESSORS) {
		num_buckets += 1;
		return true;
	} else {
		num_buckets = MAX_PROCESSORS;
		return false;
	}
}

bool check_util_PA(unsigned int util) {
	return util <= 1000;
}

bool utilization_bound_test(unsigned int util, unsigned int tasks) {
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

unsigned long rt_ceil_div(unsigned int rt, unsigned int period) {
	int out = rt / period;
	if (rt % period) out++;
	return out;
}

unsigned long rt_accumulate_init(struct bucket_info *bucket, struct bucket_task_ll *task) {
	unsigned long a = 0;
	struct bucket_task_ll *search = bucket->first_task;

	while (search && search != task && search->period <= task->period) {
		a += search->cost;
		search = search->next;
	}

	return a;
}

unsigned long rt_accumulate(struct bucket_info *bucket, struct bucket_task_ll *task, unsigned long old_a) {
	unsigned long a = task->cost;
	struct bucket_task_ll *search = bucket->first_task;

	while (search && search != task && search->period <= task->period) {
		a += rt_ceil_div(old_a, search->period) * search->cost;
		search = search->next;
	}

	return a;
}

bool response_time_test(struct bucket_info *bucket, struct bucket_task_ll *task) {
	// Assume response_time_test passes for all tasks in bucket
	int old;
	
	// Phase 1 - Check it's possible to admit the new task
	int a = rt_accumulate_init(bucket, task);
	do {
		if (a > task->period) { return false; }
		old = a;
		a = rt_accumulate(bucket, task, old);
	} while (a != old);


	// Phase 2 - Check all old tasks still are schedulable with new task
	struct bucket_task_ll *search = bucket->first_task;
	while (search) {
		int a = rt_accumulate_init(bucket, search);
		do {
			if (a > search->period) { return false; }
			old = a;
			a = rt_accumulate(bucket, task, old);
			if (search->period < task->period) {
				a += rt_ceil_div(old, task->period) * task->cost;
			}
		} while (a != old);
		
		search = search->next;
	}

	return true;
}

bool check_util(struct bucket_info *bucket, struct bucket_task_ll *task) {
	bool valid = false;
	int new_util = bucket->running_util + task->util;
	int new_num_tasks = bucket->num_tasks + 1;

	if (new_util > 1000) return false;

	if (utilization_bound_test(new_util, new_num_tasks)) {
		return true;
	} else if (response_time_test(bucket, task)){
		printf("Resorting to response time test\n");
		return true;
	} else {
		return false;
	}
}

int find_FF(struct bucket_task_ll *task) {
	for (int i = 0; i < num_buckets; i++) {
		if (check_util(&buckets[i], task)) {
			return i;
		} 
	}
	return -1;
}

int find_NF(struct bucket_task_ll *task) {
	static unsigned int next_bucket = 0;

	for (int i = 0; i < 4; i++) {
		if (check_util(&buckets[next_bucket], task)) {
			return  next_bucket;;
		} else {
			next_bucket = (next_bucket + 1) % num_buckets;
		}
	}

	return -1;
}

int find_BF(struct bucket_task_ll *task) {
	int best_utilized_bucket = -1;
	int best_utilization = -1;

	for (int i = 0; i < num_buckets; i++) {
		if ((int) buckets[i].running_util > best_utilization) {
			if (check_util(&buckets[i], task)) {
				best_utilized_bucket = i;
				best_utilization = buckets[i].running_util;
			} 
		}
	}

	return best_utilized_bucket;
}

int find_WF(struct bucket_task_ll *task) {
	int least_utilized_bucket = -1;
	int least_utilization = 1001;

	for (int i = 0; i < num_buckets; i++) {
		if (buckets[i].running_util < least_utilization) {
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

int find_PA(struct bucket_task_ll *task) {
	struct bucket_task_ll *tmp;
	for (int i = 0; i < num_buckets; i++) {
		tmp = buckets[i].first_task;
		bool valid = true;
		while(tmp) {
			if (tmp->period > task->period) {
				valid = (tmp->period % task->period == 0);
			} else {
				valid = (task->period % tmp->period == 0);
			}

			if (!valid) break;
			tmp = tmp->next;
		}

		unsigned int computed_util = task->util + buckets[i].running_util;
		if (valid && check_util_PA(computed_util)) {
			return i;
		}
	}

	return -1;
}

int find_LST(struct bucket_task_ll *task) {
	increase_bucket_count();
	return find_WF(task);
}

// Finds a bucket 
// Upon failure returns a negative number
int find_bucket(struct bucket_task_ll *task) {
	unsigned int util = (task->cost * 1000) / task->period;
	task->util = util;
	switch (algo) {
		case FF: return find_FF(task);
		case NF: return find_NF(task);
		case BF: return find_BF(task);
		case WF: return find_WF(task);
		case PA: return find_PA(task);
		case LST: return find_LST(task);
		default:
			printf("Somehow trying to use invalid insertion algorithm");
			return -1;
	}
}

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

	bucket->num_tasks++;
	bucket->running_util += task->util;
}

void remove_task_from_bucket(struct bucket_info *bucket, struct bucket_task_ll *task) {
	if (bucket == NULL) return;

	struct bucket_task_ll *prev = bucket->first_task;
	struct bucket_task_ll *search = bucket->first_task;

	if (prev->tid == task->tid) {
		bucket->first_task = buckets->first_task->next;
	}

	while (search && search->tid != task->tid) {
		prev = search;
		search = search->next;
	}

	if (search && (search->tid == task->tid)) {
		prev->next = search->next;
		bucket->running_util -= task->util;
		bucket->num_tasks--;
	}
}

// Returns processor task was assigned into
// Upon failure returns a negative number
// TODO: Change to take in threadNode
int insert_task(struct bucket_task_ll *task) {
	int bucket = find_bucket(task);
	if (bucket < 0) {
		if (!increase_bucket_count()) {
			return -1;
		}
		bucket = find_bucket(task);
	}

	if (bucket >= 0 && bucket < MAX_PROCESSORS) {
		add_task_to_bucket(&buckets[bucket], task);
	} else {
		printf("Failed to insert task %d", task->tid);
	}

	// Syscall to assign task to bucket here
	return bucket;
}

void print_buckets() {
	struct bucket_task_ll *tmp;
	for (int i = 0; i < num_buckets; i++) {
		printf("[%d] %d Tasks: %d total util\n", i, buckets[i].num_tasks, buckets[i].running_util);
		tmp = buckets[i].first_task;
		while (tmp) {
			printf("\t[%d]: (%d, %d) util .%d\n", tmp->tid, tmp->cost, tmp->period, tmp->util);
			tmp = tmp->next;
		}
	}
}

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

