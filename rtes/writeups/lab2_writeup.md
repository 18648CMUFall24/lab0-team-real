# Lab 2 Writeup

1. Concurrent execution means that two or more threads are being interweaved to be executed on the same processor, this involves some form of context switching happening between programs. Parallel execution means two or more threads are executing at the same time by different processors, thus memory has to be explicitly syncronized in order for data to be shared between threads.

2. Dexter's Problems
- (A) (1s) / (100.01 us) = 9,999 max syscalls per period. (1000 samples / 1 sample/syscall) = 1000 syscalls < 9999 syscalls. 0% of samples are lost due to overhead delay.
- (B) (100,000 samples / 1 sample/syscall) = 100,000 syscalls > 9999 syscalls. (100,000 - 9999) / 100,000) ~= 90% of syscalls are lost due to overhead delay.
- (C) (1s) / (100 us + 1000 * 1ns) = 9,901 max syscalls per second (1,000 samples per syscall). (100,000 samples) / (1000 samples/syscall) = 100 syscalls < 9901 syscalls. 0% of samples are lost due to overhead delay.
- (D) Dexter can increase his buffer size to amortize the cost of making the round trip between user & kernel space. Another way to implement this is to have a shared memory region between kernel and user space, allowing the need for multiple repeated syscalls to be eliminated and can access the data points directly on the shared memory region.

