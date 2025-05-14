# Chapter 1: Introduction to the Linux Kernel

## Summary
- Linux is a **monolithic**, preemptive, multiuser, multitasking OS kernel.
- It manages hardware via device drivers, schedules tasks, manages memory and filesystems, and provides networking.
- Kernel runs in privileged mode (ring 0), userspace in ring 3.
- Boot flow starts from `start_kernel()` → `rest_init()`.
- Kernel handles multiple types of processes: kernel threads and user-space processes.

## Quick Recall
- What is the difference between a monolithic kernel and a microkernel?
- What are the responsibilities of the kernel?
- What does the function `start_kernel()` do?
- How does Linux initialize multitasking after boot?

## Hands-On Ideas
- On BeagleBone Black (BBB), check the running kernel version:
  ```bash
  uname -a
  ```
- Examine `/proc` for system data:
  ```bash
  cat /proc/cpuinfo
  cat /proc/meminfo
  ```
- Check boot arguments from U-Boot or kernel using:
  ```bash
  cat /proc/cmdline
  ```
