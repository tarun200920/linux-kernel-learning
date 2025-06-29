# Chapter 8: [Bottom Halves and Deferring Work]

## **Summary**

### **Bottom Halves**
- **Purpose**: Mechanisms to defer non-critical work from interrupt handlers to run later, improving system responsiveness.
- The job of bottom halves is to perform any interrupt-related work not performed by Interrupt Handler.
- Unfortunately, no hard and fast rules exist about what work to perform where - the decision is left entirely up to the device-driver author.
- Interrupt handlers run asynchronously, with at least the current interrupt line disabled. Minimizing their duration is important. Some useful tips to divide the work:
	- If the work is time sensitive, perform it in the interrupt handler.
	- If the work is related to the hardware, perform it in the interrupt handler.
	- If the work needs to ensure that another interrupt (particularly the same interrupt) does not interrupt it, perform it in the interrupt handler.
	- For everything else, consider performing the work in the bottom half.

- #### Why Bottom Halves?
	- **Need**: Interrupt handlers disable interrupts, slowing system response.
	- **Issue**: `IRQF_DISABLED` handlers disable all interrupts locally, plus current IRQ globally.
	- **Solution**: Defer heavy tasks to bottom halves to minimize disabled interrupt time.
	- **When**: Run "*later*" (not specific time), when system is less busy, interrupts enabled.
	- **Benefit**: Bottom halves run with all interrupts enabled, reducing latency.
	- **Top vs. Bottom**:
		- Top half: Quick, runs with interrupts disabled.
		- Bottom half: Runs later, with interrupts enabled.
	- **Example**: Defer network packet processing to avoid delaying keystrokes.

- #### A World of Bottom Halves
	- Unlike the top half, which is implemented entirely via the interrupt handler, multiple mechanisms are available for implementing a bottom half.
	- ##### **The Original Bottom Half:**
		- **Overview**: Early Linux used "BH" (Bottom Half) for deferring work.
		- **Mechanism**: Single, simple interface for bottom halves in kernel 2.0.
		- **Structure**: Statically defined 32 bottom halves, marked by bits in a 32-bit integer.
		- **Operation**: Top half sets bit to schedule BH; runs after interrupt.
		- **Synchronization**: Globally synchronized, no parallel execution, even on multi-CPU.
		- **Limitations**: Inflexible, bottleneck due to single-threaded design.
		- **Status**: Obsolete in 2.6, replaced by softIRQs, tasklets, workqueues.
	- ##### Task Queues
		- **Definition**: Pre-2.5 kernel method to queue deferred work in linked lists.
		- **Operation**: Queues hold functions to run at specific times (e.g., timer-based).
		- **Usage**: Drivers register bottom halves in appropriate queue.
		- **Limitations**: Inflexible, not lightweight enough for performance-critical tasks.
		- **Status**: Replaced by workqueues in 2.6; obsolete.
		- **Example**: Queued periodic tasks for legacy drivers.
	- ##### Softirqs and Tasklets
		- **Softirqs**
			- Static bottom halves, run concurrently on any processor, even same type.
			- High-performance, used in critical subsystems (e.g., networking).
			- Requires careful coding to handle concurrency.
		- **Tasklets**
			- Dynamic bottom halves, built on softirqs, simpler for drivers.
			- Different tasklets run concurrently, but same type cannot.
			- Easier to use, sufficient for most driver needs.
		- **Status**: Replaced BH interface in 2.3; key mechanisms in 2.6.
		- **Example**: Network uses softIRQs; drivers use tasklets for simplicity.
	- ##### Kernel Timers
		- **Definition**: Mechanism to defer work until a specific time elapses.
		- **Difference**: Unlike softIRQs/tasklets, timers schedule for future execution.
		- **Usage**: Used when precise delay is needed, not just "*later*."
		- **Details**: Covered in Chapter 11, “Timers and Time Management.”
	
	- Currently, three methods exist for deferring work: softirqs, tasklets, and work queues. Tasklets are built on softirqs and work queues are their own subsystem.

- #### **Softirqs**
	- Softirqs are rarely used directly; tasklets are much more common form of bottom half. The softirq code lives in the file `kernel/softirq.c` in the kernel source tree.
	- **Purpose**: High-performance bottom halves for critical tasks, run in interrupt context.
	- ##### Implementing Softirqs
		- Softirqs are statically allocated at compile time. Unlike tasklets, you cannot dynamically register and destroy softirqs.
		- Softirqs are represented by the `softirq_action` structure which is defined in `<linux/interrupt.h>`:
		```c
		struct softirq_action {
			void (*action)(struct softirq_action *);
		};
		```
		- A 32-entry array of this structure is declared in `kernel/softirq.c`:
		  ```c
		  static struct softirq_action softirq_vec[NR_SOFTIRQS];
		  ```
		- Each registered softirq consumes one entry in the array. Consequently, there are `NR_SOFTIRQS` registered softirqs.
		- The number of registered softirqs is statically determined at compile time and cannot be changed dynamically.The kernel enforces a limit of 32 registered softirqs; in the current kernel(2.6).
	- ##### The Softirq Handler
		- The prototype of a softirq handler, action, looks like
		  ```c
		  void softirq_handler(struct softirq_action *)
		  ```
		- The kernel passes the entire structure to the softirq handler. This trick enables future additions to the structure without requiring a change in every softirq handler.
		- A softirq never preempts another another softirq. The only event that can preempt a softirq is an interrupt handler. Another softirq, even the same one, can run on another processor however.
	- ##### Executing Softirqs
		- A registered softirq must be marked before it will execute. This is called *raising the softirq*.
		- Usually, the interrupt handler marks its softirq for execution before returning. Then at a suitable time, the softirq runs.
		- Pending softirqs are checked for and executed in the following places:
			- In the return from hardware interrupt code path
			- In the ksoftirqd kernel thread
			- In any code that explicitly checks for and executes pending softirqs, such as the networking subsystem.
		- Regardless of the method of invocation, softirq execution occurs in `_do_softirq()`, which is invoked by `do_softirq()`. The function is simple - if there are pending, `_do_softirq()` loops over each one, invoking its handler.
	- ##### Using Softirqs
		- Softirqs are reserved for the most timing-critical and important bottom-half processing on the system.
		- Currently, only two subsystems, networking and block devices, directly use softirqs. Additionally, kernel timers and tasklets are built on top of softirqs.
		- ###### Assigning an Index
			- Softirqs should be declared statically at compile time via an enum in `<linux/interrupt.h>`. The kernel uses this index, which starts at zero, as a relative priority. Softirqs with the lowest numerical priority execute before those with a higher numerical priority.
			- By convention, `HI_SOFTIRQ` is always the first and `RCU_SOFTIRQ` is always the last entry. A new entry likely belongs in between `BLOCK_SOFTIRQ` and `TASKLET_SOFTIRQ`.
			- ![[Pasted image 20250621182750.png]]
		- ###### Registering Your Handler
			- Softirq handler is registered at run-time via open_softirq(), which takes two parameters: the softirq's index and its handler function.
			- The networking subsystem, for example, registers its softirqs like this, in `net/core/dev.c`:
			  ```c
			  open_softirq(NET_TX_SOFTIRQ, net_tx_action);
			  open_softirq(NET_RX_SOFTIRQ, net_rx_action);
			  ```
			- The softirq handlers run with interrupts enabled and cannot sleep.
			- While a softirq handler runs, softirqs on the current processor are disabled. Another processor, however, can execute other softirqs.
			- If the same softirq is raised again while it is executing, another processor can run it simultaneously. This means that any shared data - even global data used only within the softirq handler - needs proper locking. This is an important point, and it is the reason tasklets are usually preferred.
		- ###### Raising your Softirq
			- After a handler is added to the `enum` list and registered via `open_softirq()`, it is ready to run.
			- To mark it pending, so it is run at the next invocation of `do_softirq()`, call `raise_softirq()`.
			- **Example**: Network interrupt raises `NET_RX_SOFTIRQ` for packet processing.
			  ```c
			  raise_softirq(NET_TX_SOFTIRQ);
			  ```
			- This function disables interrupts prior to actually raising the softirq and then restores them to their previous state.
			- If interrupts are already off, the function `raise_softirq_irqoff()` can be used as a small optimization.
			- Softirqs are most often raised from within interrupt handlers. In the case of interrupt handlers, the interrupt handler performs the basic hardware-related work, raises the softirq, and then exits.

- #### **Tasklets**
	- Tasklets are a bottom-half mechanism built on top of softirqs. They have nothing to do with tasks.
	- Tasklets are similar in nature and behavior to softirqs; however, they have a simpler interface and relaxed locking rules.
	- ##### Implementing Tasklets
		- As tasklets are implemented on top of softirqs, they *are* softirqs.
		- Tasklets are represented by two softirqs: `HI_SOFTIRQ` and `TASKLET_SOFTIRQ`. The only difference between the two is `HI_SOFTIRQ`-based tasklets run prior to the `TASKLET_SOFTIRQ`-based tasklets.
		- ###### The Tasklet Structure
			- Tasklets are represented by the `tasklet_struct` structure. Each structure represents a unique tasklet. The structure is defined in `<linux/interrupt.h>`.
			  ```c
			  struct tasklet_struct {
				  struct tasklet_struct *next; /* next tasklet in the list */
				  unsigned long state;         /* state of the tasklet */
				  atomic_t count;              /* reference counter */
				  void (*func)(unsigned long); /* tasklet handler function */
				  unsigned long data;      /* argument to the tasklet function */
			  };
			  ```
			- `func` - tasklet handler (equivalent of `action` to softirqs) and receives `data` as its sole argument.
			- `state`:
				- zero
				- `TASKLET_STATE_SCHED` - denotes a tasklet is scheduled to run.
				- `TASKLET_STATE_RUN` - denotes a tasklet that is running.
				  **Note:** *`TASKLET_STATE_RUN` is used only on multiprocessor machines. Because a uniprocessor machine always know whether a tasklet is running. (It is either the currently executing code, or not).*
			- `count` - reference count for the tasklet.
				- non-zero: the tasklet is disabled and cannot run.
				- zero: the tasklet is enabled and can run if marked pending.
		- ###### Scheduling Tasklets
			- Scheduled tasklets (the equivalent of raised softirqs) are stored in two pre-processor structures: 
				- `tasklet_vec` (for regular tasklets)
				- `tasklet_hi_vec` (for high-priority tasklets)
			- Both the structures are linked list of `tasklet_struct` structure. Each `tasklet_struct` structure in the list represents a different task.
			- **Functions**:
				- `tasklet_schedule()`: Schedules regular tasklet via `TASKLET_SOFTIRQ`.
				- `tasklet_hi_schedule()`: Schedules high-priority tasklet via `HI_SOFTIRQ`.
			- **Steps performed by `tasklet_schedule()`**:
			  1. Check if tasklet already scheduled (`TASKLET_STATE_SCHED`); return if set.
			  2. Call `__tasklet_schedule()`.
			  3. Save the state of the interrupt system, and then disable local interrupts. This ensures that nothing on this processor will mess with the tasklet code while `tasklet_schedule()` is manipulating the tasklets.
			  4. Add tasklet to be scheduled to head of `tasklet_vec` or `tasklet_hi_vec` linked list, which is unique to each processor in the system.
			  5. Raise `TASKLET_SOFTIRQ` or `HI_SOFTIRQ` softirq, so `do_softirq()` executes this tasklet in the near future.
			  6. Restore interrupts to their previous state and return.
			  
			- At the next earliest convenience, do_softirq() is run. Because `TASKLET_SOFTIRQ` or `HI_SOFTIRQ` is now raised, do_softirq() executes the associated handlers.
			- These handlers, `tasklet_action()` and `tasklet_hi_action()`, are the heart of tasklet processing. Let’s look at the steps these handlers perform:
			  1. **Disable Interrupts and Retrieve List**: Disable local interrupt delivery of the current processor. There is no need to save the interrupt state because `tasklet_action()` or `tasklet_hi_action()` is always called by `do_softirq()` which runs when interrupts are guaranteed to be enabled (typically after an interrupt handler returns, with interrupts re-enabled). Then the `tasklet_vec` or `tasklet_hi_vec` list for this processor is retrieved.
			  2. **Clear List**: Clear the list for this processor by setting it equal to NULL to prevent re-processing the same tasklets.
			  3. **Enable Interrupts**: Enable local interrupt delivery.
			  4. **Loop Over Tasklets**: Loop over each pending tasklet in the retrieved list.
			  5. **Check Running Status**: On multi-processor systems, checks  `TASKLET_STATE_RUN` flag; if set (tasklet running on another processor), skips to next tasklet. (Recall that only one tasklet of a given type may run concurrently.)
			  6. **Mark as Running**: If not running, sets `TASKLET_STATE_RUN` flag to block other processors from running this tasklet concurrently.
			  7. **Check if Enabled**: Verifies tasklet’s `count` is zero (not disabled); if disabled, skips to next tasklet.
			  8. **Run Tasklet Handler**: Executes the tasklet’s handler function to perform deferred work (e.g., process network packets).
			  9. **Clear Running Flag**: After handler finishes, clears `TASKLET_STATE_RUN` flag, allowing the tasklet to run again if rescheduled.
			  10. Repeat for the next pending tasklet, until there are no more scheduled tasklets waiting to run.

	- ##### Using Tasklets
		- **Purpose**: Preferred for driver bottom halves; dynamic, easy, fast.
		- ###### Declaring Your Tasklet
			- Defines tasklet with handler function and data argument.
			- **Static**: `DECLARE_TASKLET(name, func, data)` (enabled, count=0).
			- **Static**: `DECLARE_TASKLET_DISABLED(name, func, data)` (disabled, count=1).
			- **Dynamic**: `tasklet_init(t, func, data)` for pointer to `tasklet_struct`.
		- ###### Writing Your Tasklet Handler
			- **Prototype**: `void tasklet_handler(unsigned long data)`.
			- **Constraints**:
				- Cannot sleep (no semaphores or blocking calls).
				- Runs with interrupts enabled; use locks for shared data.
			- **Concurrency**: Same tasklet never runs concurrently, but different tasklets can.
		- ###### Scheduling Your Tasklet
			- **Function**: `tasklet_schedule(&my_tasklet)` marks tasklet as pending.
			- **Behavior**:
				- Runs once soon after scheduling, on same processor for cache efficiency.
				- Rescheduling before running: still runs once; Rescheduling during running: runs again.
			- **Control**:
				- `tasklet_disable(&my_tasklet)`: Disables, waits for completion.
				- `tasklet_disable_nosync(&my_tasklet)`: Disables without waiting.
				- `tasklet_enable(&my_tasklet)`: Enables tasklet (needed for disabled ones).
				- `tasklet_kill(&my_tasklet)`: Removes from queue, waits for completion, not for interrupt context.
			- **Example**: Schedule network tasklet to process packets.

- #### ksoftirqd
	- **Purpose**: Per-process kernel threads (`ksoftirqd/n`) handle softirqs/tasklets during high load.
	- **Problem**: High softirq rates (e.g., network traffic) or reactivated softirqs can starve user-space.
	- **Solutions Considered**:
		- **Process All Softirqs Immediately**: Risks starving user-space under heavy load.
		- **Ignore Reactivated Softirqs**: Delays softirq processing, poor for idle systems.
	- **Compromise**: `ksoftirqd` threads process excess softirqs, run at low priority (nice 19).
	- **Operation**:
		- One thread per processor, named `ksoftirqd/n` (e.g., `ksoftirqd/0`, `ksoftirqd/1`).
		- Loops: Checks `softirq_pending(cpu)`, calls `do_softirq()` if pending.
		- Handles reactivated softirqs in loop, yields via `schedule()` if needed.
		- Sets `TASK_INTERRUPTIBLE` when idle, schedules out.
	- **Trigger**: Woken by `do_softirq()` when softirqs reactivate excessively.
	- **Benefits**: Prevents user-space starvation, ensures timely softirq processing, fast on idle systems.
	- **Example**: Network softirq overload wakes `ksoftirqd` to process packets.

- #### Work Queues
	- **Purpose**: Defer tasks to kernel threads running in process context, allowing sleep.
	- **Overview**: Work queues run deferred work in process context, unlike softirqs/tasklets.
	- **Benefits**: Can sleep, supports blocking tasks (e.g., memory allocation, I/O).
	- **Use Case**: Choose work queues if task needs to sleep; otherwise, use tasklets.
	- **Preference**: Preferred over custom kernel threads, easier to use.
	- **Example**: Defer driver’s disk I/O to work queue.
	- ##### Implementing Work Queues
		- **Mechanism**: Interface to create kernel threads (worker threads) for deferred work.
		- **Default Threads**: Named events/n (n = processor number, e.g., `events/0` for uniprocessor).
		- **Usage**: Default thread handles work from multiple drivers; custom threads possible.
		- **Custom Threads**: Useful for heavy or performance-critical tasks to avoid starving other work running on default threads.
		- ###### Data Structures Representing the Threads
			- **Key Structure**: The worker threads are represented by the `struct workqueue_struct`, defined in `kernel/workqueue.c`
				- Contains array of `cpu_workqueue_struct` (one per processor).
				  ```c
				  /* The externally visible workqueue abstraction
				-  * is an array of per-CPU workqueues:
				-  */
				  struct workqueue_struct {
					struct cpu_workqueue_struct cpu_wq[NR_CPUS];
					struct list_head list;
					const char *name;
					int singlethread;
					int freezeable;
					int rt;
				};
				  ```
			- Because the worker threads exist on each processor in the system, there is one of these structures per worker thread, per processor, on a given machine.
			- **Core Structure**: `struct cpu_workqueue_struct` represents one worker thread per processor - also defined in `kernel/workqueue.c`
			  ```c
				struct cpu_workqueue_struct {
					spinlock_t lock; /* lock protecting this structure */
					struct list_head worklist; /* list of work */
					wait_queue_head_t more_work;
					struct work_struct *current_struct;
					struct workqueue_struct *wq; /*associated workqueue_struct */
					task_t *thread; /* associated thread */
				};			  
			  ```
			  - Note that each *type* of worker thread has one `workqueue_struct` associated to it. Inside, there is one `cpu_workqueue_struct` for every thread and, thus, every processor, because there is one worker thread on each processor.
		- ###### Data Structures Representing the Work
			- All worker threads are implemented as normal kernel threads running the `worker_thread()` function.
			- After initial setup, this function enters an infinite loop and goes to sleep.
			- When work is queued, the thread is awakened and processes the work. When there is no work left to process, it goes back to sleep.
			- The work is represented by the `work_struct` structure, defined in `<linux/workqueue.h>`:
			  ```c
			 struct work_struct {
				atomic_long_t data;
				struct list_head entry;
				work_func_t func;
			 };
			  ```
			- Fields: `data` (argument), `entry` (linked list link), `func` (handler function).
			- Organized in per-processor linked lists, one per queue type (e.g., default queue).
			- **worker_thread() Steps**:
			1. Marks itself `TASK_INTERRUPTIBLE`, joins wait queue.
			2. If work list empty, calls `schedule()`, sleeps.
			3. If work list not empty, sets `TASK_RUNNING`, leaves wait queue.
			4. Calls `run_workqueue()` to process work.
			
			- The function `run_workqueue()`, in turn, actually performs the deferred work:
			  1. Loops through work list while not empty.
			  2. Gets next work’s `func` and `data`.
			  3. Removes work from list, clears pending bit.
			  4. Runs the work’s `func`.
			  5. Repeats for next work.

	- ##### Work Queue Implementation Summary
	  ![[Pasted image 20250627072202.png]]
	  
		- **Overview**: Describes how work queue data structures connect (visualized in Figure 8.1).
		- **Worker Threads**:
			- One thread per processor per type (e.g., `events/n` for default, `falcon/n` for custom).
			- Represented by `cpu_workqueue_struct` (one per thread).
		- **Workqueue Structure**:
			- `workqueue_struct` represents all threads of a type (e.g., one for *events*, one for *falcon*).
			- Contains array of `cpu_workqueue_struct` for each processor.
		- **Work**:
			- The driver creates work, which it wants to defer to later.The `work_struct` structure represents this work.
			- Submitted to a worker thread (e.g., `falcon/0`), which wakes and executes it.
		- **Default vs. Custom**:
			- Most drivers use default *events* threads (simple, shared).
			- Some more serious situations, however, demand their own worker threads.
			  The XFS filesystem, for example, creates two new types of worker threads.

	- ##### Using Work Queues
		- **Overview**: Steps to create, manage, and schedule work in work queues.
		- ###### Creating Work
			- **Methods**:
				- `DECLARE_WORK(name, func, data)`: Statically creates `work_struct` with handler `func` and `data`.
				- `INIT_WORK(work, func, data)`: Dynamically initializes `work_struct` pointer.
			- **Purpose**: Prepares work item for queuing with handler and argument.
		- ###### Your Workqueue Handler
			- **Prototype**: `void work_handler(void *data)`
			- **Context**: Runs in process context, interrupts enabled, can sleep.
			- **Limitation**: Cannot access user-space memory (no user-space mapping).
			- **Locking**: Uses standard process-context locking (covered in Chapters 9, 10).
			- **Example**: Handler performs disk I/O for driver.
		- ###### Scheduling Work
			- **Functions**:
				- `schedule_work(&work)`: Queues work to default events thread, runs when thread wakes.
				- `schedule_delayed_work(&work, delay)`: Queues work after delay ticks (see Chapter 10).
			- **Behavior**: Work executes in process context via worker thread.
			- **Example**: Schedule driver task after interrupt.
		- ###### Flushing Work
			- **Function**: `flush_scheduled_work()`: Waits until all default queue work completes.
			- **Use Case**: Ensures no pending work before module unload or to avoid races.
			- **Limitation**: Process context only (sleeps); doesn’t cancel delayed work.
			- **Cancel Delayed Work**: `cancel_delayed_work(&work)` cancels pending delayed work.
			- **Example**: Flush work before unloading driver module.
		- ###### Creating New Work Queues
			- **Function**: `create_workqueue(name):` Creates custom work queue with per-processor threads.
			- **Naming**: Threads named *name/n* (e.g., `events/0` for default queue).
			- **Functions for Custom Queues**:
				- `queue_work(wq, &work)`: Schedules work on custom queue.
				- `queue_delayed_work(wq, &work, delay)`: Schedules delayed work on custom queue.
				- `flush_workqueue(wq)`: Waits for custom queue to empty.
			- **Use Case**: Dedicated queues for performance-critical tasks.
			- **Example**: Create queue for driver-specific heavy processing.

- #### The Old Work Queue Mechanism
	- **Concept**: Task queues (`tq`) were an older method to defer work, replaced by work queues.
	- **Structure**: Multiple named queues (e.g., *scheduler*, *timer*, *immediate*) for different tasks.
	- **Operation**: Scheduler queue used kernel thread `keventd` (predecessor to work queues); others ran at specific times (e.g., timer queue at system tick, and, immediate queue "*immediately*" (hack!)).
	- **Strength**: Simple interface for queuing work; scheduler queue enabled process-context deferral.
	- **Weakness**: Arbitrary queue types, inconsistent execution rules, poorly organized.
	- **Evolution**: In 2.5 kernel, split into tasklets (for some users) and work queues (for others).
	- **Status**: Obsolete in 2.6, fully replaced by work queues for process-context work.

- #### Which Bottom Half Should I Use
	- **Choices**: Softirqs, tasklets, work queues (2.6 kernel).
	- **Softirqs**:
		- Least serialization, same type runs concurrently on processors.
		- Best for high-frequency, timing-critical tasks (e.g., networking).
		- Needs careful concurrency handling (e.g., per-processor variables).
	- **Tasklets**:
		- Built on softirqs, simpler interface, no concurrent same-type execution.
		- Preferred for drivers unless concurrency handling is ready.
	- **Work Queues**:
		- Run in process context, only choice if sleep is needed (e.g., I/O, memory allocation).
		- Highest overhead (context switching), less suited for high-frequency tasks.
	- **Ease of Use**:
		- Work queues easiest (default `events` queue simple).
		- Tasklets next, softirqs hardest (static creation, complex concurrency).
	- **Selection Guide**:
		- Use work queues for process context/sleeping tasks.
		- Use tasklets for simple, non-sleeping driver tasks.
		- Use softirqs for performance-critical, threaded tasks.
	![[Pasted image 20250629091246.png]]

- #### Locking Between the Bottom Halves
	- **Need**: Protect shared data from concurrent access, even on single-processor systems.
	- **Tasklets**: Same tasklet doesn’t run concurrently, no intra-tasklet locking needed; different tasklets sharing data need locks.
	- **Softirqs**: No serialization, same softirq can run concurrently on different processors, all shared data needs locks.
	- **Work Queues**: Run in process context, require standard kernel locking (like other process context code).
	- **Process Context Sharing**: Disable bottom halves and acquire lock to protect shared data, ensures SMP safety, prevents deadlocks.
	- **Interrupt Context Sharing**: Disable interrupts and acquire lock to protect shared data, ensures SMP safety, prevents deadlocks.
	- **Details**: Locking covered in Chapters 9 (concurrency) and 10 (locking primitives).
	  **Note:** ***SMP - Symmetric Multi-Processing :*** *A system with multiple processors that share the same memory and can run tasks concurrently.*

- #### Disabling Bottom Halves
	- **Purpose**: Disable softirqs/tasklets to protect shared data in core kernel code.
	- **Functions**:
		- `local_bh_disable()`: Disables softirq/tasklet processing on local processor.
		- `local_bh_enable()`: Enables softirq/tasklet processing on local processor.
	- **Mechanism**: Uses `preempt_count` (per-task counter) to track disable calls.
		- Increment by `SOFTIRQ_OFFSET` to disable; decrement to enable.
		- Only final `local_bh_enable()` re-enables processing (nesting supported).
	- **Execution**: `local_bh_enable()` runs pending softirqs if `preempt_count` reaches zero.
	- **Implementation**: Architecture-specific macros in `<asm/softirq.h>`.
	- **Work Queues**: Not affected (run in process context, use standard locking).
	- **Use Case**: Combine with locks for SMP safety (details in Chapters 9, 10).
	- **Example**: Disable softirqs before accessing shared driver data.


## **Quick Recall**
- **Q: Why use bottom halves?**
	- **A**: Defer heavy tasks from interrupt handlers to reduce interrupt-off time, improve responsiveness.
- **Q: How do softirqs differ from tasklets?**
	- **A**: Softirqs are static, run concurrently (same type), high-performance; tasklets are dynamic, serialized, simpler.
- **Q: When to use work queues?**
	- **A**: For tasks needing process context (e.g., sleeping, I/O); otherwise, use tasklets/softirqs.
- **Q: What does** `ksoftirqd` **do?**
	- **A**: Per-processor threads process excess softirqs/tasklets at low priority to prevent user-space starvation.
- **Q: How to disable softirqs/tasklets?**
	- **A**: `local_bh_disable()` disables, `local_bh_enable(`) enables, using `preempt_count`.
- **Q: Why was the BH interface removed?**
	- **A**: Static, limited to 32, non-scalable on SMP; replaced by softirqs/tasklets/work queues.
- **Q: How do work queues handle work?**
	- **A**: `worker_thread()` loops, sleeps when idle, runs `run_workqueue()` to execute `work_struct` tasks.

## **Hands-on Ideas**
- **GPIO Work Queue**:
	- Write BBB kernel module to defer GPIO button press handling to work queue.
	- Use `INIT_WORK` and `schedule_work` to toggle LED.
	- Test with button presses, check `/proc/kallsyms` for worker thread.
	- **Learn**: Work queue setup, process context execution.
- **Tasklet vs. Work Queue Comparison**:
	- Create BBB module with GPIO interrupt: one path uses tasklet, another uses work queue.
	- Measure latency (using `ktime_get()`) for LED toggle in each.
	- Test under load (e.g., `stress` tool).
	- **Learn**: Performance differences, context trade-offs.
- **Softirq Overload Test**:
	- Simulate high-frequency GPIO interrupts on BBB to trigger `ksoftirqd`.
	- Monitor `ksoftirqd/n` activity via `top` or `/proc/stat`.
	- Log softirq counts in `/proc/softirqs`.
	- **Learn**: `ksoftirqd` role in heavy loads.
- **Custom Work Queue**:
	- Create BBB module with custom work queue using `create_workqueue()`.
	- Queue multiple tasks (e.g., log GPIO states) with `queue_work()`.
	- Flush queue with `flush_workqueue()` before module unload.
	- **Learn**: Custom queue management, flushing.
- **Locking with Tasklets**:
	- Write BBB module with two tasklets sharing a counter, using `spinlock`.
	- Trigger tasklets via GPIO interrupts, verify counter consistency.
	- Test without lock to observe SMP issues (on multi-core BBB).
	- **Learn**: Tasklet serialization, SMP locking needs.