
# Chapter 15: [The Process Address Space]


## **Summary**

- [[Ch12_Memory_Management]] looked at how kernel manages physical memory.
- In addition to managing its own memory, the kernel also has to manage the memory of user-space processes. This memory is called the *process address space*, which is the representation of memory given to each user-space process on the system.
- This chapter discusses how the kernel manages the process address space.

### Address Spaces

- This is the "Virtual Reality" aspect of the OS.
- The kernel deceives every process into thinking it owns the entire memory of the system.
- This isolation prevents a buggy application from crashing the whole system (or other apps).

- **Q: What is the Process Address Space?**
	- It is the set of all virtual memory addresses that a process is _allowed_ to use.
	    - In a 32-bit system, this is a flat range from `0` to `4GB`.
	    - Even if you only have 512MB of physical RAM, the process _thinks_ it has 4GB of addressable space. The kernel maps the virtual addresses to physical RAM (or swap) on the fly.

- **Q: What are Memory Areas (VMAs)?**
	- The address space isn't just one big blank canvas. It is divided into specific intervals called Memory Areas (or VMAs - Virtual Memory Areas).
	    - You cannot just access random address `0x12345678`. It must fall inside a valid VMA (like the stack, or the code section).
	    - _Analogy:_ Think of the Address Space as a huge empty field. VMAs are the fenced-off zones where you are actually allowed to build houses or plant crops.

- **Q: What happens if I touch memory outside a VMA?**
	- **Segmentation Fault**.
	    - If a process tries to read/write an address that does not belong to a valid Memory Area, or tries to write to a Read-Only area, the kernel catches it and kills the process.
	    - This is why `char *p = NULL; *p = 'a';` crashes. `0x0` is not in a valid VMA.

- **Q: What "Goodies" live in these Memory Areas?**
    1. **Text Section:** The executable code (Read-only, Executable).
    2. **Data Section:** Initialized global variables (e.g., `int count = 5;`).
    3. **BSS Section:** Uninitialized global variables (e.g., `int count;`). These are zeroed out.
    4. **Stack:** For local variables and function calls.
    5. **Memory Mapped Files:** Shared libraries (.so files) or `mmap()` regions.


### The Memory Descriptor

- The kernel needs a data structure to track all these virtual bits for every single process.
- That structure is `struct mm_struct`, often called the Memory Descriptor.
- If `task_struct` is the "Passport" of a process, `mm_struct` is its "Property Deed".

- **Q: What is `struct mm_struct`?**
	- It is the main container that holds all information about a process's address space.
	    - It lives in `<linux/mm_types.h>`.
	    - It contains the list of all VMAs, the pointer to the Page Tables, and usage counters.

- **Q: Why does it store Memory Areas in TWO different ways (`mmap` and `mm_rb`)?**
	- This is a classic "Speed vs. Simplicity" optimization. The same data (the VMAs) is stored in two structures simultaneously:
		1. **Linked List (`mmap`):** Simple to walk through linearly. Used when we need to dump all memory (e.g., `/proc/pid/maps`).
		2. **Red-Black Tree (`mm_rb`):** A balanced binary tree. Used for searching.
	- _Scenario:_ When a Page Fault happens at address `X`, the kernel needs to quickly find which VMA contains `X`. Searching a linked list is slow ($O(N)$). Searching a tree is fast ($O(\log N)$).

- **Q: What is the difference between `mm_users` and `mm_count`?**
	- This distinguishes between "threads using the memory" and "kernel references."
		- `mm_users`: The number of actual threads sharing this address space. (If you have 4 threads, this is 4).
		- `mm_count`: The reference count of the structure itself. Even if all threads die (`mm_users` = 0), the kernel might still be looking at the structure. It is only freed when `mm_count` hits 0.


#### Allocating a Memory Descriptor

- Every process needs an `mm_struct`.
- It is usually created when a process is born (`fork`).

- **Q: How do Threads relate to `mm_struct`?**
	- This is the _definition_ of a thread in Linux.
	    - **Normal Process:** Has its own unique `mm_struct`.
	    - **Thread:** Shares the `mm_struct` of its parent.
    - When you call `clone()` with the `CLONE_VM` flag, the kernel doesn't copy the memory descriptor; it just points the new task to the old one and increments `mm_users`.
    - _Analogy:_ Roommates share the same house address (`mm_struct`), but have different keys (`task_struct`).

- **Visualization: Process vs Thread Memory**
  ```mermaid
	graph TD
    subgraph "Process A (Parent)"
        TaskA[task_struct A]
        MM["mm_struct <br/> (The Address Space)"]
    end

    subgraph "Process B (Child / Fork)"
        TaskB[task_struct B]
        MM_B["mm_struct <br/> (Copy of A)"]
    end

    subgraph "Thread C (Clone)"
        TaskC[task_struct C]
    end

    TaskA -- points to --> MM
    TaskB -- points to --> MM_B
    TaskC -- SHARES --> MM
    
    note[Process B gets a COPY. <br/> Thread C SHARES the original.]
  ```


#### Destroying a Memory Descriptor

- When a process exits, the kernel cleans up.
- This logic is handled in `exit_mm()`.

- **Q: What is the cleanup sequence?**
	1. Decrement `mm_users`.
	2. If `mm_users` hits 0, it means the last thread has left the building. The kernel tears down the memory mappings (VMAs) and page tables.
	3. Decrement `mm_count`.
	4. If `mm_count` hits 0, the `mm_struct` itself is freed back to the slab allocator.


#### The `mm_struct` and Kernel Threads

- Kernel Threads (like `kworker`, `ksoftirqd`) run strictly in kernel mode.
- They never access user-space memory.

- **Q: Do Kernel Threads have a Memory Descriptor?**
	- **No.**
	    - Their `task_struct->mm` field is `NULL`.
	    - They do not have a user address space because they don't need one.

- **Q: But they still need Page Tables to see kernel memory. What do they use?**
	- They "borrow" the address space of whatever process ran before them.
		- This is the `active_mm` field.
		- When a kernel thread is scheduled in, the kernel sees `mm` is NULL. It says: "Okay, keep the previous process's Page Tables loaded (`active_mm`), but remember you are not allowed to touch user-space."
		- _Analogy:_ A maintenance worker (Kernel Thread) enters a hotel room. They don't rent the room (No `mm`), but they stand inside it (`active_mm`) to fix the AC. They don't touch the guest's luggage.

#### Modern Usage:

- **The Maple Tree (`mt_tree`) Revolution (v6.1+)**
	- **Old Kernel:** VMAs are stored in a Linked List (`mmap`) and a Red-Black Tree (`mm_rb`).
	- **Modern Kernel:** As of Kernel 6.1 (2022), the Red-Black Tree and the Linked List have been largely **replaced** by a new data structure called the **Maple Tree**.
		- **Why?** The Maple Tree is optimized for modern CPUs (better cache usage) and handles "ranges" of memory much better than a binary tree. This reduces contention (locking issues) on massive servers.
		- _Note:_ You won't find `mm_rb` playing the same role in the newest kernels.

- **ASLR (Address Space Layout Randomization):**
	- **Old Kernel:** Describes a standard layout (Text at bottom, Stack at top).
	- **Modern Kernel:** For security, Linux randomizes the starting locations of the stack, heap, and libraries every time you run a program. This makes it much harder for hackers to exploit buffer overflows.

- **5-Level Paging:**
	- **Old Kernel:** Assumes 32-bit or 64-bit with 4 levels of paging.
	- **Modern Kernel:** Modern Intel/AMD CPUs support 5-level paging, allowing address spaces up to 128 Petabytes (virtually). The `mm_struct` handles this massive scale.



### Virtual Memory Areas

- While `mm_struct` represents the entire address space (the whole house), the `vm_area_struct` represents the individual rooms (Kitchen, Bedroom, Garage).
- A VMA represents a **contiguous** interval of addresses that share the same **permissions**.
- If you have a block of memory that is Read-Only, and right next to it a block that is Read-Write, those **must** be two separate VMAs.

- **Q: What is the `vm_area_struct`?**
	- It is the kernel data structure that defines a memory region. Key fields include:
		- `vm_start`: The starting address (inclusive).
		- `vm_end`: The ending address (exclusive).
		- `vm_mm`: A pointer back to the main `mm_struct` (owner).
		- `vm_flags`: Permissions (Read, Write, Exec).
		- `vm_ops`: Functions to handle operations on this memory (like what to do if a Page Fault occurs here).

- **Q: What are VMA Flags?**
	- These flags tell the kernel _how_ to treat the memory. The hardware (MMU) enforces some, and the kernel software enforces others.
		- **`VM_READ`, `VM_WRITE`, `VM_EXEC`:** Standard permissions. (e.g., Code is `VM_READ | VM_EXEC`).
		- **`VM_SHARED`:** If set, writing to this memory updates the file on the disk (and other processes see it). If NOT set, it's a private copy (Copy-on-Write).
		- **`VM_IO`:** **Crucial for Embedded**. This flag tells the kernel "This is not RAM; this is a mapped hardware device register." It prevents the kernel from trying to include this memory in a core dump (which would crash the system).
		- **`VM_SEQ_READ`:** A hint that the app is reading the file sequentially. The kernel will aggressively pre-load data (Read-Ahead).

- **Q: What are VMA Operations (`vm_ops`)?**
	- This is Object-Oriented Programming in C.
		- Different VMAs behave differently. A VMA backed by a file behaves differently than a VMA backed by a hardware device.
		- The `vm_ops` table holds function pointers.
		- **The Big One:** `fault()`. When you access an address in this VMA but the data isn't in RAM yet, the kernel calls this function to go fetch the data (from disk, swap, or network).

- **Visualization: The VMA Chain**
  
  ```mermaid
	graph LR
    MM[mm_struct]
    
    subgraph "The VMA List"
        VMA1[VMA: Text Section Read-Only 0x1000 - 0x2000]
        VMA2[VMA: Data Section Read-Write 0x2000 - 0x3000]
        VMA3[VMA: Stack Read-Write 0xBFFF - 0xC000]
    end
    
    MM -- points to --> VMA1
    VMA1 -- vm_next --> VMA2
    VMA2 -- vm_next --> VMA3
    VMA3 -- vm_next --> NULL
  ```


#### List and Trees of Memory Areas (Data Structures)

- As mentioned in the previous sections, the kernel stores these VMAs in two ways simultaneously. This section explains _why_.

- **Q: Why keep two data structures for the exact same data?**
	- Optimization for different usage patterns.
	1. **Linked List (`vm_next`):**
	    - _Purpose:_ Iteration.
	    - _Use Case:_ When you run `cat /proc/pid/maps`, the kernel just walks this list from start to finish to print the memory map.
	2. **Red-Black Tree (`vm_rb`):**
	    - _Purpose:_ Search (Lookup).
	    - _Use Case:_ Page Faults. When a program tries to access address `0x5000`, the kernel needs to know _immediately_: "Is `0x5000` inside a valid VMA?"
	- Walking a list is too slow ($O(N)$). Searching a tree is fast ($O(\log N)$).


#### Memory Areas in Real Life

- Theory is great, but let's see what this looks like on a real Linux system.
- We use the `pmap` tool or the `/proc` filesystem.

- **Q: What does a typical process look like?**
	- It usually has these standard VMAs:
	    1. **Text:** The code of the program (Read-Only, Executable).
	    2. **Data:** Global variables (Read-Write).
	    3. **Libs:** Shared libraries (like `libc.so`).
	    4. **Stack:** The "scratchpad" for function calls.

- **Q: Why is the `libc` text section only 40KB private, even if the library is 2MB?**
	- **Shared Memory magic.**
		- The code for `libc` (`printf`, `memcpy`, etc.) is Read-Only.
		- Linux loads it into physical RAM **once**.
		- Every single process on the system maps that _same_ physical RAM into their VMA.
		- If you run 1000 processes, you don't load `libc` 1000 times. You load it once. This saves massive amounts of RAM.


#### Modern Usage:

The way Linux manages VMAs has undergone a massive change recently (2022/2023).

- **The Death of the Red-Black Tree (Enter the Maple Tree)**
	- **Old Kernel:** Specifically describes the Red-Black Tree (`vm_rb`) and Linked List (`mmap`) as the dual structures.
	- **Modern Kernel (v6.1+):** These have been replaced by the Maple Tree (`mt_tree`).
		- **Why?**
			- The Maple Tree is a "B-tree" variant designed specifically for ranges. It is cache-friendly and supports "lock-less" lookups (using RCU). This allows massive parallelism on huge servers, where the old Red-Black tree lock was a bottleneck.

- **VMA Merging**
	- **Old Kernel:** Treats VMAs as static objects.
	- **Modern Kernel:** The kernel aggressively tries to **merge** adjacent VMAs. If VMA A ends at `0x2000` and VMA B starts at `0x2000`, and they have the exact same flags/permissions, the kernel will fuse them into one larger VMA to reduce overhead.

- **`vm_flags` Exhaustion**
	- **Old Kernel:** `vm_flags` is an `unsigned long`.
	- **Modern Kernel:** We recently ran out of bits! In very recent kernels, the flag handling has become more complex because features like "UserfaultFD", "Memory Protection Keys", and "Shadow Stacks" all need flags.


### Manipulating Memory Areas (`find_vma`)

- The kernel frequently needs to answer the question: _"This process wants to access address `0xA000`. Which VMA does this belong to?"_
	- This happens on every **Page Fault**. Speed is critical here.

- **Q: How does `find_vma()` work?**
	- It searches for the **first** VMA whose _end address_ is greater than the requested address.
	- It does _not_ necessarily mean the address is valid. It just finds the nearest VMA container.
	- After finding it, the kernel checks: `if (vma->vm_start <= address)`. If yes, it's a hit. If no, the address is in a "hole" (invalid memory).

- **Q: How does it optimize the search?**
	- It uses two tricks:
	1. **The Cache (`mmap_cache`):** The kernel remembers the _last_ VMA you looked up. Because programs often access the same area repeatedly (locality of reference), checking this first hits about 30-40% of the time.
	2. **The Tree (`vm_rb`):** If the cache misses, it searches the Red-Black Tree.


### `mmap()` and `do_mmap()`: Creating an Address Interval

- This is how you add new "rooms" to your house.
- In User Space, you call `mmap()`. In Kernel Space, the work is done by `do_mmap()`.
- Saying that this function creates a new VMA is not technically correct, because if the created address interval is adjacent to an existing address interval, and if they share the same permissions, the two intervals are merged into one. If this is not possible, a new VMA is created. In any case, `do_mmap()` is the function used to add an address interval to a process’s address space—whether that means expanding an existing memory area or creating a new one.

- **Q: What is the difference between `MAP_SHARED` and `MAP_PRIVATE`?**
	- This determines if your changes are visible to others.
		- **`MAP_SHARED`:** If you write to the memory, the changes are written back to the file on the disk. Other processes mapping the same file see the change instantly.
		- **`MAP_PRIVATE`:** The kernel uses Copy-on-Write (COW). You see the file data, but if you write to it, the kernel makes a private copy of that page just for you. The file on disk is _not_ touched. This is how writable data sections in libraries work.

- **Q: What is an Anonymous Mapping?**
	- A memory area not backed by any file.
		- This is how `malloc()` usually gets large chunks of memory. It asks the kernel for a blank sheet of paper (RAM filled with zeros).


### Page Tables (The Translation Layer)

- Everything we discussed so far (VMAs) is in the **Virtual** world.
- The CPU hardware (MMU) needs **Physical** addresses to talk to RAM chips.
- **Page Tables** are the map that translates Virtual -> Physical.

- **Q: Why do we need "Levels" of Page Tables?**
	- To save memory.
	    - Imagine a library index. If you listed every single book in one giant list, the list would be miles long.
	    - Instead, you have a "Section Index" (Science, Fiction), which points to a "Shelf Index", which points to the "Book".
	    - If a process uses only 10MB of RAM, we only create the specific index pages needed for those 10MB. We don't need a map for the empty 4GB void.

- **Q: What are the levels in Linux (Standard)?**
	1. **PGD (Page Global Directory):** The top-level index. Found in `mm_struct->pgd`.
	2. **PMD (Page Middle Directory):** The second level.
	3. **PTE (Page Table Entry):** The final level. This points to the actual Physical Page Frame.

- **Q: What is the TLB (Translation Lookaside Buffer)?**
	- The Hardware Cache for translations.
		- Walking the PGD -> PMD -> PTE chain takes time (memory reads).
		- The CPU stores recent translations in the TLB.
		- _Analogy:_ "Page Tables" are the phonebook. "TLB" is the speed-dial list on your phone.

- **Visualization: Virtual to Physical Lookup**
  ```mermaid
		graph LR
	    VA[Virtual Address] --> PGD_Idx
	    VA --> PMD_Idx
	    VA --> PTE_Idx
	    VA --> Offset
	
	    subgraph "The Page Table Walk"
	        PGD[PGD Table] -- points to --> PMD[PMD Table]
	        PMD -- points to --> PTE[PTE Table]
	        PTE -- points to --> PhysFrame[Physical Page Frame]
	    end
	
	    subgraph "Hardware RAM"
	        PhysFrame -- "\+ Offset" --> Final[Actual Data Byte]
	    end
  ```


#### Modern Usage:

The translation layer has scaled up to meet modern hardware demands.

- **From 3 Levels to 5 Levels**
	- **Old Kernel:** Mentions 3 levels (PGD, PMD, PTE) as the standard.
	- **Modern Kernel:** 64-bit architectures now use 4 levels (PUD - Page Upper Directory added) or even 5 levels (P4D added).
		- **_Why?_** To support massive amounts of RAM (Petabytes).

- **Huge Pages (2MB / 1GB Pages):**
	- **Old Kernel:**  Assumes standard 4KB pages.
	- **Modern Kernel:** We use Transparent Huge Pages (THP). The kernel tries to group memory into 2MB blocks.
		- **_Benefit:_** A 2MB page requires only one TLB entry. Using 4KB pages for 2MB of data requires 512 TLB entries. Huge pages reduce TLB misses drastically.

- **The Maple Tree (Again)**
	- Just a reminder: `find_vma()` in v6.1+ uses the Maple Tree, not the Red-Black tree mentioned in the book text.



## **Quick Recall**

- **Q: What is the difference between `mm_users` and `mm_count` in the memory descriptor?**
	- `mm_users` counts how many threads/processes are actively sharing this address space. `mm_count` is the reference count for the `struct mm_struct` itself. The structure is only freed when `mm_count` hits 0.

- **Q: If I malloc(1GB) of memory, does the kernel immediately reserve 1GB of physical RAM?**
	- **No**. The kernel creates a VMA (a virtual reservation) saying "You are allowed to have this." Physical RAM is only allocated page-by-page via Page Faults when you actually write to that memory.

- **Q: Why do we have separate `vm_area_structs` (VMAs)? Why not just one big list?**
	- Because different memory regions need different **permissions** (Read-only vs Read-Write) and different **backing stores** (File vs Anonymous). You cannot mix them in one blob.

-  **Q: What happens if a kernel thread (like `kworker`) tries to access a user-space virtual address?**
	- It will crash (Oops). Kernel threads do not have a user address space (`mm` is NULL). They run purely in the kernel portion of the virtual map.

- **Q: What is the "Page Table Walk"?**
	- The process where the MMU (or software) reads the PGD, then the PMD, then the PTE to find the physical address. It is slow, which is why the TLB (cache) exists.


## **Hands-on Ideas**

As an embedded developer, understanding memory maps is crucial for debugging crashes and optimizing RAM usage.

### The "Cartographer": Inspecting `pmap`
- See exactly how your program is laid out in memory.
- **Task:**
	1. Write a simple C program that halts:
	   ```c
		#include <stdlib.h>
		#include <stdio.h>
		int main()
		{
		    void *p = malloc(1024 * 1024 * 10); // 10MB
		    printf("Malloc done. Pid: %d\n", getpid());
		    getchar(); // Pause
		    return 0;
		}
	   ```
	   2. Run it in the background.
	   3. Run `pmap -x <PID>`.
	   4. **Observe:** You will see the "anon" block (your malloc). You will see `libc` and `ld`.
	   5. **Check:** Look at the "RSS" (Resident Set Size) column. Is it 10MB? (Hint: No, it will be near 0 because you haven't written to the memory yet! Allocation is lazy).

### The "Raw Map": Reading /proc/maps
- Embedded systems often lack `pmap`. You must read the raw source.
- **Task:**
	1. Cat the maps file: `cat /proc/self/maps`.
	2. **Decode the format:**
	    - `Address Range` | `Perms` | `Offset` | `Dev` | `Inode` | `Path`
	    - e.g., `00400000-00401000 r-xp ... /bin/cat`
	3. **Identify:** Which regions are `r-xp` (Code/Text)? Which are `rw-p` (Data/Stack)?

### The "Leak Hunter": Watching VMA growth
- If an app keeps calling `mmap` but forgets `munmap`, it leaks VMAs.
- **Task:**
	1. Run `watch -n 1 "cat /proc/<PID>/maps | wc -l"`.
	2. If that number keeps going up, your application is leaking memory mappings. The kernel has a limit (`sysctl vm.max_map_count`), and if you hit it, `malloc` will fail even if you have plenty of free RAM.