
# Chapter 5: [System Calls]

## Summary
- System Call is an interface between the user-space applications and the kernel. It provides a layer between the hardware and the user-space processes.
- The System Call layer severs primarily three purposes:
	1. It provides an abstracted hardware to the user-space applications. It means that the user-space applications don't bother what type of disk, media or filesystem it resides in.
	2. It provides security and stability. It means that the user-space applications cannot access the hardware incorrectly, or, steal the resources of some other process.
	3. It gives a sense of virtualization of the system to the processes.
- Other interfaces such as device files or /proc/ are also accessed via system calls by the user-space applications.
- Typically, the applications don't use the system calls directly.
  For example: printf() is not a system call but it uses an API implemented in user-space which uses the system call i.e. write().
  ![[Pasted image 20250516071611.png]]
- Most of the common application programming interfaces (APIs) in the Unix are based on the POSIX standard.
  **POSIX:** POSIX is a combination of a series of standards provided by IEEE that aim to provide a portable operating system standard.
- The system call interface is provided in part by the C library.
- System calls have a defined behavior.
  For example: The system call `getpid()` is defined to return an integer value which is the current process's PID. The implementation of this syscall in kernel is simple:
    ```c
	SYSCALL_DEFINE0(getpid)
	{
		return task_tgid_vnr(current); // returns current->tgid
	}
  ```
- SYSCALL_DEFINE0 is simple a macro that defines a system call with 0 arguments (That's why DEFINE0). The expanded code looks like this:
  ```c
	  asmlinkage long sys_getpid(void)
 ```  
 - Naming convention: `getpid()` system call is defined as `sys_getpid()`
 - Each system call is assigned a unique *syscall number*.
 - `sys_ni_syscall()`: "Not Implemented" system call.
   This does nothing except return `-ENOSYS`  which refers to invalid system call.
 - The kernel keeps a list of all registered system calls in a table sys_call_table.
   For x86-64, the table is define in `arch/i386/kernel/syscall_64.c`
 - System call handler (`system_call()`) in kernel is an exception handler which gets executed when user-space application sends a signal (software interrupt/trap) to kernel to inform that it wants to execute a system call.
 - Before causing the trap into the kernel, the user-space writes the system call number in the `eax` register which is read by system call handler.
 ![[Pasted image 20250516202701.png]]

- To pass the arguments user-space uses the registers `ebx`, `ecx`, `edx`, `esi`, and `edi`.
- The return value of the system call is also return via a register `eax`.

### System Call Implementation

- System call should have exactly one purpose. Multiplexing system call (a system call that does wildly different things depending on a flag argument) is discouraged in Linux.
  `ioctl()` is an example of what *not* to do.
- While writing a system call we must ensure that it supports the need of portability and robustness, not just today but in the future.
- System call must verify the parameters passed to ensure they are valid and legal. If the parameters are invalid and not verified at system call level, then while handling in the kernel space it can cause security and stability issues.
    For example: File I/O system calls must check whether the file descriptor is valid and correct. Process related functions must check the provided PID is valid and correct.
- For reading or writing any memory, the system call must check if these memory is marked as readable and writable respectively. 
- For writing into user-space `copy_to_user()` method is provided.
- For reading from user-space `copy_from_user()` method is provided
- Let's consider a bad example of using the above two methods which is worthless:
```c
/*
* silly_copy - pointless syscall that copies the len bytes from
* ‘src’ to ‘dst’ using the kernel as an intermediary in the copy.
* Intended as an example of copying to and from the kernel.
*/
SYSCALL_DEFINE3(silly_copy,
unsigned long *, src,
unsigned long *, dst,
unsigned long len)
{
unsigned long buf;
/* copy src, which is in the user’s address space, into buf */
if (copy_from_user(&buf, src, len))
return -EFAULT;
/* copy buf into dst, which is in the user’s address space */
if (copy_to_user(dst, &buf, len))
return -EFAULT;
/* return amount of data copied */
return len;
}
```

- Both `copy_to_user()` and `copy_from_user()` may block. This may occur if the page containing the user data is not in physical memory but is swapped to the disk. In that case the process sleeps until the page fault handler can bring the page from the swap file on disk into physical memory.
- We must use the system provided `capable()` function to check if the caller holds the valid access.
  For example: `capable(CAP_SYS_NICE)` checks whether the caller has the ability to modify the nice values of other processes.
- By default, superuser possesses all capabilities and non-root possesses none.
  See `<linux/capability.h>` for a list of all capabilities and what rights they entail.

### System Call Context
- Kernel is in the process context while executing a system call.
- In process context, kernel is capable of sleeping (for example, if the system call blocks on a call or explicitly calls `schedule()`), and is fully preemptible.
- Care must be taken to ensure that the system calls are reentrant. This will be covered in Chapter 9, “*An Introduction to Kernel Synchronization,*” and Chapter 10, “*Kernel Synchronization Methods*”.
- When the system call returns, control continues in `system_call()`, which ultimately switches to user-space and continues the execution of the process.

### Register a System Call
1. Add an entry to the system call table. This needs to be done for each architecture that supports the system call. On adding the new system call (`sys_foo`), it would be given the next subsequent number.
   ```c
    .long sys_rt_tgsigqueueinfo     /* 335 */
	.long sys_perf_event_open
	.long sys_recvmmsg
	.long sys_foo
```
2. For each supported architecture, define the system call number in `<asm/unistd.h>`.
   ```c
	#define __NR_rt_tgsigqueueinfo    335
	#define __NR_perf_event_open      336
	#define __NR_recvmmsg             337
	#define __NR_foo                  338
```
3. Compile the system call into the kernel image not as a module.
   This can be done by putting the system call in a relevant file under `/kernel` such as `sys.c`, which is home to miscellaneous system calls.
   ```c
	#include <asm/page.h>
	/*
	* sys_foo – everyone’s favorite system call.
	*
	* Returns the size of the per-process kernel stack.
	*/
	asmlinkage long sys_foo(void)
	{
		return THREAD_SIZE;
	}
```

### Accessing a System Call from User-Space
- User application can pull in the function prototypes from the standard headers and link with C library to use your system call (or the library routine that, in turn, uses your syscall call). If you just wrote the system call, it is doubtful that glibc already supports it.
- Linux provides a set of macros for wrapping the access to system calls. It sets up the register content and issues the trap instructions.
  These macros are named as `_syscalln()` where n is between 0 and 6 and it corresponds to the number of parameters passed into the system call.
- For example, consider the system call `open()`, defined as:
  ```c
	long open(const char *filename, int flags, int mode)
```
	The syscall macro to use this system call without explicit library support would be:
	```c
	#define __NR_open 5
	_syscall3(long, open, const char *, filename, int, flags, int, mode)
```
	Then the application can simple call `open()`.
	- **Note:** Here `_syscalln` pushes the system call number along with the parameters into the correct registers and finally issues the software interrupt to trap into the kernel.
- Let's see how to use the new system call in a test application:
  ```c
	#define __NR_foo 283
	__syscall0(long, foo)
	
	int main ()
	{
		long stack_size;
		stack_size = foo ();
		printf (“The kernel stack size is %ld\n”, stack_size);
		return 0;
	}
```


### Pros, Cons, and Alternatives of System Call

#### Pros
- System call performance on Linux is fast.
- System calls are simple to implement and easy to use.
#### Cons
- After the system call is in a stable series kernel, it is written in stone. The interface cannot change without breaking user-space applications.
- Each architecture needs to separately register the system call and support it.
- System calls are not easily used from scripts and cannot be accessed directly from filesystem.
- For simple exchanges of information, a system call is overkill.
#### Alternatives
- Implement a device node (under `/dev`) and `read(`)/`write()` to it. Use `ioctl()` to manipulate specific settings or retrieve specific information.
- Certain interfaces, such as semaphores, can be represented as file descriptors and manipulated as such.
- Add the information as a file to the appropriate location in `sysfs`.

## Quick Recall
- Q1: What will happen if the user-space application are free to access the system resources without the kernel knowledge?
	- It would be nearly impossible to implement multitasking, and virtual memory, and certainly impossible to do so with stability and security.
- Q2: Where does the system call table resides?
	- In most of the architectures, the table is located in `entry.S`.
- Q3: How to write a system call?
	- [[Ch05_SystemCalls#Register a System Call]]]
- Q4: What are the alternatives of a system call?
	- [[Ch05_SystemCalls#Pros, Cons, and Alternatives of System Call]]

## Hands-On Ideas
1. Write a Simple Custom System Call (Kernel Module Alternative)
	- The BBB uses an ARM processor and a minimal system, so instead of modifying syscall tables (risky on embedded), you can:
		- **Write a kernel module** that mimics a syscall (e.g., takes two integers and returns their sum).
		- Use `copy_from_user()` and `copy_to_user()` properly to simulate user-to-kernel interaction.
		- Write a small user-space app that `ioctl()`s or reads from `/proc`/`/sys` to interact with your module.
	- *Goal: Understand how arguments/data move between user and kernel space.*
2. Trace System Calls using `strace` on the BBB
	- Install and run `strace` on simple commands (e.g., `ls`, `cat`, or a custom C program).
	- Note how system calls like `open`, `read`, `write`, `execve` show up.
	- Try intercepting failures and what system calls lead up to them.
	- *Goal: See real syscall behavior, inputs, and return values in context.*
3. Create a User-Space Program That Directly Uses System Calls
	- Use syscall wrappers via `unistd.h` (like `syscall(SYS_getpid)`).
	- Also manually invoke `syscall()` with proper numbers.
	- Observe what happens when you pass wrong arguments or invalid pointers.
	- *Goal: Understand syscall ABI and error handling.*
4. Compare Assembly for Libc-Wrapped vs Raw System Calls
	- Use `objdump` or `gcc -S` to see how a libc call like `write()` compares to direct `syscall()`.
	- Bonus: Use GDB on the BBB to step into the syscall.
	- *Goal: Learn how system calls bridge user space and kernel via `svc` (on ARM).*
5. Monitor Syscalls from a Custom User App Using eBPF (Advanced, Optional)
	- Try tracing your own app's system calls using `bpftrace` (if supported on your BBB) or `perf`.
	- *Goal: Connect chapter content to modern tracing tools.*
