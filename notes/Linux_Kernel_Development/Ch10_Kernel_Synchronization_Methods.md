# Chapter 10: [Kernel Synchronization Methods]

## **Summary**
- ### Introduction
	- **Purpose**: Describes kernel mechanisms to protect shared data and prevent race conditions.
	- **Mechanisms**: Includes atomic operations, spinlocks, semaphores, mutexes, and more.
	- **Context**: Builds on Chapter 9, providing practical tools for synchronization.
	- **Goal**: Ensure data integrity in concurrent environments (e.g., SMP, preemption).
	- **Example**: Atomic increment for counters, spinlocks for critical regions.

- ### Atomic Operations
	- **Purpose**: Provide indivisible instructions to prevent race conditions in shared data.
	- **Types**: Atomic integer operations (on `atomic_t`, `atomic64_t`) and bitwise operations (on memory bits).
	- **Benefit**: Execute without interruption, ensuring consistent data states.
	- #### Atomic Integer Operations
		- **Type**: `atomic_t` (32-bit integer, `volatile int counter`, in `<linux/types.h>`).
		  ```c
			typedef struct {
			  volatile int counter;
			} atomic_t;
		  ```
		- **Why** `atomic_t`**:** Ensures atomic use, prevents compiler optimization errors, hides architecture differences.
		- **Example**:
		  ```c
			atomic_t v;                   /* define v */
			atomic_t u = ATOMIC_INIT(0); /* define u and initialize it to zero */
			atomic_set(&v, 4);    /* v = 4 (atomically) */
			atomic_add(2, &v);    /* v = v + 2 = 6 (atomically) */
			atomic_inc(&v);       /* v = v + 1 = 7 (atomically) */
			printk(“%d\n”, atomic_read(&v)); /* will print “7” */
		  ```
		- **Atomic Integer Methods:**
			- `ATOMIC_INIT(int i)` - At declaration, initialize to i.
			- `int atomic_read(atomic_t *v)` - Atomically read the integer value of v.
			- `void atomic_set(atomic_t *v, int i)` - Atomically set v equal to i.
			- `void atomic_add(int i, atomic_t *v)` - Atomically add i to v.
			- `void atomic_sub(int i, atomic_t *v)` - Atomically subtract i from v.
			- `void atomic_inc(atomic_t *v)` - Atomically add one to v.
			- `void atomic_dec(atomic_t *v)` - Atomically subtract one from v.
			- `int atomic_sub_and_test(int i, atomic_t *v)` - Atomically subtract i from v and return `true` if the result is zero; otherwise `false`.
			- `int atomic_add_negative(int i, atomic_t *v)` - Atomically add i to v and return `true` if the result is negative; otherwise `false`.
			- `int atomic_add_return(int i, atomic_t *v)` - Atomically add i to v and return the result.
			- `int atomic_sub_return(int i, atomic_t *v)` - Atomically subtract i from v and return the result.
			- `int atomic_inc_return(int i, atomic_t *v)` - Atomically increment v by one and return the result.
			- `int atomic_dec_return(int i, atomic_t *v)` - Atomically decrement v by one and return the result.
			- `int atomic_dec_and_test(atomic_t *v)` - Atomically decrement v by one and return `true` if zero; `false` otherwise.
			- `int atomic_inc_and_test(atomic_t *v)` - Atomically increment v by one and return `true` if the result is zero; `false` otherwise.

	- #### Atomicity versus Ordering
		- **Atomicity**: Ensures instructions complete uninterrupted (e.g., read/write of a word).
		- **Ordering**: Ensures specific instruction sequence across threads/processors (needs *barriers*, later in chapter).
		- **Example**: Atomic read (`atomic_read`) ensures consistent value, not specific timing.
		- **Note**: Atomic operations guarantee atomicity, not ordering (Memory barriers like `smp_mb()` used for ordering in modern kernels).

	- #### 64-Bit Atomic Operations
		- **Type**: `atomic64_t` (64-bit, `volatile long counter`), for 64-bit architectures.
		  ```c
			typedef struct {
				volatile long counter;
			} atomic64_t;
		  ```
		- **Atomic Integer Methods:** Similar to `atomic_t` (e.g., `atomic64_inc`, `atomic64_read`), prefixed with *atomic64*.
		- **Support**: Available on 64-bit architectures; some 32-bit (e.g., x86-32) support it.
		- **Use Case**: Architecture-specific code needing 64-bit counters (Common in modern drivers for large data tracking).
		- **Caution**: Use `atomic_t` for portability across all architectures.

	- #### Atomic Bitwise Operations
		- **Scope**: Operate on any memory address, bit number (0 = LSB, 31/63 = MSB on 32/64-bit). Defined in `<asm/bitops.h>`.
		- **Use Case**: Manipulate flags or hardware registers atomically.
		- **Example**:
		  ```c
			set_bit(0, &word);      /* bit zero is now set (atomically) */
			set_bit(1, &word);      /* bit one is now set (atomically) */
			printk(“%ul\n”, word);  /* will print “3” */
			clear_bit(1, &word);    /* bit one is now unset (atomically) */
			change_bit(0, &word);   /* bit zero is flipped; now it is unset (atomically) */

			/* atomically sets bit zero and returns the previous value (zero) */
			if (test_and_set_bit(0, &word)) {
				/* never true ... */
			}

			/* the following is legal; you can mix atomic bit instructions with normal C */
			word = 7;
		  ```
		- **Nonatomic Variants**: Prefixed __ (e.g., `__set_bit`), faster if data already protected.
		- **Use Case**: Manipulate flags or hardware registers atomically.
		- **Helpers**: find_first_bit, find_first_zero_bit for bit searches (Used in memory management, CPU masks).
		- **Atomic Bitwise Methods**
			- `void set_bit(int nr, void *addr)`- Atomically set the `nr`-*th* bit starting from `addr`.
			- `void clear_bit(int nr, void *addr)` - Atomically clear the `nr`-*th* bit starting from `addr`.
			- `void change_bit(int nr, void *addr)`- Atomically flip the value of the `nr`-*th* bit starting from `addr`.
			- `int test_and_set_bit(int nr, void *addr)` - Atomically set the `nr`-*th* bit starting from `addr` and return the previous value.
			- `int test_and_clear_bit(int nr, void *addr)` - Atomically clear the `nr`-*th* bit starting from `addr` and return the previous value.
			- `int test_and_change_bit(int nr, void *addr)` - Atomically flip the `nr`-*th* bit starting from `addr` and return the previous value.
			- `int test_bit(int nr, void *addr)` - Atomically return the value of the `nr`-*th* bit starting from `addr`.

	- #### What the Heck Is a Nonatomic Bit Operation?
		- **Issue**: Nonatomic bit operations (e.g., `__set_bit`) may not ensure intermediate states.
		- **Atomicity**: Guarantees all operations complete fully, preserving all states (e.g., *set* then *clear*).
		- **Nonatomic Risk**: Operations may overlap, missing intermediate states (e.g., *set* skipped).
			- **Why "Skipped"?**:
				- Without atomicity, operations can interleave (e.g., two threads on different processors).
				- If a `__set_bit` and `__clear_bit` occur simultaneously, the *set* may not be visible (overwritten).
				- Result: Final state (e.g., bit cleared) is correct, but intermediate state (bit set) may never exist or set operation appears "skipped."
			- **Why It Matters**:
				- Intermediate states are critical for hardware registers or when other code depends on them.
				- Example: A driver sets a bit to signal a device, then clears it; nonatomic operations may skip the signal.
			- 
		- **Importance**: Critical for hardware registers or ordered operations.
		- **Example**: Atomic `set_bit` ensures bit is set before cleared; non-atomic may skip *set*.


- ### Spin Locks
	- **Purpose**: Protect complex critical regions (e.g., multi-function data updates) where atomic operations are insufficient.
	- **Definition**: Single-holder lock; contended threads busy-wait (spin), consuming processor time.
	- **Behavior**: Only one thread enters critical region; others spin until lock is released.
	- **Use Case**: Short-duration locks (< two context switches) to minimize spin overhead.
	- **Suitability**: Safe in interrupt handlers, unlike semaphores (which sleep).

	- #### Spin Lock Methods
		- Spin locks are architecture-dependent and implemented in assembly.The architecture-dependent code is defined in `<asm/spinlock.h>`. The actual usable interfaces are defined in `<linux/spinlock.h>`.
		- **Usage**: The basic use of a spin lock is:
		  ```c
			DEFINE_SPINLOCK(mr_lock);
			spin_lock(&mr_lock);
			/* critical region ... */
			spin_unlock(&mr_lock);
		  ```
		- The lock can be held simultaneously by at most only one thread of execution. Consequently, only one thread is allowed in the critical region at a time.This provides the needed protection from concurrency on multiprocessing machines.
		- **Uniprocessor**: Locks compile away, only disable/enable preemption.
		- Spin locks can be used in interrupt handlers, whereas semaphores cannot be used because they sleep. If a lock is used in an interrupt handler, you must also disable local interrupts (interrupt requests on the current processor) before obtaining the lock.
		- The kernel provides an interface that conveniently disables interrupts and acquires the lock. Usage is:
		  ```c
			DEFINE_SPINLOCK(mr_lock);
			unsigned long flags;
			
			spin_lock_irqsave(&mr_lock, flags);
			/* critical region ... */
			spin_unlock_irqrestore(&mr_lock, flags);
		  ```
		  - **Interrupt-Safe**: `spin_lock_irqsave(&mr_lock, flags)` disables interrupts, saves state; `spin_unlock_irqrestore(&mr_lock, flags)` restores.
		  - **Optimized**: `spin_lock_irq(&mr_lock)` assumes interrupts enabled; not recommended.

		- ##### Warning: Spin Locks Are Not Recursive!
			- **Issue**: Acquiring a held spinlock causes self-deadlock (spins waiting for itself).
			- **Rule**: Avoid double-acquiring same lock in Linux (non-recursive), unlike some operating systems with recursive locks.
		- ##### What Do I Lock?
			- **Rule**: Lock data, not code, to protect shared resources (e.g., `struct foo` with `foo_lock`).
			- **Practice**: Clearly associate locks with specific data to avoid race conditions.
			- **Avoid**: Locking code regions, as it’s error-prone (Modern kernels emphasize data-specific locking).
		- ##### Debugging Spin Locks
			- **Tools**: `CONFIG_DEBUG_SPINLOCK` checks for uninitialized or invalid lock usage.
			- **Advanced**: `CONFIG_DEBUG_LOCK_ALLOC` tracks lock lifecycles (Use `lockdep` for deadlock detection in modern kernels).
			- **Practice**: Enable debugging during testing to catch errors.

	- #### Other Spin Lock Methods
		- `spin_lock()` - Acquires given lock
		- `spin_lock_irq()` - Disables local interrupts and acquires given lock
		- `spin_lock_irqsave()` - Saves current state of local interrupts, disables local interrupts, and acquires given lock
		- `spin_unlock()` - Releases given lock
		- `spin_unlock_irq()` - Releases given lock and enables local interrupts
		- `spin_unlock_irqrestore()` - Releases given lock and restores local interrupts to given previous state
		- `spin_lock_init()` - Dynamically initializes given spinlock_t
		- `spin_trylock()` - Tries to acquire given lock; if unavailable, returns nonzero
		- `spin_is_locked()` - Returns nonzero if the given lock is currently acquired, otherwise it returns zero

	- #### Spin Locks and Bottom Halves
		- **Tasklets**: Same-type tasklets don’t run concurrently; no lock needed within same tasklet.
		- **Softirqs**: Same-type softirqs may run on different processors; need locks for shared data.
		- **Functions**: `spin_lock_bh(&lock)` obtains the lock and disables bottom halves; `spin_unlock_bh(&lock)` disables the lock and enables bottom halves.
		- **Protection**: Use `spin_lock_bh` for process-bottom half sharing; `spin_lock_irqsave` for interrupt-bottom half sharing (Common in modern drivers for mixed-context safety).

- ### Reader-Writer Spin Locks
	- Sometimes, lock usage can be clearly divided into reader and writer paths. For example, consider a list that is both updated and searched. When the list is updated (written to), it is important that no other threads of execution concurrently write to *or* read from the list. Writing demands mutual exclusion.
	- On the other hand, when the list is searched (read from), it is only important that nothing else writes to the list. Multiple concurrent readers are safe so long as there are no writers.
	- The task list’s access patterns fit this description. Not surprisingly, a *reader-writer spin lock* protects the task list.
	- **Purpose**: Protect data with distinct reader (read-only) and writer (read-write) paths.
	- **Behavior**: Multiple readers can hold lock concurrently; only one writer holds it exclusively, with no readers.
	- **Use Case**: Data structures like task lists where reads are frequent, writes need mutual exclusion (e.g., Chapter 3’s task list).
	- **Names**: Also called *shared/exclusive* or *concurrent/exclusive* locks.
	- **Example**: Normally, the readers and writers are in entirely separate code paths, such as in this example.
	  ```c
	  DEFINE_RWLOCK(mr_rwlock);
	  
	  read_lock(&mr_rwlock);
	  /* critical section (read only) ... */
	  read_unlock(&mr_rwlock);
	  
	  write_lock(&mr_rwlock);
	  /* critical section (read and write) ... */
	  write_unlock(&mr_lock);
	  ```
	- **Limitation**: Cannot upgrade read lock to write lock (causes deadlock). Use write lock from start if writing possible.
	  ```c
	  read_lock(&mr_rwlock);
	  write_lock(&mr_rwlock);
	  ```
		- Executing these two functions as shown will deadlock, as the write lock spins, waiting for all readers to release the shared lock—including yourself. If you ever need to write, obtain the write lock from the start.
	- **Interrupt Handling**:
		- Readers in interrupts: Use `read_lock()` (no interrupt disable needed if no writers in interrupts).
		- Writers in interrupts: Use `write_lock_irqsave()` to avoid deadlock.
	- **Bias**: Favors readers; writers wait until all readers release, risking writer starvation.
	- **Consideration**: Avoid if frequent writes or balanced read/write; use spin locks instead (Modern kernels prefer *RCU* for read-heavy cases)
	- **Suitability**: Best for short hold times, non-sleeping code (like spin locks); use semaphores for long holds or sleeping.
	- **Reader-Writer Spin Lock Methods:**
		- `read_lock()` - Acquires given lock for reading
		- `read_lock_irq()` - Disables local interrupts and acquires given lock for reading
		- `read_lock_irqsave()` - Saves the current state of local interrupts, disables local interrupts, and acquires the given lock for reading
		- `read_unlock()` - Releases given lock for reading
		- `read_unlock_irq()` - Releases given lock and enables local interrupts
		- `read_unlock_ irqrestore()` - Releases given lock and restores local interrupts to the given previous state
		- `write_lock()` - Acquires given lock for writing
		- `write_lock_irq()` - Disables local interrupts and acquires the given lock for writing
		- `write_lock_irqsave()` - Saves current state of local interrupts, disables local interrupts, and acquires the given lock for writing
		- `write_unlock()` - Releases given lock
		- `write_unlock_irq()` - Releases given lock and enables local interrupts
		- `write_unlock_irqrestore()` - Releases given lock and restores local interrupts to given previous state
		- `write_trylock()` - Tries to acquire given lock for writing; if unavailable, returns nonzero
		- `rwlock_init()` - Initializes given `rwlock_t`

- ### Read-Copy Update (RCU)
	- **What is RCU?**
		- **Definition**: Read-Copy-Update is a synchronization mechanism for read-heavy scenarios with rare writes.
		- **Purpose**: Allows multiple readers to access data concurrently with minimal overhead while writers update safely.
	- **Why Use RCU?**
		- **Efficiency**: Readers don’t lock, reducing contention compared to reader-writer spin locks.
		- **Scalability**: Ideal for SMP systems with many processors, as reads scale without writer starvation risks.
		- **Use Case**: Protects data structures like lists (e.g., task lists, network tables) where reads dominate.
	- **How RCU Works**:
		- **Readers**: Access data without locks, using `rcu_read_lock()` and `rcu_read_unlock()` to mark read section.
		- **Writers**: Create a new copy of data, update it, and replace the old data; old data is freed after all readers finish.
		- **Grace Period**: Writers wait for all readers to complete (grace period) before freeing old data, ensuring safety.
	- **Key Points**:
		- Readers are lightweight, non-blocking, and preemptible.
		- Writers ensure old data persists until no readers use it.
		- Defined in `<linux/rcupdate.h>`; common in modern kernels (Widely used in networking, memory management).
	- **Comparison to Reader-Writer Spin Locks**:
		- **RW Locks**: Readers lock, writers exclude all; risk writer starvation.
		- **RCU**: Readers lock-free, writers use copy-update; better for read-heavy cases but complex for writes.
		- **Example**: Task list reads use RCU for fast access; updates are rare, avoiding lock contention.
	- **Real-World Analogy**:
		- Like a library where readers borrow books without locking shelves, while librarians update the catalog by creating a new version and swapping it after all readers return old books.

### Semaphores
- **Definition**: Sleeping locks; unavailable semaphore puts task to sleep on wait queue, freeing processor.
- **Behavior**: Only one task (for binary semaphore or mutex) or up to count tasks (for counting semaphore) hold lock.
- **Suitability**: Best for long hold times or code that may sleep (e.g., user-space sync).
- **Constraints**: Only in process context (not interrupt context); can sleep while holding.
- **Comparison**: Higher overhead than spin locks (due to sleep/wake); doesn’t disable preemption.
- **Real-World Analogy**: Let’s jump back to the door and key analogy.
	- When a person reaches the door, he can grab the key and enter the room.
	- The big difference lies in what happens when another dude reaches the door and the key is not available. In this case, instead of spinning, the fellow puts his name on a list and takes a number.
	- When the person inside the room leaves, he checks the list at the door. If anyone’s name is on the list, he goes over to the first name and gives him a playful jab in the chest, waking him up and allowing him to enter the room.
	- In this manner, the key (read: semaphore) continues to ensure that there is only one person (read: thread of execution) inside the room (read: critical region) at one time.

- #### Counting and Binary Semaphores
	- A final useful feature of semaphores is that they can allow for an arbitrary number of simultaneous lock holders. Whereas spin locks permit at most one task to hold the lock at a time, the number of permissible simultaneous holders of semaphores can be set at declaration time. This value is called the *usage count* or simply the *count*.
	- **Types**:
		- **Binary Semaphore (Mutex)**: count = 1, enforces mutual exclusion (one holder).
		- **Counting Semaphore**: count > 1, allows multiple holders (up to count), limits resource access.
	- **Usage**: Mutex for mutual exclusion; counting semaphore for resource limits (rare in kernel).
	- **Operations**: `down()` (acquire, decrement count), `up()` (release, increment count).
	- **Real-World Analogy**: Single bathroom key (mutex) vs. multiple theater tickets (counting); only ticket holders enter.
	- **Note**: Kernel prefers mutexes (Modern kernels use `struct mutex` over semaphores for simplicity)

- #### Creating and Initializing Semaphores
	- **Type**: `struct semaphore` defined in `<asm/semaphore.h>`.
	- **Static Creation**:
		- `struct semaphore name`; `sema_init(&name, count)`; for any count.
		- `static DECLARE_MUTEX(name)`; for mutex (count = 1).
	- **Dynamic Creation**: `sema_init(sem, count)`; or `init_MUTEX(sem)`; where `sem` is a pointer and `count` is the usage count of the semaphore.
	- **Note**: Naming inconsistencies exist (e.g., `init_MUTEX` vs. `sema_init`) (Modern `struct mutex` uses `mutex_init`).

- #### Using Semaphores
	- **Acquire**:
		- The function `down_interruptible()` attempts to acquire the given semaphore. If the semaphore is unavailable, it places the calling process to sleep in the `TASK_INTERRUPTIBLE` state (This process state implies that a task can be awakened with a signal, which is generally a good thing). If the task receives a signal while waiting for the semaphore, it is awakened and `down_interruptible()` returns `-EINTR`.
		- The function `down()`places the task in the `TASK_UNINTERRUPTIBLE` state when it sleeps. You most likely do not want this because the process waiting for the semaphore does not respond to signals. Therefore, use of `down_interruptible()` is much more common (and correct) than `down()`.
		- `down_trylock(&sem)`: Non-blocking, returns non-zero if unavailable, zero if acquired.
	- **Release**:
		- To release a given semaphore, call `up()`. It wakes a waiting task if queue exists.
	- **Example**:
	  ```c
		/* define and declare a semaphore, named mr_sem, with a count of one */
		static DECLARE_MUTEX(mr_sem);
	  
		/* attempt to acquire the semaphore ... */
		if (down_interruptible(&mr_sem)) {
			/* signal received, semaphore not acquired ... */
		}
		
		/* critical region ... */
		
		/* release the given semaphore */
		up(&mr_sem);
	  ```

- #### Semaphore Methods
	- `sema_init(struct semaphore *, int)` - Initializes the dynamically created semaphore to the given count.
	- `init_MUTEX(struct semaphore *)` - Initializes the dynamically created semaphore with a count of one. 
	- `init_MUTEX_LOCKED(struct semaphore *)` - Initializes the dynamically created semaphore with a count of zero (so it is initially locked).
	- `down_interruptible (struct semaphore *)` - Tries to acquire the given semaphore and enter interruptible sleep if it is contended.
	- `down(struct semaphore *)` - Tries to acquire the given semaphore and enter uninterruptible sleep if it is contended.
	- `down_trylock(struct semaphore *)` - Tries to acquire the given semaphore and immediately return nonzero if it is contended.
	- `up(struct semaphore *)` - Releases the given semaphore and wakes a waiting task, if any.

### Reader-Writer Semaphores
- **Purpose**: Protect data with distinct read (read-only) and write (read-write) paths, like reader-writer spin locks.
- **Behavior**: Multiple readers can hold lock concurrently; only one writer holds it exclusively (no readers).
- **Type**: `struct rw_semaphore` in `<linux/rwsem.h>`, always mutexes (count = 1).
- **Use Case**: Data with frequent reads, rare writes, needing sleep (e.g., user-space sync) (Often replaced by RCU for read-heavy cases)
- **Real-World Analogy**: Library reading room; many can read books (readers), but only one can reorganize shelves (writer).
- #### Creating and Initializing Reader-Writer Semaphores
	- **Static Creation**: `static DECLARE_RWSEM(name)`; where `name` is the declared name of the new semaphore.
	- **Dynamic Creation**: `init_rwsem(&sem)`; for pointer to semaphore.
	- **Real-World Analogy**: Setting up a library policy; allow multiple readers or one writer at a time.
	- **Note**: Always initialized as mutex (count = 1) (Modern kernels use struct mutex for simpler mutex cases).
- #### Using Reader-Writer Semaphores
  ```c
	static DECLARE_RWSEM(mr_rwsem);
	
	/* attempt to acquire the semaphore for reading ... */
	down_read(&mr_rwsem);
	
	/* critical region (read only) ... */

	/* release the semaphore */
	up_read(&mr_rwsem);
	
	/* ... */

	/* attempt to acquire the semaphore for writing ... */
	down_write(&mr_rwsem);

	/* critical region (read and write) ... */

	/* release the semaphore */
	up_write(&mr_sem);
  ```
	- **Try Locks**:
		- `down_read_trylock(&sem)`: Returns non-zero if acquired, zero if contended.
		- `down_write_trylock(&sem)`: Same, but for write lock (**note:** opposite of semaphore convention).
	- **Special Function**: `downgrade_write(&sem)` converts write lock to read lock atomically.
	- **Sleep**: Uses uninterruptible sleep (no interruptible variant).
	- **Real-World Analogy**: Readers browse books without disturbing others; writer locks room for updates; downgrade lets writer read after reorganizing.
	- **Note**: Use only with clear read/write path separation (RCU preferred for high read concurrency)

### Mutexes
- **Definition**: Sleeping lock (`struct mutex`) for mutual exclusion, simpler than semaphores (count = 1).
- **Purpose**: Replaces semaphore for straightforward mutual exclusion cases, with better performance.
- **Initialization**: `DEFINE_MUTEX(name);` (static) or `mutex_init(&mutex);` (dynamic).
- **Usage**:
  ```c
	mutex_lock(&mutex);
	/* critical region */
	mutex_unlock(&mutex);
  ```
  - **Constraints**:
	  - Only one holder (count = 1).
	  - Same task must lock and unlock (no cross-context unlocking).
	  - No recursive lock/unlock.
	  - No exit while holding mutex.
	  - Not for interrupt/bottom half (even with `mutex_trylock`).
	  - Use official API only (no manual initialization/copy).
  - **Debugging**: `CONFIG_DEBUG_MUTEXES` checks constraint violations (Enhanced with `lockdep` in modern kernels)
  - **Semaphores vs. Mutexes**:
	  - Prefer mutex unless semaphore’s flexibility (e.g., user-space sync) is needed.
	  - Mutex is simpler, more efficient, with stricter rules.
  - **Spin Locks vs. Mutexes**:
	  - Spin lock: Low overhead, short hold, interrupt context.
	  - Mutex: Long hold, sleeping allowed, process context only.
	  - **Real-World Analogy (Choice)**: Spin lock like waiting at a door (busy wait); mutex like signing up for a time slot (sleep).
  - Mutexes are default for process-context locking in modern kernels; RCU or spinlocks for high concurrency.
  - **Mutex Methods**
	  - `mutex_lock(struct mutex *)` - Locks the given mutex; sleeps if the lock is unavailable
	  - `mutex_unlock(struct mutex *)` - Unlocks the given mutex
	  - `mutex_trylock(struct mutex *)` - Tries to acquire the given mutex; returns one if successful and the lock is acquired and zero otherwise
	  - `mutex_is_locked (struct mutex *)` - Returns one if the lock is locked and zero otherwise


### Completion Variables
- **Purpose**: Synchronize two tasks when one signals an event (e.g., task completion) to wake another.
- **Behavior**: One task waits on a completion variable; another signals completion to wake it.
- **Comparison**: Like semaphores but simpler, for specific event-wait scenarios.
- **Type**: `struct completion` in `<linux/completion.h>`.
- **Initialization**: `DECLARE_COMPLETION(name);` (static) or `init_completion(&comp);` (dynamic).
- **Usage**:
	- Wait: `wait_for_completion(&comp);` puts task to sleep until signaled.
	- Signal: `complete(&comp);` wakes all waiting tasks.
- **Use Case**: `vfork()` waits for child to exec/exit (see `kernel/sched.c`, `kernel/fork.c`) (Common in driver initialization).
- **Completion Variable Methods**
	- `init_completion(struct completion *)` - Initializes the given dynamically created completion variable
	- `wait_for_completion(struct completion *)` - Waits for the given completion variable to be signaled 
	- `complete(struct completion *)` - Signals any waiting tasks to wake up
- **Real-World Analogy**: Chef waits for ingredients (completion variable); supplier signals when delivered, waking chef.
- **Note:** Modern kernels use completions for task synchronization (e.g., module loading, device setup).

### BKL: The Big Kernel Lock
- **Definition**: Global spin lock for transitioning Linux from coarse SMP (2.0) to fine-grained locking (2.2+).
- **Properties**:
	- Allows sleeping; lock auto-drops on sleep, reacquired on reschedule (no deadlock).
	- Recursive: Same task can acquire multiple times without deadlock.
	- Process context only; not usable in interrupt context.
	- Disables kernel preemption; no locking on UP kernels.
- **Usage**:
  ```c
	lock_kernel();
	
	/*
	 * Critical section, synchronized against all other BKL users...
	 * Note, you can safely sleep here and the lock will be transparently
	 * released. When you reschedule, the lock will be transparently
	 * reacquired. This implies you will not deadlock, but you still do
	 * not want to sleep if you need the lock to protect data here!
	 */
	 
	 unlock_kernel();
  ```
  - **BKL Methods**
	  - `lock_kernel ()` - Acquires the BKL.
	  - `unlock_ kernel()` - Releases the BKL.
	  - `kernel_ locked()` - Returns nonzero if the lock is held and zero otherwise. (UP always returns nonzero.)
  - **Issues**: Unclear what it protects (code vs. data); hard to replace with fine-grained locks.
  - **Status**: Discouraged; new code must avoid BKL (Removed in Linux 3.0 (2011); use spinlocks/mutexes instead)

- **Explaining some terminologies**
	- **What is SMP?**
		- Symmetric Multiprocessing (SMP) means multiple processors share the same memory and run the kernel together.
	- **Coarse SMP (Linux 2.0)**:
		- In Linux 2.0, only one processor could run kernel code at a time, using a single big lock (BKL).
		- **Simple Explanation**: Like one key for the entire house; only one worker (processor) could enter, others waited, slowing things down.
	- **Fine-grained locking (Linux 2.2+)**:
		- Starting with Linux 2.2, the kernel used many smaller locks (e.g., spinlocks, mutexes) for specific data, allowing multiple processors to work concurrently.
		- **Simple Explanation**: Like separate keys for each room; multiple workers can work in different rooms at once, improving speed.
	- **What is UP?**
		- Uniprocessor (UP) means a system with only one processor.
		- No need for locks like BKL, as there’s no concurrent access to shared data (no other processor can interfere).
		- **Simple Explanation**: Like a single-person office; no need to lock doors since only one person is there, no one else can mess with your work.

### Sequential Locks
- **Definition**: Seq locks (`seqlock_t`) manage shared data with many readers and few writers, using a sequence counter.
- **Behavior**:
	- Writers: Acquire lock (`write_seqlock`), increment counter (*odd* during write, *even* after).
	- Readers: Check counter before/after read (`read_seqbegin`, `read_seqretry`); retry if counter changes (write occurred).
	- **Initialization**: `seqlock_t lock = DEFINE_SEQLOCK(mr_seq_lock);`
	- **Usage**:
		- Write:
			```c
			write_seqlock(&mr_seq_lock);
			/* write lock is obtained... */
			write_sequnlock(&mr_seq_lock);
		  ```
		- Read:
		    ```c
			do {
				seq = read_seqbegin(&mr_seq_lock);
				/* read data here ... */
			} while (read_seqretry(&mr_seq_lock, seq));
			```
	- **Properties**:
		- Favors writers; readers retry if writers active (unlike reader-writer locks).
		- Lightweight for readers, scalable for many readers.
	- **Requirements**:
		- Many readers, few writers.
		- Writers prioritized (no writer starvation).
		- Simple data (e.g., structures, non-atomic integers).
	- **Use Case**: A prominent user of the seq lock is jiffies, the variable that stores a Linux machine’s uptime. Jiffies holds a 64-bit count of the number of clock ticks since the machine booted. On machines that cannot atomically read the full 64-bit `jiffies_64` variable, `get_jiffies_64()` is implemented using seq locks:
	  ```c
		u64 get_jiffies_64(void)
		{
			unsigned long seq;
			u64 ret;
			do {
			seq = read_seqbegin(&xtime_lock);
			ret = jiffies_64;
			} while (read_seqretry(&xtime_lock, seq));
			return ret;
		}
	  ```
	  Updating jiffies during the timer interrupt, in turns, grabs the write variant of the seq lock:
	  ```c
		write_seqlock(&xtime_lock);
		jiffies_64 += 1;
		write_sequnlock(&xtime_lock);
	  ```
	  - **Real-World Analogy**: Bulletin board; readers check notices, retry if writer updates board; writer posts without waiting for readers.
	  - **Modern Usage**:
		  - Still used for jiffies and timekeeping (e.g., `xtime_lock`).
		  - Less common for new code; RCU often preferred for read-heavy cases due to no reader retries.
		  - Enhanced debugging with `CONFIG_DEBUG_LOCK_ALLOC` for seqlock consistency (`lockdep` validates seqlock usage in modern kernels)

### Preemption Disabling
- **Problem**: Kernel preemption allows a higher-priority task to interrupt a running task, potentially accessing the same per-processor data, causing race conditions even on uniprocessor systems.
- **Why It Happens**:
	- Kernel is preemptive; tasks can be paused (preempted) for others.
	- Per-processor data (unique to each processor) may not need locks for SMP safety but needs protection from preemption within the same processor.
- **Example Issue**:
	- Task A uses per-processor variable foo (no lock, assuming single processor).
	- Task A is preempted; Task B runs, modifies foo.
	- Task A resumes, continues modifying foo, causing corruption.
- **Solution**: Disable preemption to ensure a task completes its work on per-processor data without interruption.
- **Mechanism**:
	- `preempt_disable()` - Disables kernel preemption by incrementing the preemption counter; nestable (call multiple times)
	- `preempt_enable()` - Decrements the preemption counter and checks and services any pending reschedules if the count is now zero
	- `preempt_enable_no_resched()` - Enables kernel preemption but does not check for any pending reschedules
	- `preempt_count()` - Returns the preemption count; 0 means preemptive, ≥1 means non-preemptive.
- **Helper Functions**:
	- `get_cpu()` - Disables preemption, returns current processor number (for per-processor data).
	- `put_cpu()` - Reenables preemption.
	- **Usage Example**:
	  ```c
		int cpu;
		
		/* disable kernel preemption and set “cpu” to the current processor */
		cpu = get_cpu();
		
		/* manipulate per-processor data ... */
		
		/* reenable kernel preemption, “cpu” can change and so is no longer
		 valid */
		put_cpu();
	  ```
  - **Why Not Locks?**
	  - Per-processor data doesn’t need SMP locks (only one processor accesses it), but preemption disabling prevents same-processor task switches.
  - **Real-World Analogy**
	  - Like a chef using a private counter (per-processor data) in a kitchen; locking the door (disabling preemption) ensures no one else (another task) interrupts and messes with the counter while cooking.
  - **Modern Usage**
	  - Preemption disabling still used for per-CPU data (e.g., `per_cpu` variables in `sched/`,` mm/`).
	  - Modern kernels prefer RCU or `local_lock` for per-CPU data to reduce explicit preemption disabling.
	  - `lockdep` validates preemption safety, catching misuse in debugging (Enabled via `CONFIG_PROVE_LOCKING`)
  - #### Clarification on Problem and Solution
	  - **What Problem?**
		  - On a single processor, a task working on per-processor data (e.g., a counter unique to that processor) can be interrupted by a higher-priority task. If both tasks modify the same data, it leads to inconsistent results (a race condition), even without SMP concurrency.
	  - **How Preemption Disabling Helps?**
		  - By calling `preempt_disable()`, the kernel ensures the current task runs uninterrupted on its processor until `preempt_enable()`. This protects per-processor data without needing a full lock, as no other task can run on that processor during this time.
	  - **Why Not Always Use Locks?**
		  - Locks (e.g., spinlocks) protect against multi-processor access but add overhead. For data only one processor uses, disabling preemption is lighter and sufficient.

### Ordering and Barriers
- **What’s the Problem?**
	- **Reordering Issue**: Compiler or processor may change the order of memory reads (loads) or writes (stores) from your code’s sequence for speed, breaking intended order.
	- **When It Happens?**
		- **Compiler**: At compile time, it rearranges instructions (e.g., `a=1; b=2;` may compile as `b=2; a=1;`) if it sees no data dependency (no reliance between `a` and `b`).
		- **Processor**: During execution, modern processors (except x86 for writes) execute instructions out-of-order to optimize pipeline usage (e.g., faster instructions first).
	- **Why It Happens?**
		- **Compiler**: Optimizes code for speed, assuming no external impact (e.g., ignores other threads or hardware).
		- **Processor**: Executes instructions in parallel or out-of-order to maximize performance, unaware of other processors or devices needing specific order.
	- **Example Issue**:
		- Code: `a=3; b=4;` (intended: `a` first, then `b`).
		- Without barriers, another thread/processor might see `b=4` before `a=3`, causing incorrect data access (e.g., device expects `a` set first).
- **Real-World Analogy**:
		- Writing a recipe (code); compiler/processor is a chef who might mix steps (e.g., “add sugar” before “mix flour”) to save time, confusing helpers (other threads/devices) who need steps in order.
- **Solution**: Barriers enforce the intended order of reads/writes for correct data visibility across threads, processors, or hardware.

- **How Barriers Work**:
	- **Compiler Barriers**: Tell compiler not to reorder instructions in generated code (affects compile-time order).
	- **Processor Barriers**: Force processor to complete reads/writes in code order, ensuring other processors/devices see correct sequence.
- **Barrier Types**:
	- **Compiler-Specific**:
		- `barrier()`: Stops compiler from reordering loads/stores across the call (lightweight, no processor impact).
		- **How**: Ensures compiled code follows source order (e.g., `a=1; barrier(); b=2;` keeps `a=1` before `b=2` in object code).
	- **Processor-Specific**:
		- `rmb()`-  Read memory barrier; ensures no loads reorder across it
		  (e.g., `read a; rmb(); read b;` ensures `a` read before `b`).
		- `wmb()` -  Write memory barrier; ensures no stores reorder
		  (e.g., `write a; wmb(); write b;` ensures `a` written before `b`).
			- No-op on x86 (writes not reordered).
		- `mb()` - Read and write barrier; ensures no loads/stores reorder (combines `rmb` and `wmb`).
		- `read_barrier_depends()` - Read barrier for dependent loads
		  (e.g., `read p; read *p;`; ensures `p` read before `*p`).
			- Faster than `rmb` on some architectures.
	- **SMP Variants**:
		- `smp_rmb()`, `smp_wmb()`, `smp_mb()`, `smp_read_barrier_depends()` act as barriers on SMP, only `barrier()` on UP (optimizes for single processor).

- **Which Barrier for What?**
	- **Compiler Optimization**: Use `barrier()` to stop compile-time reordering (e.g., ensure code order for interrupt handlers).
	- **Processor Optimization**: Use `rmb`, `wmb`, `mb`, or `read_barrier_depends` to stop runtime reordering (e.g., for SMP or device communication).
	- **Example**: Assume the initial value of a is 1 and the initial value of b is two.
	  ![[Pasted image 20250720115647.png]]
		- Without barriers, `c` could equal four (what you’d expect), yet `d` could equal one (not what you’d expect)
		- Using the `mb()` ensured that `a` and `b` were written in the intended order, whereas the `rmb()` insured c and d were read in the intended order.
	- **Dependent Read(Load) Example**:
	  ![[Pasted image 20250720120144.png]]
		- Without memory barriers, it would be possible for `b` to be set to `pp` before `pp` was set to `p`.
		- `read_barrier_depends()` ensures `p` is read into `pp` before dereferencing `pp` to read `*pp` (dependent load).
		- It would also be sufficient to use `rmb()` here, but because the reads are data dependent, we can use the potentially faster `read_barrier_depends()`.

- **Use Case**: Device drivers (e.g., ordered register writes), SMP data sharing 
  (e.g., `jiffies`).
- **Modern Usage**
	- Used in drivers (e.g., `drivers/pci/`), memory management (e.g., `mm/`).
	- RCU uses `smp_mb()` for pointer updates.
	- `READ_ONCE()`, `WRITE_ONCE()` with barriers for volatile data (Standard in 2025 kernels)
	- `lockdep` validates barriers via `CONFIG_PROVE_LOCKING`.


## **Quick Recall**
- **Q: Why are atomic operations used in the kernel?**
    - **A**: Ensure indivisible updates to counters or flags (e.g., `atomic_inc`) without locks, preventing race conditions.
- **Q: What is the main difference between spin locks and semaphores?**
	- **A**: Spin locks busy-wait (short holds, interrupt-safe); semaphores sleep (long holds, process context only).
- **Q: When are reader-writer locks useful?**
	- **A**: For data with many readers and few writers, allowing concurrent reads but exclusive writes.
- **Q: How do mutexes differ from semaphores?**
	- **A**: Mutexes are simpler, `count=1`, stricter rules (no recursive locks, same context), better for mutual exclusion.
- **Q: What do completion variables do?**
	- **A**: Synchronize tasks; one waits (wait_for_completion) until another signals (complete) an event.
- **Q: Why was the Big Kernel Lock (BKL) introduced?**
	- **A**: Eased transition from coarse (single lock) to fine-grained locking in SMP systems; now obsolete.
- **Q: When are sequential locks (seqlocks) preferred?**
	- **A**: For many readers, few writers, prioritizing writers, with simple data (e.g., jiffies_64).
- **Q: Why disable kernel preemption?**
	- **A**: Protects per-processor data from same-processor task switches without needing SMP locks.
- **Q: What problem do memory barriers solve?**
	- **A**: Prevent compiler/processor reordering of reads/writes, ensuring correct order for SMP or hardware.
- **Q: How does RCU improve read-heavy synchronization?**
	- **A**: Allows lock-free reads, writers update via copy; faster than reader-writer locks (Common in 2025 kernels).

## **Hands-on Ideas**
- **Atomic Counter Test**
	- Write a kernel module with a global `atomic_t` counter.  
	- Create two kernel threads incrementing counter without locks; observe inconsistent results.    
	- Use `atomic_inc` and `atomic_read` to ensure correct increments.
	- **Learn**: Importance of atomic operations to prevent race conditions without locks.
- **Spinlock List Protection**
	- Implement a module with a shared linked list and kernel threads adding/removing items.
	- Access list without locks; observe data corruption.
	- Protect list with `spin_lock_irqsave` and test in interrupt context.
	- **Learn**: Spinlocks for short, interrupt-safe critical sections.
- **Reader-Writer Lock Benchmark**
	- Modify list module to use `rwlock_t` for read-heavy access (e.g., multiple readers, one writer).
	- Simulate frequent reads, rare writes; measure performance with `read_lock`/`write_lock`.
	- Enable `CONFIG_DEBUG_SPINLOCK` to catch misuse.
	- **Learn**: Reader-writer locks for concurrent reads, risks of writer starvation (Compare with RCU).
- **Semaphore Resource Limit**
	- Create a module with a semaphore-protected buffer (`count=1` for mutex).
	- Spawn kernel threads competing for buffer access using `down_interruptible`.
	- Test signal handling (e.g., `-EINTR` return); observe sleep behavior.
	- **Learn**: Semaphores for long holds and sleeping in process context.
- **Mutex Process Context Test**
	- Replace semaphore in buffer module with `struct mutex`.
	- Attempt invalid operations (e.g., recursive lock, unlock from different context).
	- Enable `CONFIG_DEBUG_MUTEXES` to verify strict rules.
	- **Learn**: Mutex simplicity and constraints for mutual exclusion.
- **Completion Variable Sync**
	- Write a module where one thread initializes a data structure, signals via `complete`.
	- Another thread waits using `wait_for_completion` (mimic `vfork` behavior).
	- Test in `kernel/fork.c` style; verify correct wake-up.
	- **Learn**: Task synchronization with completion variables.
- **Seqlock Jiffies Emulation**
	- Implement a module with a 64-bit counter protected by `seqlock_t`, mimicking `jiffies_64`.
	- Use `write_seqlock` in one thread, `read_seqbegin`/`read_seqretry` in others; test retries.
	- Measure reader retry frequency under frequent writes.
	- **Learn**: Seqlocks for read-heavy, writer-prioritized data.
- **Preemption Disable Data Protection**
	- Create a module with a `per_cpu` variable accessed by kernel threads.
	- Use `get_cpu`/`put_cpu` to disable preemption; test without to observe race conditions.
	- Check `preempt_count` with `CONFIG_PROVE_LOCKING`.
	- **Learn:** Preemption disabling for per-processor data safety.
- **Barrier Ordering Test**
	- Write a module with two kernel threads sharing variables (e.g., `a`, `b`).
	- Test without barriers (`mb`, `rmb`); observe incorrect read/write orders.
	- Add `mb` in writer, `rmb` in reader; verify correct order (Use `READ_ONCE`/`WRITE_ONCE` as per modern kernel).
	- **Learn**: Barriers ensure proper memory ordering for SMP/hardware.
- **RCU List Access**
	- Modify list module to use RCU instead of reader-writer locks for read-heavy access.
	- Implement `rcu_read_lock`, `rcu_assign_pointer`, `synchronize_rcu` for safe updates.
	- Compare performance with `rwlock_t` (Test with `lockdep`)
	- **Learn**: RCU’s efficiency for read-heavy synchronization in modern kernels.