#ifndef _RTES_FRAMEWORK_H
#define _RTES_FRAMEWORK_H

#include <linux/time.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>

// Define the structure for linked list nodes
struct threadNode {
    struct timespec C;        // Computation time
    struct timespec T;        // Period time
    pid_t tid;                // Thread ID
    int cpuid;                // CPU ID
    struct threadNode* next;  // Pointer to the next node
};

// Declare the head of the linked list
extern struct threadNode* threadHead;

#endif /* _RTES_FRAMEWORK_H */
