1. *How does a system call execute? Explain the steps in detail from making the call in the
userspace process to returning from the call with a result.*

> A system call executes by generating a software interrupt that corresponds to a number in a syscall table. That table stores a pointer to a function which executes with the args passed prior to the syscall being triggered.

2. *Define re-entrancy and thread-safety.*
> Re-entracy is the ability for code to be executing, get interrupted, and then continue executing without any incorrect behavior or data corruption.
> Thread-safety is the ability for multiple threads to access data concurrently without race conditions or other unexpected behaviors.   

3. *What does it mean for a kernel to be preemptive? Is the Linux kernel you are hacking on
preemptive?*
> A kernel being preemptive means that it can interrupt a running process to switch tasks. The Linux kernel is preemptive.

4. *When does access to data structures in userspace need to be synchronized?*
> Data structues in userspace need to be synchronized when one task has touched a region of memory which is actively is needed by another task.

5. *What synchronization mechanism can be used to access shared kernel data structures safely
on both single- and multi-processor platforms?*
> An Read-Copy-Update (RCU) can be used ot share kernel data structures safely on both platforms.

6. *What is the container of macro used for in the kernel code? In rough terms, how is it
implemented?*
> The `container_of` macro obtains the address of the parent struct from one of the struct's members. It does so by doing pointer math by subtracting the byte offset of some member in a struct from the pointer to that member which gives the pointer of the struct.
