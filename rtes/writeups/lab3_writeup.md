# Lab 2 Writeup

1. (1 point) Your hardware device, like any other, has a very limited number of hardware timers. Does
that limit extend to the number of hrtimers that can be created? Why or why not?

It does not limit the number of hrtimers due to the hrtimers being software created and managed by the kernel. The kernel multiplex hrtimers onto the available hadrware timers creating multiple hrtimers to use.

2. (1 point) Assume a thread makes a blocking file read I/O call and the OS does not have the data
handy in memory. The OS blocks and deschedules the thread until the data arrives from disk. When
the data does arrive, how does the OS know which thread to wake up? Which kernel mechanism is
used?

It puts the thread in a wait queue associated with the data that is needed. Once the data has arrived, it triggers a wake-up and notifies the approrpriate thread that the thread is waiting upon. The mechanism is called a wake up. 

3. (1 point) Periodic work in the kernel can be performed by adding it to a work queue or to an hrtimer
callback. What is a work queue handler allowed to do that an hrtimer handler is not?

A work queue handler can peform blocking operations or sleep since it runs in a process context switch call, while hrtimers are called interupted based so sleeping and blocking is not allowed or limited due to not wanting to wait in an interrupt which can cause deadlock or the software to be stuck in the interrupt cause it stops the program to execute its code

4. (1 point) What is the difference between regular signals and real-time signals?

Regular signals have limited priority and can combine identical signals into one, while real time signals have higher priority and a guaranteed order to be send and can carry different data in the POSIX versions. 

5. (1 point) What kernel subsystem decides which sleep state the processor enters and what parameters
does it use to make the decision?

The Power Management system decides which sleep state the processor enters. Its parameters it uses is the system load, latency, and power conumpsion to make its decisions.

