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
	// struct calc_data running_util;
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

// bool check_util_PA(struct calc_data util) {
bool check_util_PA(unsigned int util) {
	/*
	if (util.whole > 1 || (util.whole == 1 && util.decimal > 0)) { 
		return false; 
	} else {
		return true
	}
	*/
	return util <= 1000;
}

// bool check_util(struct calc_data util, u64 tasks) {
bool check_util(unsigned int util, unsigned int tasks) {
	return check_util_PA(util);

	/*
	// Greater than 100% util isn't allowed
	// if (util.whole > 1 || (util.whole == 1 && util.decimal > 0)) { return false; }
	if (util > 1000) {return false;}

	// Due to rounding error sometimes 0% util will exist which is fine
	// if (util.whole == 0 && util.decimal == 0) { return true; }
	if (util == 0) {return false;}

	if (tasks == 0) {
		// No tasks == No Util
		// return ((util.whole == 0) && (util.decimal == 0));
		return util == 0;
	} else if (tasks == 1) {
		// Works because other cases are caught above
		// return (util.whole == 1) ^ (util.decimal != 0);
		return util <= 1000;
	} else {
		// if (util.whole > 0) { return false; }
		if (util > 1000) { return false; }

		if (tasks > 10) {
			// Hedging tests won't test edge cases like 11 tasks and 70% util
			// Bounding more than 10 tasks to 0.693
			// return util.decimal <= MAX_UTIL;
			return util<= MAX_UTIL;
		} else {
			// Actual checking on precomputed bounds for less than 10 tasks
			// return util.decimal <= UTIL_TABLE[tasks];
			return util <= UTIL_TABLE[tasks];
		}
	}
	*/
}



int find_FF(unsigned int util) {
	for (int i = 0; i < num_buckets; i++) {
		unsigned int computed_util = util + buckets[i].running_util;
		unsigned int new_task_count = 1 + buckets[i].num_tasks;
		if (check_util(computed_util, new_task_count)) {
			return i;
		} 
	}
	return -1;
}

int find_NF(unsigned int util) {
	static unsigned int next_bucket = 0;

	for (int i = 0; i < 4; i++) {
		unsigned int computed_util = util + buckets[next_bucket].running_util;
		unsigned int new_task_count = 1 + buckets[next_bucket].num_tasks;
		if (check_util(computed_util, new_task_count)) {
			return  next_bucket;;
		} else {
			next_bucket = (next_bucket + 1) % num_buckets;
		}
	}

	return -1;
}

int find_BF(unsigned int util) {
	int best_utilized_bucket = -1;
	int best_utilization = -1;

	for (int i = 0; i < num_buckets; i++) {
		if ((int) buckets[i].running_util > best_utilization) {
			unsigned int computed_util = util + buckets[i].running_util;
			unsigned int new_task_count = 1 + buckets[i].num_tasks;
			if (check_util(computed_util, new_task_count)) {
				best_utilized_bucket = i;
				best_utilization = buckets[i].running_util;
			} 
		}
	}

	return best_utilized_bucket;
}

int find_WF(unsigned int util) {
	int least_utilized_bucket = -1;
	int least_utilization = 1001;

	for (int i = 0; i < num_buckets; i++) {
		if (buckets[i].running_util < least_utilization) {
			least_utilized_bucket = i;
			least_utilization = buckets[i].running_util;
		}
	}

	unsigned int computed_util = util + buckets[least_utilized_bucket].running_util;
	unsigned int new_task_count = 1 + buckets[least_utilized_bucket].num_tasks;

	if (check_util(computed_util, new_task_count)) {
		return least_utilized_bucket;
	} else {
		return -1;
	}
}

int find_PA(unsigned int util, unsigned int period) {
	struct bucket_task_ll *tmp;
	for (int i = 0; i < num_buckets; i++) {
		tmp = buckets[i].first_task;
		bool valid = true;
		while(tmp) {
			if (tmp->period > period) {
				valid = (tmp->period % period == 0);
			} else {
				valid = (period % tmp->period == 0);
			}

			if (!valid) break;
			tmp = tmp->next;
		}

		unsigned int computed_util = util + buckets[i].running_util;
		if (valid && check_util_PA(computed_util)) {
			return i;
		}
	}

	return -1;
}

int find_LST(unsigned int util) {
	increase_bucket_count();
	return find_WF(util);
}

// Finds a bucket 
// Upon failure returns a negative number
int find_bucket(struct bucket_task_ll *task) {
	unsigned int util = (task->cost * 1000) / task->period;
	task->util = util;
	switch (algo) {
		case FF: return find_FF(util);
		case NF: return find_NF(util);
		case BF: return find_BF(util);
		case WF: return find_WF(util);
		case PA: return find_PA(util, task->period);
		case LST: return find_LST(util);
		default:
			printf("Somehow trying to use invalid insertion algorithm");
			return -1;
	}
}

void add_task_to_bucket(struct bucket_task_ll *task, unsigned int bucket) {
	task->next = buckets[bucket].first_task;
	buckets[bucket].first_task = task;
	buckets[bucket].num_tasks++;
	buckets[bucket].running_util += task->util;
}

// TODO: Change to take in threadNode
void remove_task_from_bucket(struct bucket_task_ll *task, unsigned int bucket) {
	struct bucket_task_ll *prev = buckets[bucket].first_task;
	struct bucket_task_ll *search = buckets[bucket].first_task;

	if (prev->tid == task->tid) {
		buckets[bucket].first_task = buckets[bucket].first_task->next;
	}

	while (search && search->tid != task->tid) {
		prev = search;
		search = search->next;
	}

	if (search && (search->tid == task->tid)) {
		prev->next = search->next;
		buckets[bucket].running_util -= task->util;
		buckets[bucket].num_tasks--;
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

	if (bucket >= 0) {
		add_task_to_bucket(task, bucket);
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
	struct bucket_task_ll G = {.tid=5, .cost=100, .period=1000, .util=0, .next=NULL};

	algo = WF;

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

