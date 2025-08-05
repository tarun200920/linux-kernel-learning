# Chapter 11: [Timers and Time Management]

## **Summary**

### Introduction
- **Purpose**: Kernel manages time for time-driven functions (e.g., scheduler balancing, screen refresh).
- **Time Types**:
	- **Relative**: Events scheduled in the future (e.g., “run in 5 seconds”).
	- **Absolute**: Current time of day (e.g., wall time for user-space).
- **Mechanisms**:
	- **System Timer**: Hardware-driven, issues interrupts at fixed frequency (*tick rate*) for periodic tasks.
	- **Dynamic Timers**: Kernel schedules one-time events after a delay (e.g., floppy drive motor shutdown).
- **Key Components**: Timer interrupt (handles periodic tasks) and dynamic timers (for delayed events).
- **Real-World Analogy**: Like a kitchen clock (system timer) ticking regularly to check oven tasks, and a timer (dynamic) set to remind you to turn off the stove later.
- **Modern Usage**: 
	- High-resolution timers (HRT) enhance precision beyond tick rate (Common in 2025 kernels for real-time tasks).
	- Tickless kernels reduce interrupts on idle systems for power efficiency.

### Kernel Notion of Time
- **System Timer**: Hardware (e.g., digital clock, processor frequency) generates interrupts at a fixed tick rate.
- **Tick and Tick Rate**
	- The system timer goes off (often called *hitting* or *popping*) at a preprogrammed frequency, called the *tick rate*.
	- When the system timer goes off, it issues an interrupt that the kernel handles via a special interrupt handler.
	- Because the kernel knows the preprogrammed tick rate, it knows the time between any two successive timer interrupts. This period is called a tick and is equal to
	  `1/(tick rate)` seconds.
- **Timer Interrupt**: Handles tasks on each tick:
	- Updates system uptime and wall time.
	- Balances scheduler runqueues on SMP (see Chapter 4).
	- Runs expired dynamic timers.
	- Updates resource usage and processor time stats.
- **Wall Time**: Absolute time of day; provided to user-space via system calls.
- **Uptime**: Relative time since boot; used by kernel and user-space for time differences.
- **Frequency**: Some tasks run every tick, others every `n` ticks (fraction of tick rate).
- **Modern Usage**:
	- `jiffies_64` and `xtime_lock` (seqlock) still used for timekeeping (see `kernel/time/timekeeping.c`).
	- Tickless kernels use dynamic ticks, adjusting interrupt frequency for efficiency.

### The Tick Rate: HZ
- **Definition**: Tick rate (HZ) is the frequency of the system timer interrupt, set at boot via `<asm/param.h>`.
- **Tick**: Time between interrupts (1/HZ seconds); e.g., HZ=100 → 10 ms, HZ=1000 → 1 ms.
- **Variation**: HZ varies by architecture (e.g., x86 defaults to 100); never assume a fixed value in code.
- **Importance**: Drives kernel’s timekeeping, periodic tasks; impacts resolution and overhead.
- **Real-World Analogy**: Like a metronome’s tempo; faster ticks (higher HZ) give precise beats but tire the musician (processor).
  (A metronome is a device that provides a steady, repeating beat to help musicians and others practice and maintain a consistent tempo)
- **Modern Usage**: 
	- Common HZ values: 100, 250, 1000; configurable via `CONFIG_HZ`.
	- Tickless kernels (dynamic ticks) dominate for power efficiency (Standard in 2025).
- #### The Ideal HZ Value
	- **Background**: x86 historically used HZ=100 (10 ms); raised to 1000 in 2.5, now configurable (e.g., 100, 500, 1000).
	- **Impact**: Higher HZ increases timer interrupt frequency, affecting resolution and accuracy.
	- **Resolution**: HZ=100 → 10 ms granularity; HZ=1000 → 1 ms (finer timing for events).
	- **Accuracy**: Average error is half the tick period (e.g., ±5 ms for HZ=100, ±0.5 ms for HZ=1000).
	- **Real-World Analogy**: Like a clock’s second hand; finer ticks (seconds vs. minutes) give precise timing but require more effort.
	- **Modern Usage**:
		- HZ=250 or 1000 common for desktops; servers may use 100 for lower overhead.
		- High-resolution timers (HRT) bypass HZ for sub-millisecond precision.
- #### Advantages with a Larger HZ
	- **Benefits**:
		- Finer timer resolution (e.g., 1 ms vs. 10 ms for scheduling, timeouts).
		- Improved accuracy for system calls like `poll()`/`select()` (less wait time).
		- More precise resource usage and uptime tracking.
		- Better process preemption (e.g., 1 ms vs. 10 ms scheduling latency).
	- **Example**: With HZ=100, a 2 ms timeslice may overrun by 10 ms; HZ=1000 reduces this to 1 ms.
	- **Real-World Analogy**: Like a stopwatch vs. a wall clock; stopwatch (higher HZ) tracks short races better.
	- **Modern Usage**:
		- Higher HZ benefits real-time tasks (e.g., audio processing).
		- HRT and dynamic timers reduce dependency on high HZ for precision.
- #### Disadvantages with a Larger HZ
	- **Drawbacks**:
		- More frequent timer interrupts increase processor overhead.
		- Higher cache thrashing and power consumption.
	- **Impact**: HZ=1000 has 10x overhead vs. HZ=100, but modern systems handle it well.
	- **Modern Usage**:
		- Overhead manageable on modern hardware; HZ=1000 standard for desktops.
		- Tickless kernels mitigate overhead by skipping interrupts when idle.
- #### A Tickless OS
	- **Concept**: `CONFIG_NO_HZ` enables dynamic timer interrupts based on pending timers, not fixed HZ.
	- **Behavior**: Interrupts fire only when needed (e.g., 3 ms for next timer), not every
	  1 ms.
	- **Benefits**: Reduces overhead and power consumption, especially during idle periods.
	- **Example**: Idle system skips interrupts for 200 ms or more, saving power.
	- **Real-World Analogy**: Like a smart alarm; only rings when needed (e.g., for a meeting) instead of every minute.
	- **Modern Usage**:
		- Tickless operation (`CONFIG_NO_HZ_FULL`) standard in 2025 for servers, laptops.
		- Enhances power efficiency in IoT, mobile devices; see `kernel/time/tick-sched.c`.

### Jiffies
- **Introduction**:
	- The global variable `jiffies` holds the number of ticks that have occurred since the system booted.
	- On boot, the kernel initializes the variable to zero, and it is incremented by one during each timer interrupt.
	- Thus, because there are `HZ` timer interrupts in a second, there are `HZ` jiffies in a second.
	- The system uptime is therefore `jiffies/HZ` seconds.
- **Usage**: Measures relative time; e.g., (`seconds * HZ`) converts seconds to `jiffies`,
  `(jiffies / HZ)` converts `jiffies` to seconds.
  *Note:* The former, converting from seconds to ticks, is more common.
- **Declaration**: The `jiffies` variable is declared in `<linux/jiffies.h>` as
  ```c
	extern unsigned long volatile jiffies;
  ```
  *Note*: that the `jiffies` variable is prototyped as `unsigned long` and that storing it in anything else is incorrect.
- **Initialization**: The kernel initializes `jiffies` to a special initial value, causing the variable to overflow more often, catching bugs. When the actual value of `jiffies` is sought, this “offset” is first subtracted.
- **Example**: Code often needs to set a value for some time in the future, for example:
  ```c
	unsigned long time_stamp = jiffies;        /* now */
	unsigned long next_tick = jiffies + 1;     /* one tick from now */
	unsigned long later = jiffies + 5*HZ;      /* five seconds from now */
	unsigned long fraction = jiffies + HZ / 10;/* a tenth of a second from now */
  ```
- **Real-World Analogy**: Like a car’s odometer counting engine ticks; tracks runtime, not clock time.
- **Modern Usage**: 
	- `jiffies_64` used for high-precision timekeeping; see `kernel/time/timekeeping.c`.
	- High-resolution timers (HRT) reduce reliance on jiffies for fine-grained tasks.

- #### The Etymology of the Jiffy
	- **Etymology Meaning**: Etymology is the study of a word’s origin and history.
	- **Jiffy Origin**: Unknown; used since 18th-century England for a brief time.
	- **Meanings**:
		- General: Short, indeterminate time (e.g., “in a jiffy”).
		- Science: `10ms` or time for light to travel a distance (e.g., a foot).
		- Computing: Time between clock cycles (e.g., `1/60s` in US electrical systems).
		- Unix/Linux: Time between timer ticks (e.g., `10ms` for `HZ=100`).
	- **Real-World Analogy**: Like “moment” meaning different durations (a second, a minute) depending on context.
	- **Modern Usage**
		- Jiffy remains Linux-specific term for tick period (e.g., `1ms` with `HZ=1000`).
		- Less used in user-space due to high-resolution APIs (e.g., `CLOCK_MONOTONIC`).

- #### Internal Representation of Jiffies
	- **Structure**: The jiffies variable has always been an unsigned long, and therefore
		- 32 bits in size on 32-bit architectures, and
		- 64-bits on 64-bit architectures
	- **32-bit jiffies**: With a tick rate of 100, a 32-bit jiffies variable would overflow in about 497 days.With HZ increased to 1000, however, that overflow now occurs in just 49.7 days!
	- **64-bits jiffies**: If jiffies were stored in a 64-bit variable on all architectures, then for any reasonable `HZ` value the `jiffies` variable would never overflow in anyone’s lifetime.
	- **Overlay**:
		- `jiffies` is defined as an unsigned long:
		  ```c
			extern unsigned long volatile jiffies;
		  ```
		- A second variable, `jiffies_64`, is also defined in `<linux/jiffies.h>`:
		  ```c
			extern u64 jiffies_64;
		  ```
		- The `ld(1)` linker script used to link the main kernel image (`arch/x86/kernel/vmlinux.lds.S` on x86) then *overlays* the `jiffies` variable over the start of the `jiffies_64` variable:
		  ```c
			jiffies = jiffies_64;
		  ```
		-  Thus, `jiffies` is the lower 32 bits of the full 64-bit `jiffies_64` variable.
		  ![[Pasted image 20250726072515.png]]
	- **Access**:
		- Code that accesses `jiffies` simply reads the lower 32 bits of `jiffies_64`. The function `get_jiffies_64()` can be used to read the full 64-bit value. Such a need is rare; consequently, most code simply continues to read the lower 32 bits directly via the `jiffies` variable.
		- **Note:** A special function `get_jiffies_64()` is needed because 32-bit architectures cannot atomically access both 32-bit words in a 64-bit value. The special function locks the jiffies count via the `xtime_lock` lock before reading.
		- On 64-bit architectures, `jiffies_64` and `jiffies` refer to the same thing. Code can either read `jiffies` or call `get_jiffies_64()` as both actions have the same effect.
	- **Purpose**: Maintains 32-bit compatibility while preventing 64-bit overflow.
	- **Real-World Analogy**: Like a car’s trip meter (`jiffies`) showing part of a full odometer (`jiffies_64`).
	- **Modern Usage**
		- `jiffies_64` critical for long-running systems; accessed via `get_jiffies_64()` in `kernel/time/jiffies.c`.
		- 64-bit architectures use `jiffies_64` directly, simplifying code.

- #### Jiffies Wraparound
	- **Issue**: 32-bit `jiffies` overflows after `2^32` ticks (e.g., ~497 days at HZ=100, ~49.7 days at HZ=1000).
	- **Problem**: Comparisons fail post-wraparound (e.g., `jiffies < timeout` incorrect if `jiffies` wraps to 0).
	- **Solution**: Macros in `<linux/jiffies.h>` handle wraparound:
	  ```c
		#define time_after(unknown, known) ((long)(known) - (long)(unknown) < 0)
		#define time_before(unknown, known) ((long)(unknown) - (long)(known) < 0)
		#define time_after_eq(unknown, known) ((long)(unknown) - (long)(known) >= 0)
		#define time_before_eq(unknown, known) ((long)(known) - (long)(unknown) >= 0)
	  ```
		- The `unknown` parameter is typically `jiffies` and the `known` parameter is the value against which you want to compare.
		- `time_after(unknown, known)` - returns `true` if time unknown is after time known; otherwise, it returns `false`.
		- `time_before(unknown, known)`  - returns `true` if time unknown is before time known; otherwise, it returns `false`.
		- `time_after_eq(unknown, known)` and `time_before_eq(unknown, known)` - perform identically to the first two, except they also return `true` if the parameters are equal.
	- **Example**:
	  ```c
		unsigned long timeout = jiffies + HZ/2; /* timeout in 0.5s */
		
		/* do some work ... */
		if (time_before(jiffies, timeout)) {
			/* we did not time out, good ... */
		} else {
			/* we timed out, error ... */
		}
	  ```
	- **Real-World Analogy**: Like a 4-digit odometer rolling over to 0000; special math ensures correct distance checks.
	- **Modern Usage**
		- Wraparound macros widely used (e.g., `net/core/dev.c` for timeouts).
		- 64-bit `jiffies_64` minimizes overflow issues on modern systems.

- #### User-Space and HZ
	- **Purpose**: Kernel provides time values (e.g., system uptime, elapsed time) to user-space via interfaces like `/proc/uptime` or system calls (e.g., `gettimeofday`).
	- **Role of jiffies**: `jiffies` counts kernel timer ticks at `HZ` ticks/second (e.g., 1000 ticks/second for HZ=1000); user-space reads derived time, not raw jiffies.
	- **Historical Context**: Early Linux on x86 used `HZ=100` (`100 ticks/second`, `10ms/tick`); legacy user-space apps (e.g., `uptime`) assumed this rate for time calculations.
	- **Issue**:
		- Changing `HZ` affects user-space (e.g., uptime misreported if HZ ≠ 100).
		- If kernel `HZ` differs from 100 (e.g., HZ=1000), unscaled jiffies would cause errors in legacy apps (e.g., 1000 ticks misread as 10 seconds instead of 1).
	- **Solution**: `USER_HZ` (e.g., 100 on x86) defines user-space tick rate; kernel scales via `jiffies_to_clock_t`.
	- **USER_HZ**: Kernel macro (e.g., 100 on x86) defining the tick rate user-space expects, ensuring compatibility with legacy apps assuming HZ=100.
	- **Scaling**:
		- Kernel converts `jiffies` (based on `HZ`) to `USER_HZ` using `jiffies_to_clock_t()` to provide correct time to user-space.
		- The function `jiffies_64_to_clock_t()` is provided to convert a 64-bit jiffies value from `HZ` to `USER_HZ` units.
			- **Example**: For `HZ=1000`, 1000 jiffies = 1 second; scaled to `USER_HZ=100`, it is 100 ticks = 1 second (divide by 1000/100 = 10).
			- **Formula** (simple case): `jiffies / (HZ / USER_HZ)`.
	- **Why Scaling**: Ensures user-space sees consistent time (e.g., 5 seconds as 5 seconds) regardless of kernel HZ, preventing errors in legacy interfaces.
	- **User-Space Time Usage**: Apps read time via:
		- `/proc/uptime` (uptime in seconds, scaled to `USER_HZ`).
		- System calls like `gettimeofday` (wall time) or `clock_gettime` (monotonic time).
		- Legacy apps rely on scaled jiffies; modern apps use HZ-independent APIs.
	- **Why USER_HZ Exists**: Maintains compatibility with older apps expecting HZ=100 on x86, avoiding misinterpretation of time when kernel HZ changes (e.g., to 250 or 1000).
	- **Modern Approach**: User-space apps can use `clock_gettime(CLOCK_MONOTONIC)` for nanosecond-precision, HZ-independent timing, bypassing `USER_HZ` and `jiffies`.
	- **Example**:
	  ```c
		unsigned long start;
		unsigned long total_time;
		
		start = jiffies;
		/* do some work ... */
		total_time = jiffies - start;
		printk(“That took %lu ticks\n”, jiffies_to_clock_t(total_time));
	  ```
		- User-space expects the previous value as if `HZ=USER_HZ`. If they are not equivalent, the macro scales as needed and everyone is happy.
		- Of course, this example is silly: It would make more sense to print the message in seconds, not ticks. For example:
		  ```c
			printk(“That took %lu seconds\n”, total_time / HZ);
		  ```
	- **Real-World Analogy**: Like a car’s dashboard showing speed in mph (USER_HZ=100) for a US driver (user-space), even if the engine measures in km/h (kernel HZ); the kernel converts to match the driver’s expectation.
	- **Modern Usage**
		- `jiffies_to_clock_t()` used in `/proc/uptime`, `/proc/stat` (see `kernel/time/time.c`).
		- Modern apps prefer `clock_gettime(CLOCK_MONOTONIC)` for HZ-independent timing (Standard in 2025).
		- Non-x86 architectures may use different `USER_HZ` (e.g., 1024 on some ARM), but scaling ensures consistency.
		- Tickless kernels reduce jiffies dependency, but scaling persists for legacy interfaces.

### Hardware Clocks and Timers
- **Purpose**: Two hardware devices, real-time clock (RTC) and system timer, help kernel manage time.
- **Devices**: RTC for persistent wall time; system timer for periodic interrupts.
- **Variation**: Implementation differs by architecture (e.g., x86 vs. ARM), but purpose is similar.
- **Real-World Analogy**: Like a wall clock (RTC) showing the date/time and a metronome (system timer) ticking to keep tasks on rhythm.

- #### Real-Time Clock
	- **Function**: Nonvolatile device storing wall time (actual date/time), powered by a battery to run when system is off.
	- **Usage**:
		- On boot, kernel reads RTC to set `xtime` (wall time variable).
		- Rarely read again; some architectures (e.g., x86) save wall time back to RTC periodically.
		- Kernel reads/writes RTC via low-level drivers (e.g., `drivers/rtc/rtc-cmos.c`).
	- **Interface**: Interacts with processor via I/O ports or memory-mapped registers (e.g., x86 uses CMOS I/O ports `0x70–0x71`).
	- **Key Role**: Initializes system time at boot; persists time across power cycles.
	- **Real-World Analogy**: Like a battery-powered wall clock; tells you the time when you wake up (boot).
	- **Modern Usage**:
		- RTC still used for boot-time initialization (see `drivers/rtc/`).
		- Modern systems sync with NTP, reducing RTC reliance post-boot.

- #### System Timer
	- **Function**: Hardware generating periodic interrupts at `HZ` frequency to drive kernel tasks.
	- **Implementations**:
		- **Oscillator**: Programmable clock (e.g., x86 PIT) triggers interrupts at set rate.
		- **Decrementer**: Counter decrements to zero, triggering interrupt (used in some architectures).
	- **Interface**: Programmed via I/O ports (PIT) or processor registers (APIC/TSC).
	- **x86 Specific**:
		- **PIT**: Programmable Interrupt Timer, drives interrupt 0 at `HZ` (set on boot).
		- Other sources: Local APIC timer (per-processor), TSC (processor time stamp counter).
	- **Role**: Drives timer interrupt for `jiffies`, scheduler, and dynamic timers.
	- **Real-World Analogy**: Like a metronome ticking every second to keep a band (kernel) in sync.
	- **Modern Usage**:
		- PIT less common; modern x86 uses APIC or TSC for higher precision (see `arch/x86/kernel/time.c`).
		- Tickless kernels (`CONFIG_NO_HZ`) use dynamic timers, reducing fixed interrupts.

- **Why can’t we use just one hardware?**:
	- RTC can’t generate frequent interrupts (too slow, designed for persistent storage); system timer can’t store time when powered off (no battery). Both are needed for distinct roles: RTC for initial wall time, system timer for runtime tasks.
- **Modern Relevance**: High-resolution timers and tickless kernels reduce reliance on traditional system timers, but RTC remains critical for boot-time setup.


### The Timer Interrupt Handler
- **Purpose**: Handles system timer interrupt (every `1/HZ` seconds, e.g., `10ms` for `HZ=100`) to update time and perform periodic kernel tasks.
- **Structure**: Split into architecture-dependent (handles hardware) and independent (`tick_periodic()`, core logic) routines.
- **Frequency**: Runs 100–1000 times/second on x86 (HZ=100 or 1000).
- **Real-World Analogy**: Like a kitchen timer buzzing every minute to remind a chef (handler) to check cooking tasks (kernel tasks).

- #### Architecture-Dependent Routine
	- **Tasks**:
		- **Acquire `xtime_lock`**: Protects `jiffies_64` and `xtime` (wall time) from concurrent access. This ensures safe updates in SMP systems where multiple processors access time variables.
		- **Acknowledge/Reset Timer**: Clears interrupt or resets hardware (e.g., PIT) for next tick. This prevents missed or duplicate interrupts, keeping timer consistent.
		- **Save Wall Time to RTC**: Periodically writes `xtime` to RTC (on some architectures, e.g., x86). This keeps RTC updated for accuracy across reboots, though rarely needed post-boot.
		- **Call `tick_periodic()`**: Invokes architecture-independent routine for core tasks. This function separates hardware-specific code from generic timekeeping for portability.
	- **Real-World Analogy**: Like a chef setting up the oven timer (hardware) before starting cooking tasks (`tick_periodic`).
	- **Modern Usage**:
		- `xtime_lock` replaced by `timekeeper_seq` in modern kernels for finer locking (see `kernel/time/timekeeping.c`).
		- APIC/TSC preferred over PIT for interrupts.

- #### Architecture-Independent Routine (tick_periodic)
	- **Tasks (via `do_timer` and `update_process_times`)**:
		- **Increment `jiffies_64`**: Adds 1 to `jiffies_64` per tick (protected by `xtime_lock`). This tracks system uptime and relative time; basis for kernel timing (e.g., `jiffies`).
		- **Update Process Times**: Tracks user/system time for running process via `account_process_tick`. This helps monitoring resource usage (e.g., for `top`); this credits tick to user or kernel mode based on interrupt context.
		- **Run Expired Timers**: Schedules softirq, `run_local_timers()`, to execute dynamic timers. This ensures timely execution of delayed tasks (e.g., driver timeouts) without clogging interrupt handler.
		- **Update Scheduler**: Calls `scheduler_tick()` to decrement process timeslice, set `need_resched`, balance SMP runqueues. This helps in maintaining fair scheduling, reduces latency (see Chapter 4).
		- **Update Wall Time**: Adjusts `xtime` via `update_wall_time()` to reflect elapsed ticks. This keeps accurate date/time for user-space (e.g., `date` command).
		- **Calculate Load Average**: Updates system load stats via `calc_global_load()`. This provides performance metrics (e.g., for `uptime`, `top`).
	- **Key Functions**:
		- `do_timer()`: Increments `jiffies_64`, updates wall time, load average.
		- `update_process_times()`: Updates process stats, timers, scheduler, RCU, POSIX timers.
		- `account_process_tick()`: Credits tick to user/system/idle time based on mode.
	- **Real-World Analogy**: Like a chef checking the clock (jiffies), tracking cooking time (process stats), and starting new dishes (timers) each timer buzz.

- **Why split into architecture-dependent and independent parts?**
	- Architecture-dependent handles hardware-specific tasks (e.g., PIT programming); Architecture-independent handles portable logic (e.g., jiffies update).
	- Ensures code reusability across architectures (x86, ARM) while managing hardware differences.
- **Why update jiffies_64?**
	- `jiffies_64` tracks ticks since boot, driving kernel timing (e.g., timeouts, uptime).
	- Reliable time base for relative timing, protected by `xtime_lock` for SMP safety.
- **Why is process accounting imprecise?**
	- Credits entire tick to mode (user/kernel) at interrupt time, ignoring mode switches or other processes.
	- Simplifies accounting; higher HZ (e.g., 1000) reduces error but can’t track sub-tick changes.
	- **Imprecision Trade-off**: Higher HZ improves accuracy (e.g., `1ms` vs. `10ms`) but increases overhead.
- **Real-World Analogy**: Like a kitchen timer buzzing every minute, prompting a chef (handler) to update the recipe log (jiffies), track cooking progress (process times), and start new tasks (timers).
- **Modern Usage**:
	- Tickless kernels (`CONFIG_NO_HZ`) reduce interrupt frequency when idle, but handler logic remains similar.
	- High-resolution timers (HRT) enhance timer precision beyond HZ (see `kernel/time/hrtimer.c`).
	- `scheduler_tick()` optimized for SMP scalability (See `kernel/sched/core.c`).


### The Time of Day
- The current time of day (the wall time) is defined in `kernel/time/timekeeping.c`:
  ```c
	struct timespec xtime;
  ```
- The `timespec` data structure is defined in `<linux/time.h>` as:
  ```c
	struct timespec {
	__kernel_time_t tv_sec;    /* seconds */
	long tv_nsec;              /* nanoseconds */
};
  ```
- `xtime.tv_sec` - Seconds since epoch (January 1, 1970, UTC).
- `xtime.tv_nsec` - Nanoseconds within the last second.
- **Access**:
	- Protected by `xtime_lock` (seqlock, not spinlock; see Chapter 10).
	- **Write**: Use `write_seqlock(&xtime_lock)` and `write_sequnlock` to update `xtime`.
	  ```c
		write_seqlock(&xtime_lock);
		
		/* update xtime ... */
		
		write_sequnlock(&xtime_lock);
	  ```
	- **Read**: Use `read_seqbegin(&xtime_lock)` and `read_seqretry` loop to ensure no concurrent writes.
	  ```c
		unsigned long seq;
		
		do {
			unsigned long lost;
			seq = read_seqbegin(&xtime_lock);
			
			usec = timer->get_offset();
			lost = jiffies - wall_jiffies;
			if (lost)
				usec += lost * (1000000 / HZ);
			sec = xtime.tv_sec;
			usec += (xtime.tv_nsec / 1000);
		} while (read_seqretry(&xtime_lock, seq));
	  ```
- **User-Space Interface**:
	- Apps like `ls` (file timestamps), `cron` (scheduling), and clocks rely on `gettimeofday()` or time.
	- `gettimeofday()` (via `sys_gettimeofday()`): Returns wall time (`tv`) and timezone (`tz`).
		- Calls `do_gettimeofday()` to read `xtime` safely; copies to user-space or returns `-EFAULT` on error.
		- `do_gettimeofday()` - Adjusts `xtime` with jiffies delta `(jiffies - wall_jiffies`) for precision, converts nanoseconds to microseconds for user-space.
	- `settimeofday()`: Sets `xtime` to user-provided value; requires `CAP_SYS_TIME`.
	- `time()`: Older system call, often emulated via `gettimeofday()` in C library.
	- C library: Provides `ftime()`, `ctime()` for wall time.
- **Kernel Usage**: Less frequent than user-space; mainly used in filesystem code for inode timestamps (e.g., file access/modification times).
- **Real-World Analogy**: Like a wall clock in a house (`xtime`) showing the current date/time, checked often by family (user-space) but rarely by the house manager (kernel) who uses a stopwatch (jiffies) for tasks.

- **Why use a seqlock for `xtime`?**
	- Seqlock allows multiple readers with low overhead, retrying if a writer (e.g., timer interrupt) updates `xtime`.
	- Efficient for frequent reads (user-space time queries) and rare writes (timer updates); see Chapter 10.
- **Why does user-space need wall time more?**
	- User-space apps (e.g., `date`, file managers) need absolute time for display, logs, and file timestamps; kernel focuses on relative time (`jiffies`) for scheduling/timers.

- **Modern Usage**
	- `xtime` replaced by `timekeeper` struct in modern kernels for high-resolution timekeeping (see `kernel/time/timekeeping.c`) but core concepts remain.
	- `clock_gettime(CLOCK_REALTIME)` preferred over `gettimeofday()` for nanosecond precision.
	- Filesystem timestamps use VFS layer with high-resolution clocks (e.g., `struct timespec64`).

### Timers
- **Purpose**: Kernel timers (dynamic timers) delay function execution by a specified time (in jiffies), unlike bottom halves which defer work vaguely (see Chapter 8 [[Ch08_Bottom_Halves_and_Deferring_Work]]).
- **Characteristics**: Non-cyclic (destroyed after expiration), dynamically created, no limit on number.
- **Use Case**: Common in kernel (e.g., driver timeouts, like floppy drive motor shutdown).
- **Real-World Analogy**: Like setting a kitchen reminder (timer) to check food after 5 minutes, then removing it once it rings.

- #### Using Timers
	- **Structure**: Timers are represented by `struct timer_list`, which is defined in `<linux/timer.h>`:
	  ```c
		struct timer_list {
			struct list_head entry; /* entry in linked list of timers */
			unsigned long expires; /* expiration value, in jiffies */
			void (*function)(unsigned long); /* the timer handler function */
			unsigned long data; /* lone argument to the handler */
			struct tvec_t_base_s *base; /* lone argument to the handler */
		};
	  ```
	- **Steps**:
		- **Define:** `struct timer_list my_timer;`
		- **Initialize:** `init_timer(&my_timer);`
		- **Set:**
		  ```c
			my_timer.expires = jiffies + delay; /*timer expires in delay ticks*/
			my_timer.data = 0; /* zero is passed to the timer handler */
			my_timer.function = my_function; /* function to run when timer expires */
		  ```
		- **Activate:** `add_timer(&my_timer);`
			- And, voila, the timer is off and running!
		- **Handler prototype:** `void my_timer_function(unsigned long data);`
	- **Functions**:
		- `mod_timer(&my_timer, jiffies + new_delay)`:
			- To modify the expiration of an already active timer.
			- The `mod_timer()` function can operate on timers that are initialized but not active, too.
			- The function returns zero if the timer were inactive and one if the timer were active. In either case, upon return from `mod_timer()`, the timer is activated and set to the new expiration.
		- `del_timer(&my_timer)`:
			- deactivate a timer prior to its expiration.
			- The function works on both active and inactive timers.
			- If the timer is already inactive, the function returns zero; otherwise, the function returns one.
			- Note that you do *not* need to call this for timers that have expired because they are automatically deactivated.
		- `del_timer_sync(&my_timer)`:
			- Deactivates and waits for handler completion if it is being executed concurrently by another processor (not for interrupt context).

	- The kernel runs the timer handler when the current tick count is *equal to or greater than* the specified expiration. Although the kernel guarantees to run no timer handler prior to the timer’s expiration, there may be a delay in running the timer. Typically, timers are run fairly close to their expiration; however, they might be delayed until the first timer tick after their expiration. Consequently, timers cannot be used to implement any sort of hard real-time processing.

- #### Timer Race Conditions
	- **Issues**:
		- Timers run asynchronously (via softirq), risking concurrent access to shared data.
		- Deleting timer doesn’t guarantee handler isn’t running on another processor.
		- Avoid the following (Unsafe on SMP):
		  ```c
			del_timer(my_timer);
			my_timer->expires = jiffies + new_delay;
			add_timer(my_timer);
		  ```
	  - **Solutions**:
		  - Use `del_timer_sync()` to wait for handler completion (preferred for safety).
			  - Otherwise, you cannot assume the timer is not currently running, and that is why you made the call in the first place! Imagine if, after deleting the timer, the code went on to free or otherwise manipulate resources used by the timer handler.Therefore, the synchronous version is preferred.
		  - Protect shared data in handler with locks (see Chapters [[Ch08_Bottom_Halves_and_Deferring_Work]], [[Ch09_Introduction_to_Kernel_Synchronization]]).
	  - **Real-World Analogy**: Like ensuring a kitchen reminder doesn’t trigger after you’ve already checked the food, locking the recipe book (data) to avoid mix-ups.
	  - **Modern Usage**
		  - `del_timer_sync()` standard for safe cleanup (see kernel/time/timer.c).
		  - High-resolution timers (HRT) preferred for precise tasks, reducing race risks.

- #### Timer Implementation
	- **Execution**: Timers run as softirqs (`TIMER_SOFTIRQ`) in bottom-half context, triggered by `run_local_timers()` in timer interrupt.
	  ```c
		void run_local_timers(void)
		{
			hrtimer_run_queues();
			raise_softirq(TIMER_SOFTIRQ); /* raise the timer softirq */
			softlockup_tick();
		}
	  ```
	- The `TIMER_SOFTIRQ` softirq is handled by `run_timer_softirq()`. This function runs all the expired timers (if any) on the current processor. In other words, Timer softirq (`run_timer_softirq()`) executes expired timers, scheduled by `raise_softirq(TIMER_SOFTIRQ)` in `run_local_timers()`.
	- **Storage**: Timers in linked list, partitioned into five groups by expiration time.
		- Timers move through groups as expiration nears, minimizing search overhead.
	- **Real-World Analogy**: Like a kitchen assistant (softirq) checking a prioritized to-do list (timer groups) when the timer buzzes, handling tasks efficiently.
	- **Modern Usage**
		- High-resolution timers (HRT) use `hrtimer_run_queues()` for sub-jiffy precision (see `kernel/time/hrtimer.c`).
		- Timer wheel optimized for scalability in modern kernels

- **Clarification: Do All Bottom Halves Use Timers?**
	- **No**: Only timer-related bottom halves (via `TIMER_SOFTIRQ`) use kernel timers.
	- **Example**: A network driver’s interrupt handler raises `NET_RX_SOFTIRQ` directly, without a timer.
	- **Timer Case**: A driver sets a timer (e.g., for motor shutdown), and its handler runs via `TIMER_SOFTIRQ`.
	- **Why Not Timers for All**: Many bottom halves (e.g., network, I/O) need immediate or event-driven execution, not time-based delays; timers are for specific delayed tasks.

- **Clarification: How Bottom Halves Are Executed**:
	- **Softirq Execution**:
		- Interrupts or kernel code call `raise_softirq(irq)` to schedule a softirq (e.g., `TIMER_SOFTIRQ`,  `NET_RX_SOFTIRQ`).
		- Post-interrupt, `do_softirq()` checks pending softirqs and runs their handlers (e.g., `run_timer_softirq()` for timers).
		- **Example**: `run_timer_softirq()` iterates timer list, runs expired timer handlers.
	- **Tasklets**: Scheduled via `tasklet_schedule()`, run in `TASKLET_SOFTIRQ` or `HI_SOFTIRQ`.
	- **Work Queues**: Scheduled via `schedule_work()`, run by kernel worker threads, not timers.

### Delaying Execution
- **Purpose**: Kernel code (e.g., drivers) needs to delay execution for short periods (e.g., microseconds for hardware) without using timers or bottom halves.
- **Use Case**: Wait for hardware tasks (e.g., Ethernet mode switch, 2μs) or specific events with timeout.
- **Methods**: Busy looping (processor-intensive), small delays (`udelay`, `mdelay`), or schedule_timeout (sleep-based).
- **Real-World Analogy**: Like a chef pausing prep to wait for an ingredient to cook (hardware), choosing to stand idle (busy loop) or do other tasks (schedule_timeout).

- #### Busy Looping
	- **Method**: Spin in a loop until `jiffies` exceeds a timeout.
	  ```c
		unsigned long timeout = jiffies + 10;  /* ten ticks */
		while (time_before(jiffies, timeout))
			;
			
		unsigned long delay = jiffies + 2*HZ;  /* two seconds */
		while (time_before(jiffies, delay))
			;
	  ```
	- **Characteristics**:
		- Delays in jiffy increments (e.g., `10ms` for HZ=100, `1ms` for HZ=1000).
		- Hogs processor, preventing other work.
	- **With Scheduler**: Yields processor via `cond_resched()` if higher-priority task exists.
	  A better solution would be to reschedule your process to allow the processor to accomplish other work while your code waits:
	  ```c
		unsigned long delay = jiffies + 5*HZ;  /* five seconds */
		while (time_before(jiffies, delay))
			cond_resched();
	  ```
	- **When to use?**
		- Provides simple, coarse delays when precision isn’t critical (e.g., non-time-sensitive driver waits).
		- `cond_resched()` reduces processor hogging by allowing multitasking when needed.
		- The call to `cond_resched()` schedules a new process, but only if `need_resched` is set.
	- **Limitations**:
		- Only process context (not interrupt context, as `cond_resched()` invokes scheduler).
		- Avoid with locks held or interrupts disabled (blocks system).
		- `jiffies` marked volatile to ensure reload per loop (prevents compiler optimization).

	- **NOTE**:
		- C aficionados might wonder what guarantee is given that the previous loops even work.The C compiler is usually free to perform a given load only once.
		- Normally, no assurance is given that the `jiffies` variable in the loop’s conditional statement is even reloaded on each loop iteration.
		- The kernel requires, however, that `jiffies` be reread on each iteration, as the value is incremented elsewhere: in the timer interrupt.
		- Indeed, this is why the variable is marked volatile in `<linux/jiffies.h>`.
		- The volatile keyword instructs the compiler to reload the variable on each access from main memory and never alias the variable’s value in a register, guaranteeing that the previous loop completes as expected.

- #### Small Delays
	- **Functions** (`<linux/delay.h>`, `<asm/delay.h>`):
		- `udelay(usecs)`: Busy loop for microseconds.
		- `ndelay(nsecs)`: Busy loop for nanoseconds.
		- `mdelay(msecs)`: Busy loop for milliseconds (uses `udelay`).
	- **Implementation**:
		- Uses `loops_per_jiffy` (BogoMIPS-based) to calculate loop iterations for precise delay.
		- BogoMIPS: Measures processor’s loop speed (stored in `/proc/cpuinfo`, computed by `calibrate_delay`).
	- **When to use?**
		- Enables precise, sub-jiffy delays (e.g., 150μs) for hardware synchronization, unlike jiffy-based loops.
		- `udelay` for short delays (`<1ms`); `mdelay` for longer but less preferred.
	- **Limitations**:
		- Busy looping hogs processor; avoid for long delays or with locks/interrupts disabled.
		- `udelay` risks overflow for `>1ms`; use `mdelay` instead.
	- **Real-World Analogy**: Like a chef counting seconds (`udelay`) for a quick sauce stir, avoiding long waits to keep kitchen efficient.
	- **Modern Usage**
		- `udelay`/`mdelay` used in drivers for hardware waits (e.g., `drivers/net/ethernet`).
		- High-resolution timers preferred for precise delays.

- #### My BogoMIPS Are Bigger Than Yours!
	- **BogoMIPS**: Measures processor’s busy loop speed (bogus MIPS, million instructions per second).
		- Stored in `loops_per_jiffy`, computed at boot by `calibrate_delay` (`init/main.c`).
		- Example: `2.4GHz` Xeon shows ~4799.56 BogoMIPS in `/proc/cpuinfo`.
	- **Purpose**: Used by `udelay`/`mdelay` to calculate loop iterations for precise micro/millisecond delays.
	- **Real-World Analogy**: Like a chef knowing how many stirs per second (BogoMIPS) to time a 150μs sauce mix (`udelay`).

- #### schedule_timeout()
	- **Method**: Puts task to sleep for specified `jiffies`, waking when time elapses or signal received.
	  ```c
		/* set task’s state to interruptible sleep */
		set_current_state(TASK_INTERRUPTIBLE);

		/* take a nap and wake up in “s” seconds */
		schedule_timeout(s * HZ);
	  ```
	- **Characteristics**:
		- Uses kernel timer to sleep, not busy looping; allows other tasks to run.
		- Requires `TASK_INTERRUPTIBLE` or `TASK_UNINTERRUPTIBLE` state.
		-  Guarantees at least specified delay, may sleep longer (e.g., due to scheduling).
	- **When to use?**: Yields processor for efficient multitasking; ideal for longer, jiffy-based delays in process context.
	- **Limitations**: Process context only (invokes scheduler); avoid with locks held.
	- **Real-World Analogy**: Like a chef napping (sleep) until a dish is ready, letting others use the kitchen.

- #### schedule_timeout() Implementation
	- **Code Flow**:
		- Sets timer to expire in `timeout` jiffies; handler (`process_timeout`) wakes task.
		- Calls `schedule()` to sleep (task in `TASK_INTERRUPTIBLE` or `TASK_UNINTERRUPTIBLE`).
		- On wake-up (timer or signal), removes timer, returns time slept.
	- **Special Case**: `MAX_SCHEDULE_TIMEOUT` sleeps indefinitely (no timer), needs external wake-up.
	  ```c
		signed long schedule_timeout(signed long timeout)
		{
			timer_t timer;
			unsigned long expire;
			switch (timeout)
			{
				case MAX_SCHEDULE_TIMEOUT:
					schedule();
					goto out;
				default:
					if (timeout < 0)
					{
						printk(KERN_ERR “schedule_timeout: wrong timeout “
						“value %lx from %p\n”, timeout,
						__builtin_return_address(0));
						current->state = TASK_RUNNING;
						goto out;
					}
			}
			expire = timeout + jiffies;
			init_timer(&timer);
			timer.expires = expire;
			timer.data = (unsigned long) current;
			timer.function = process_timeout;
			add_timer(&timer);
			schedule();
			del_timer_sync(&timer);
			timeout = expire - jiffies;
		out:
			return timeout < 0 ? 0 : timeout;
		}
	  ```
	- **When to use**: Uses timer for precise sleep; ensures safe cleanup with `del_timer_sync()`.
	- **Real-World Analogy**: Like a chef setting a timer to nap (`schedule_timeout`), woken by the timer or a phone call (`signal`).

- #### Sleeping on a Wait Queue, with a Timeout
	- **Method**: Task sleeps on wait queue (for event) but uses `schedule_timeout()` instead of `schedule()` to limit wait.
	  ```c
		wait_queue_head_t wq;
		init_waitqueue_head(&wq);
		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&wq, &wait);
		schedule_timeout(s * HZ); // Wait for event or s seconds
	  ```
	- **Behavior**: Wakes on event (`wake_up`), timeout, or signal; code checks wake-up reason.
	- **Purpose of Actions**: Balances event-driven waits with timeout for drivers (e.g., hardware response or timeout).
	- **Real-World Analogy**: Like a chef waiting for a delivery (event) but napping only 5 minutes (timeout) before continuing.

- #### Additional Information:
	- **Context Restrictions**: Busy looping (`udelay`, `mdelay`, loops) OK in interrupt context but hogs processor; `schedule_timeout()`, `cond_resched()` require process context.
	- **BogoMIPS Role**: Calibrates `udelay`/`mdelay` for precise sub-jiffy delays, critical for hardware.
	- **Timer vs. Bottom Half**: `schedule_timeout()` uses timers (executed via `TIMER_SOFTIRQ`); other bottom halves (e.g., network softirqs) don’t use timers.
	- **Volatile jiffies**: Ensures `time_before` checks read updated `jiffies`, critical for loop accuracy.
	- **Modern Usage**
		- `schedule_timeout()` used in drivers (e.g., `drivers/usb/core/hub.c`).
		- High-resolution timers (`hrtimer`) preferred for sub-jiffy sleeps (see `kernel/time/hrtimer.c`)
		- `cond_resched()` optimized for PREEMPT kernels to reduce latency.


## **Quick Recall**
- **Q: What is the purpose of jiffies in the kernel?**
	- **A**: Tracks system ticks since boot, incremented every 1/HZ seconds, used for relative timing (e.g., timeouts, scheduling). _Analogy_: Like a kitchen clock ticking to track cooking time.
- **Q: Why does the kernel use USER_HZ?**
	- **A**: Scales jiffies to a fixed rate (e.g., 100 on x86) for user-space apps, ensuring legacy compatibility. _Analogy_: Like a chef adjusting a clock to standard time for guests.
- **Q: What’s the difference between RTC and system timer?**
	- **A**: RTC stores persistent wall time (battery-powered); system timer generates periodic interrupts for kernel tasks. _Analogy_: RTC is a wall calendar; system timer is a stopwatch.
- **Q: Why does the timer interrupt handler split into architecture-dependent and independent parts?**
	- **A**: Dependent part handles hardware (e.g., PIT reset); independent part (tick_periodic) manages generic tasks (e.g., jiffies update) for portability. _Analogy_: Like a chef setting a timer (hardware) before cooking tasks (generic).
- **Q: Why use a seqlock for `xtime`?**
	- **A**: Allows efficient concurrent reads by user-space with rare writes by kernel, ensuring SMP safety. _Analogy_: Like a shared kitchen log read by many, updated rarely.
- **Q: What are kernel timers used for?**
	- **A**: Delay function execution (e.g., driver timeouts) by a specific time, executed via `TIMER_SOFTIRQ`. _Analogy_: Like setting a kitchen timer for a delayed task.
- **Q: Why use softirqs for timer execution?**
	- **A**: Defers complex handler execution from interrupt context, improving system responsiveness. _Analogy_: Like a chef delegating tasks to an assistant after a timer rings.
- **Q: Why worry about timer race conditions?**
	- **A**: Asynchronous execution risks data corruption on SMP; `del_timer_sync()` ensures safe cleanup. _Analogy_: Like ensuring a kitchen timer doesn’t trigger after cleanup.
- **Q: Why avoid busy looping for delays?**
	- **A**: Hogs processor, blocking other tasks; `schedule_timeout()` yields for efficiency. _Analogy_: Like a chef standing idle instead of multitasking.
- **Q: How does `schedule_timeout()` work?**
	- **A**: Uses a timer to sleep task for specified jiffies, waking via `process_timeout()` or signal. _Analogy_: Like a chef napping until a timer or call wakes them.
- **Modern Usage**
	- Tickless kernels (`CONFIG_NO_HZ`) reduce interrupts when idle (see `kernel/time/tick-sched.c`).
	- High-resolution timers (`hrtimer`) replace `timer_list` for precision (see `kernel/time/hrtimer.c`).


## **Hands-on Ideas**
- **Jiffies Counter Test**:
	- Write a kernel module with a global `unsigned long` to track `jiffies`.
	- Create a timer (`struct timer_list`) to read `jiffies` every second, compare with global counter without locks; observe inconsistent reads in SMP.
	- Use `xtime_lock` (seqlock) to protect reads; verify consistent results.
	- **Learn**: Importance of seqlocks (`xtime_lock`) for safe `jiffies` access in SMP systems.
	  ```c
		#include <linux/module.h>
		#include <linux/jiffies.h>
		static struct timer_list timer;
		static unsigned long my_jiffies;
		void timer_fn(struct timer_list *t) {
		    unsigned long seq, val;
		    do {
		        seq = read_seqbegin(&xtime_lock);
		        val = jiffies;
		    } while (read_seqretry(&xtime_lock, seq));
		    pr_info("Safe jiffies: %lu, Unsafe: %lu\n", val, my_jiffies);
		    my_jiffies = jiffies; // Unsafe
		    mod_timer(&timer, jiffies + HZ);
		}
		static int __init init_mod(void) {
		    timer_setup(&timer, timer_fn, 0);
		    mod_timer(&timer, jiffies + HZ);
		    return 0;
		}
		static void __exit exit_mod(void) {
		    del_timer_sync(&timer);
		}
		module_init(init_mod); module_exit(exit_mod);
		MODULE_LICENSE("GPL");
	  ```

- **Verify `USER_HZ`**:
	- Check `/proc/uptime`, calculate ticks/second: `awk '{print $1*100}' /proc/uptime`.
	- Compare with `CONFIG_HZ` in `/boot/config-$(uname -r)`.
	- **Goal**: Confirm USER_HZ=100 scaling for user-space.

- **Timer Race Condition Test**:
	- Implement a module with a timer and a shared counter incremented in the timer handler.
	- Create two kernel threads: one modifies timer (`mod_timer`), another deletes it (`del_timer`); observe race conditions (e.g., handler runs after deletion).
	- Replace `del_timer` with `del_timer_sync`; verify safe cleanup.
	- **Learn**: Need for del_timer_sync to avoid race conditions in SMP timer handling.
	  ```c
		#include <linux/module.h>
		#include <linux/kthread.h>
		static struct timer_list timer;
		static int counter;
		void timer_fn(struct timer_list *t) { counter++; }
		static int thread1(void *data) {
		    while (!kthread_should_stop()) {
		        mod_timer(&timer, jiffies + HZ/2);
		        msleep(100);
		    }
		    return 0;
		}
		static int thread2(void *data) {
		    msleep(2000);
		    del_timer_sync(&timer); // Safe
		    pr_info("Counter: %d\n", counter);
		    return 0;
		}
		static int __init init_mod(void) {
		    timer_setup(&timer, timer_fn, 0);
		    mod_timer(&timer, jiffies + HZ);
		    kthread_run(thread1, NULL, "timer_mod");
		    kthread_run(thread2, NULL, "timer_del");
		    return 0;
		}
		static void __exit exit_mod(void) {}
		module_init(init_mod); module_exit(exit_mod);
		MODULE_LICENSE("GPL");
	  ```

- **Busy Loop vs. schedule_timeout**:
	- Write a module with two functions: one uses busy loop (`time_before`), another uses `schedule_timeout` for a 2-second delay.
	- Measure system load (`/proc/loadavg`) during each; observe higher load with busy loop.
	- Use `schedule_timeout` with `TASK_INTERRUPTIBLE`, test signal wake-up (`kill -SIGUSR1`).
	- **Learn**: Efficiency of `schedule_timeout` over busy looping for yielding processor.
	  ```c
		#include <linux/module.h>
		#include <linux/sched.h>
		static void busy_loop(void) {
		    unsigned long delay = jiffies + 2*HZ;
		    while (time_before(jiffies, delay));
		    pr_info("Busy loop done\n");
		}
		static void sleep_delay(void) {
		    set_current_state(TASK_INTERRUPTIBLE);
		    schedule_timeout(2 * HZ);
		    pr_info("Sleep done\n");
		}
		static int __init init_mod(void) {
		    busy_loop();
		    sleep_delay();
		    return 0;
		}
		module_init(init_mod); module_exit(init_mod);
		MODULE_LICENSE("GPL");
	  ```

- **Wait Queue Timeout Test**:
	- Create a module with a wait queue and `schedule_timeout` for 5 seconds.
	- Simulate an event using a kernel thread calling `wake_up` after 2 seconds; check wake-up reason.
	- Test with `TASK_INTERRUPTIBLE` and signal (`kill -SIGUSR1`); verify early wake-up.
	- **Learn**: Combining wait queues with timeouts for event-driven delays.
	  ```c
		#include <linux/module.h>
		#include <linux/wait.h>
		static wait_queue_head_t wq;
		static int event;
		static int wait_thread(void *data) {
		    set_current_state(TASK_INTERRUPTIBLE);
		    schedule_timeout(5 * HZ);
		    event = 1;
		    pr_info("Woke: %s\n", event ? "event" : "timeout");
		    return 0;
		}
		static int wake_thread(void *data) {
		    msleep(2000);
		    wake_up(&wq);
		    return 0;
		}
		static int __init init_mod(void) {
		    init_waitqueue_head(&wq);
		    kthread_run(wait_thread, NULL, "waiter");
		    kthread_run(wake_thread, NULL, "waker");
		    return 0;
		}
		module_init(init_mod); module_exit(exit_mod);
		MODULE_LICENSE("GPL");
	  ```

- **Modern Usage**
	- Replace `timer_list` with `hrtimer` for high-precision tasks.
	- Use `trace-cmd record -e timer` to trace timer interrupts.