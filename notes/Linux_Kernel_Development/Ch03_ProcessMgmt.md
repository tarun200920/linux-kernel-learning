# Chapter 3: Process Management

## Summary
- All tasks are represented via `task_struct`.
- Processes may be user-space or kernel threads.
- Kernel threads are created via `kthread_create()`, started with `kthread_run()`.
- Process states:
  - `TASK_RUNNING`
  - `TASK_INTERRUPTIBLE`
  - `TASK_UNINTERRUPTIBLE`
- Scheduler picks processes based on priority, time slice, and policies (CFS, FIFO, RR).

## Quick Recall
- What is stored in `task_struct`?
- How do kernel threads differ from user-space ones?
- What is the difference between interruptible and uninterruptible sleep?
- What scheduler policies does Linux use?

## Hands-On Ideas
- Explore `/proc/[pid]` entries on BBB:
  ```bash
  cat /proc/1/status
  ps aux | grep kthreadd
  ```
- Identify kernel threads:
  ```bash
  ps -eLo pid,cls,pri,rtprio,ni,cmd | grep "\[.*\]"
  ```
- Write a simple kernel thread example module on BBB.
