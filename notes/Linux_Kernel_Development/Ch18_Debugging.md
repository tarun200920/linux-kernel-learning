# Chapter 18: [Debugging]

## **Summary** 

- Harder than user space debugging because one fault can crash the whole system.
- Success depends heavily on your experience and OS knowledge.

### Getting Started

- **Q: What 3 things do you need to start debugging?**
	- **A reproducible bug:** If you can reproduce it, you are halfway to fixing it. If not, you can only stare at code.
	- **A kernel version:** Knowing where the bug first appeared narrows down the cause.
	- **Knowledge:** Understanding the surrounding code makes the fix easier.

### Bugs in the Kernel

- Range from bad code to synchronization errors, causing poor performance, data corruption, or deadlocks.

- **Q: Why are kernel bugs hard to trace?**
	- The crash location is often far from the actual bug (a "cold case").
	- **Example:** Process A frees a struct too early (the real bug). Later, Process B accesses that dead pointer and crashes (the visible symptom).

- **Q: NULL pointer dereference vs. reading garbage?**
	- Depends if the freed memory was overwritten yet.
	- **NULL dereference:** Points to nothing valid = immediate oops.
	- **Garbage data:** Old memory still looks valid = silent corruption + weird bugs later.

#### Debugging by Printing
- Core function is `printk()`. Nearly identical to `printf()`.

##### Robustness
- `printk()` is safe to call from almost anywhere: interrupt context, process context, or while holding a lock.

- **Q: When does `printk()` fail?**
	- Very early in boot, before the console is initialized (nowhere to send output).
	- **Fix:** Use `early_printk()`. (Warning: architecture specific, not portable).


##### Loglevels
- The main difference from `printf()`. A priority tag deciding if a message hits the console.

- **Q: How does the kernel decide what to display?**
	- It compares the message's loglevel against the system's `console_loglevel`.
		- Rule: A message prints only if its priority is higher (numerically lower) than the console setting.
		- `KERN_EMERG` (`<0>`) is highest priority. `KERN_DEBUG` (`<7>`) is lowest.
		- Always specify a level (e.g. `printk(KERN_WARNING "...")`); the default is subject to change.

- 
	| Loglevel | Value | Use Case |
	| --- | --- | --- |
	| KERN_EMERG | <0> | System is dead |
	| KERN_ALERT | <1> | Needs immediate attention |
	| KERN_CRIT | <2> | Critical condition |
	| KERN_ERR | <3> | Error |
	| KERN_WARNING | <4> | Warning (Default) |
	| KERN_NOTICE | <5> | Noteworthy |
	| KERN_INFO | <6> | Info |
	| KERN_DEBUG | <7> | Debug only |

##### The Log Buffer

- Messages go to a fixed size memory buffer first, not directly to screen.
- Size set at compile time via `CONFIG_LOG_BUF_SHIFT`.

- **Q: Why a "circular" buffer?**
	- When full, new messages overwrite the oldest ones.
	- Pro: Robust and safe for interrupt context; prevents infinite memory usage.
	- Con: You can lose old messages during a log flood.

##### syslogd and klogd

- Daemons that pull logs out of kernel memory and save them to disk.
- **klogd:** Reads messages from the kernel buffer (via `/proc/kmsg`).
- **syslogd:** Takes those messages and writes them to `/var/log/messages`.

#### Modern Usage

1. **Dynamic Debug**
	- **Old Kernel:** To toggle debug prints, you edited source code (`#define DEBUG`) and recompiled.
	- **Modern Kernel:** Use Dynamic Debug (`dyndbg`). Enable/disable specific `pr_debug()` lines on a live system via debugfs—no reboot or recompile needed.

2. **Reading Logs**
	- **Old Kernel:** Relied strictly on `klogd`/`syslogd` writing to text files.
	- **Modern Kernel:** Replaced by **systemd-journald**. Read logs via `dmesg` (use `dmesg -w` to watch live) or `journalctl -k`. Timestamps are now default.

3. **Modern Kernel in ARM (Booting)**
	- **Old Kernel:** `early_printk` was the non-portable last resort.
	- **Modern ARM:** Use `earlycon`. Pass the UART's physical memory address in the bootargs (e.g. `earlycon=uart8250,mmio32,0x...`) to see crashes the instant the CPU boots.


### Magic SysRq Key

- A "lifesaver" key combo that talks directly to the kernel, even when the system is locked up.
- Enabled via `CONFIG_MAGIC_SYSRQ`. Toggled at runtime via `echo 1 > /proc/sys/kernel/sysrq`.

- **Q: How to safely reboot a frozen machine?** 
	- Use the sequence R-E-I-S-U-B (often remembered as "Reboot Even If System Utterly Broken"). The critical 3 are:
	- `SysRq-s`: **Sync** disks (saves your data).
	- `SysRq-u`: **Unmount** filesystems.
	- `SysRq-b`: **Reboot**.
	  
	| Key | Description |
	| --- | --- |
	| SysRq-b | Reboots the machine |
	| SysRq-m | Dumps memory info |
	| SysRq-p | Dumps registers |
	| SysRq-s | Syncs filesystems to disk |
	| SysRq-t | Dumps task info |
	| SysRq-u | Unmounts filesystems |

### Oops

- The kernel's way of reporting a serious fault. It dumps registers + a back trace, then tries to continue.
- Unlike apps, the kernel can't just kill itself, so it "backs out" of the current context.

- **Q: When does an oops become a fatal panic (total halt)?** 
	- When the kernel has nowhere safe to return to:
		- Oops in **interrupt context**.
		- Oops in the **idle task (pid 0)** or **init task (pid 1)**.
		- _(In a normal process, it just kills that process and continues)._

- **Q: What is the most useful part of an oops?**
	- The Back Trace. It shows the exact chain of function calls leading to the crash.
		- Offsets like `0x128/0x1c4` pinpoint the exact offending line within a function.
		- Register dumps help you see which register held the bad value (e.g. NULL).

#### ksymoops & kallsyms

- **Problem:** A raw oops shows crash locations as useless hex addresses.
- **ksymoops (Old way):** A manual tool to translate addresses to function names using the `System.map` file. Error prone.
- **kallsyms (The Fix):** Enabled via `CONFIG_KALLSYMS`. Stores the name-to-address map _inside_ the kernel image.
    - **Benefit:** The kernel decodes its own crashes automatically. No external tools needed.
    - **Cost:** Slightly larger kernel size.

#### Modern Usage (Oops)

1. **Decoding**
	- **Old Kernel:** `ksymoops` tool (now dead).
	- **Modern Kernel:** `kallsyms` is standard. For raw traces, use the script `scripts/decode_stacktrace.sh`.

2. **Security (Pointer Hashing)**
	- **Old Kernel:** Printed raw kernel memory addresses freely.
	- **Modern Kernel:** For security (defeating hackers), `%p` now prints a _hashed_ value by default, hiding the real memory address.

### Kernel Debugging Options

- Compile-time flags in the "Kernel Hacking" menu. Master switch is `CONFIG_DEBUG_KERNEL`.

- **Q: What is the most valuable check here?** 
	- **Atomicity checking** (sleep-inside-spinlock detection).
		- **Rule:** Atomic code (holding a spinlock) _cannot_ sleep. Doing so causes a deadlock.
		- This infra uses a counter to detect illegal sleeps and prints a warning + back trace.
		- **Enable via:** `CONFIG_DEBUG_SPINLOCK_SLEEP=y` (plus `PREEMPT`, `DEBUG_KERNEL`, `KALLSYMS`).

### Asserting Bugs and Dumping Information

- Sanity checks to flag situations that should "never happen".

- **Q: BUG_ON() vs BUILD_BUG_ON()?**
	- Runtime vs Compile-time.
	- `BUG_ON(condition)`: Triggers an oops at **runtime** if true. (Preferred over `BUG()` as it's more readable).
	- `BUILD_BUG_ON(condition)`: Aborts the **compilation** if true. Catches errors before deployment.

- **Q: What other dumping tools exist?**
	- `panic()`: Prints error and halts the entire kernel. Use only for unrecoverable disasters.
	- `dump_stack()`: Prints a back trace but keeps running. Used when you just want to see the call path.

#### ### Modern Usage (Assertions & Options)

1. **Shift to WARN_ON**
	- **Old Kernel:** Used `BUG_ON()` frequently (crashes the process).
	- **Modern Kernel:** Community discourages `BUG_ON()`. Prefer `WARN_ON_ONCE()`. It logs the full stack trace but keeps the system alive, letting you gather the report without a dead machine.

2. **The Sanitizers (Major Upgrade)**
	- **Old Kernel:** Manual `DEBUG_SLAB` and spinlock checks.
	- **Modern Kernel:** Dominated by compiler-based tools:
	    - **KASAN**: Instantly catches use-after-free & out-of-bounds bugs (the "cold cases").
	    - **Lockdep**: Maps all lock dependencies to predict deadlocks _before_ they happen.

3. **Modern Kernel in ARM (Size)**
	- **Old Kernel:** Discussed compiling `BUG_ON` away to save space.
	- **Modern ARM Kernel:** Standard practice. Set `CONFIG_BUG=n` to strip assertion strings, shrinking the binary for tight flash storage.


### The Saga of a Kernel Debugger

- Linux historically had no built-in debugger. Linus opposed it, arguing debuggers lead to bad "band-aid" fixes by developers who don't truly understand the code.
- As a result, debuggers exist as external patches rather than core features.

#### gdb

- Use the standard GNU debugger to peek inside a _running_ kernel.
- **Command:** `gdb vmlinux /proc/kcore`
    - `vmlinux`: The uncompressed kernel image (not `bzImage`).
    - `/proc/kcore`: Acts as the "core file" to view live memory (needs root).
- **Useful commands:** `p global_variable` (print value) and `disassemble function`.
- **Tip:** Compile with the `-g` flag for richer info (but this creates a much larger kernel).

#### kgdb

- A patch that unlocks the _full_ power of gdb to debug the kernel remotely.
- **Requirement:** Needs **two computers** connected via a serial line (null modem cable).
    - Machine 1: Runs the target kernel (patched with kgdb).
    - Machine 2: Runs gdb to control Machine 1.
- **Capability:** Unlike plain gdb, this allows the full feature set: setting breakpoints, watchpoints, single-stepping, and _modifying_ variables.


#### Modern Usage (Debuggers)

1. **kgdb is Mainline**
	- **Old Kernel:** `kgdb` was a messy external patch you had to hunt for online.
	- **Modern Kernel:** `kgdb` is fully merged into the mainline kernel. Just enable `CONFIG_KGDB`. It also gained `kdb`, a simple front-end shell for basic debugging directly on the target machine (no second PC needed for basic tasks).

2. **Modern Kernel in ARM (JTAG)**
	- **Old Kernel:** Serial line (null modem cable) was the standard connection.
	- **Modern ARM Kernel:** Hardware debugging via **JTAG/SWD** probes (like a J-Link) is the industry standard. This lets you debug the CPU physically, even before RAM or the console is initialized (impossible with kgdb).

3. **Tracing (The Big Shift)**
	- **Old Kernel:** Debugging meant _stopping_ the kernel (breakpoints).
	- **Modern Kernel:** The philosophy shifted to _tracing_ (watching without stopping). Tools like **ftrace** (function tracer) and **eBPF/bpftrace** let you probe a live production system's behavior with near-zero overhead.


### Poking and Probing the System

- A collection of practical tricks to isolate bugs during active development.

#### Using UID as a Conditional

- **Goal:** Test new/risky code without breaking the working system for everyone.
- **How:** Branch the logic based on User ID. Only your special test user runs the new code.
  ```c
	if (current->uid != 7777) {
	    /* old, safe algorithm */
	} else {
	    /* new, risky algorithm */
	}
  ```

#### Using Condition Variables

- **Goal:** Same as above, but for code _not_ in process context (where no UID exists).
- **How:** Use a global variable as an on/off switch. Set it via a debugger "poke" or an exported interface.

#### Using Statistics

- **Goal:** Count how often specific events (`foo` vs `bar`) occur.
- **How:** Declare global counters, increment them on each event, and read them via `/proc` or a debugger.
- **Note:** Not SMP-safe by default, but that's usually fine for temporary debug counters.

#### Rate and Occurrence Limiting

- **Problem:** Putting `printk()` in a hot path (called thousands of times/sec) floods the console and freezes the system.

- **Q: What are the two ways to control the flood?**
	- **1. Rate Limiting (Limit by Time):** Print a message only every few seconds. Watches an event's _progression_ without spamming.
	    - Helper: `if (error && printk_ratelimit())`. (Default: 1 message per 5 secs, allows initial burst of 10).
	- **2. Occurrence Limiting (Limit by Count):** Print a message a fixed number of times, then stop forever.
	    - Logic: `if (limit < 5) { limit++; printk(...); }`


### Binary Searching to Find the Culprit

- **Goal:** Find _when_ a bug was introduced by testing kernel versions.
- **Method:** Start with a known-good version and a known-bad version. Test the version exactly in the middle.
    - If middle is **good** -> bug is in the newer half.
    - If middle is **bad** -> bug is in the older half.
- Repeat until you narrow it down to the single breaking version. (Beats checking every version linearly!).

#### Binary Searching with Git

- `git bisect` automates this search down to the exact commit (not just the version).

- **Q: What is the git bisect workflow?**
	1. `git bisect start`
	2. `git bisect bad` (Mark current version as broken).
	3. `git bisect good v2.6.28` (Mark last known working version).
	4. _Git checks out a middle commit. You compile & test it._
	5. Tell git the result: `git bisect good` OR `git bisect bad`.
	6. Repeat until Git prints the exact offending commit.

- **Speed Tip:** If you suspect a location, limit the search scope: `git bisect start - arch/x86`.

### When All Else Fails: The Community

- If the bug is in the mainline kernel and you are truly stuck, ask the experts.
- Send a brief but complete email describing the bug and your findings to the Linux Kernel Mailing List (LKML).

#### Modern Usage (Bug Hunting)

1. **Automated Bisection**
	- **Old Kernel:** `git bisect` required manual compiling and testing at every single step.
	- **Modern Kernel:** Use `git bisect run <my_script.sh>`. Git automatically compiles, runs your test script, and bisects based on the script's exit code—fully automating the hunt.

2. **Community & Bug Reporting**
	- **Old Kernel:** Raw emails to LKML were the only channel.
	- **Modern Kernel:** Massive automated systems now catch bugs _before_ humans:
	    - syzbot: Continuously "fuzzes" the kernel (throws random inputs at it) to find crashes and auto-reports them.
	    - KernelCI: Automated testing across hundreds of hardware boards (including many ARM devices) to catch regressions instantly.

## **Quick Recall**

- **Q: Why are kernel bugs harder to trace than app bugs?**
	- The crash location is often far from the actual bug (the "cold case" problem).

- **Q: What is the core kernel printing function, and how is it unique?**
	- `printk()`. It attaches a loglevel (priority) and is safe to call from almost any context.

- **Q: Which loglevel is the highest priority?**
	- `KERN_EMERG` (`<0>`). `KERN_DEBUG` (`<7>`) is the lowest.

- **Q: What kind of buffer stores kernel messages?**
	- A fixed-size circular buffer (new messages overwrite the oldest).

- **Q: How do you read the kernel log buffer from userspace?**
	- The `dmesg` command (modern) or reading `/var/log/messages` (old).

- **Q: What is the "Magic SysRq" sequence to safely reboot a frozen box?**
	- **S-U-B** (Sync, Unmount, reBoot).

- **Q: What is an "Oops"?**
	- The kernel's report of a serious fault, dumping registers and a back trace.

- **Q: When does an Oops escalate to a fatal "Panic"?**
	- If it happens in interrupt context or in the idle (pid 0) / init (pid 1) tasks.

- **Q: What does `CONFIG_KALLSYMS` do?**
	- Stores symbol names inside the kernel image so it can decode its own crash addresses into function names.

- **Q: `BUG_ON()` vs `WARN_ON()`?**
	- `BUG_ON` crashes the process; `WARN_ON` (preferred) logs a trace but keeps the system alive.

- **Q: What modern tool instantly catches use-after-free memory bugs?**
	- KASAN (Kernel Address Sanitizer).

- **Q: What command finds the exact commit that introduced a bug?**
	- `git bisect`.

- **Q: Why did Linus resist an in-kernel debugger?**
	- He believed it leads to lazy "band-aid" fixes instead of true understanding of the code.

## **Hands-on Ideas**

1. **Play with Loglevels (BBB or Qualcomm)**
	- In a test module, print messages at `KERN_INFO`, `KERN_WARNING`, and `KERN_DEBUG`.
	- Change the console verbosity: `echo 8 > /proc/sys/kernel/printk` (shows all) then try `echo 4`.
	- _Goal: See exactly how `console_loglevel` filters what appears on your serial console._

2.  **Trigger a Controlled Oops (BBB - safest for crashing!)**
	- In a module, intentionally dereference a NULL pointer (`int *p = NULL; *p = 1;`).
	- Load it and analyze the resulting Oops dump. Read the back trace.
	- _Goal: Learn to read a real stack trace. Use the BBB since crashing it is harmless._

3. **Trigger the Magic SysRq (BBB or Qualcomm via Serial)**
	- Enable it: `echo 1 > /proc/sys/kernel/sysrq`.
	- Over your serial console, send the break command for `SysRq-t` (dump tasks) and `SysRq-m` (dump memory).
	- _Goal: Confirm you can talk to the kernel directly, a lifesaver when the board hangs._

4. **Use ftrace to Watch Live Functions (Best on Qualcomm SA525)**
	- Mount debugfs and navigate to `/sys/kernel/debug/tracing/`.
	- Set the tracer: `echo function > current_tracer`, then read the live `trace` file.
	- _Goal: See the modern "trace, don't stop" philosophy in action on a complex SoC._

5. **Dynamic Debug without Rebooting (BBB or Qualcomm)**
	- Ensure a driver uses `pr_debug()`. Enable it live via the control file:
		-  `echo 'module <name> +p' > /sys/kernel/debug/dynamic_debug/control`.
	- _Goal: Toggle debug messages on a running kernel without any recompile—a massive time saver._

6. **Bisect a Bug with Git (On your host PC)**
	- Clone a kernel repo. Simulate finding a bug: `git bisect start`, `git bisect bad`, `git bisect good <old_tag>`.
	- _Goal: Practice the workflow of narrowing down commits, a vital real-world skill._