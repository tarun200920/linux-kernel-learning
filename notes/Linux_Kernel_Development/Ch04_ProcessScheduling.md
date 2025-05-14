# Chapter 4: Process Scheduling

## Summary
- Linux uses a **Completely Fair Scheduler (CFS)**.
- Schedulers select tasks from the runqueue (`rq`) based on vruntime.
- Real-time policies: `SCHED_FIFO`, `SCHED_RR`, `SCHED_DEADLINE`.
- Preemption enables switching from one task to another mid-execution.
- CFS maintains fairness using red-black trees.

## Quick Recall
- What is vruntime in CFS?
- Difference between SCHED_NORMAL and SCHED_FIFO?
- How does Linux handle task preemption?
- How does real-time scheduling work?

## Hands-On Ideas
- Use `chrt` on BBB to change a process's scheduler:
  ```bash
  chrt -f -p 99 <pid>
  ```
- Analyze scheduling class via:
  ```bash
  ps -eLo pid,cls,cmd | less
  ```
- Write a user-space app with different priorities, observe behavior on BBB.
