
# Chapter 16: [The Page Cache and Page Writeback]


## **Summary**

- The Linux kernel implements a disk cache called the *page cache*. The goal of this cache is to minimize disk I/O by storing data in physical memory that would otherwise require disk access.
- This chapter deals with the page cache and the process by which changes to the page cache are propagated back to disk, which is called *page writeback*.
- Accessing data from memory rather than the disk is much faster, and accessing data from the processor’s L1 or L2 cache is faster still.

### Approaches to Caching

- **The Bottleneck:** Disk access is measured in _milliseconds_. RAM access is measured in _nanoseconds_. The disk is roughly 100,000x slower than RAM.
- **The Solution:** Never talk to the disk unless you absolutely have to. Keep everything you read in RAM.
- **Temporal Locality:** The principle that if you read a file now, you will likely need it again very soon.

- **Q: What is the Page Cache?**
	- It is a layer of software between the VFS (Virtual File System) and the Block Layer.
	- It stores physical pages in RAM that correspond to blocks on the disk.
	- **Dynamic Sizing:** Linux is greedy. If you have 4GB of RAM and your apps only use 1GB, Linux will use the remaining 3GB _entirely_ for the Page Cache. This is why "Free RAM" often looks low in Linux—it's actually "Borrowed for Cache."

- **Q: What happens during a Read Request?**
	1. **Check:** The kernel looks in the Page Cache.
	2. **Hit:** If the data is there, return it immediately (Zero Disk I/O).
	3. **Miss:** If not, read from the disk, store a copy in the Page Cache, and _then_ return it.
	- Analogy:
		- **Disk:** The Public Library (Slow, have to drive there).
		- **Page Cache:** Your Backpack (Fast, instant access).
		- When you need a book, check your backpack first. If it's not there, go to the library, but _put a copy in your backpack_ for next time.

#### Write Caching

- Reading is easy (just copy). Writing is hard because you have to ensure data actually hits the disk eventually so it isn't lost if the power fails.

- **Q: What are the three strategies for Write Caching?**
	1. **No-Write:** The cache is ignored. Writes go straight to disk. (Slow, rarely used).
	2. **Write-Through:** Update the Cache AND the Disk simultaneously.
	    - _Pros:_ Safe. The disk is always up to date.
	    - _Cons:_ Slow. You are limited by the speed of the disk for every write.
	3. **Write-Back (Linux Choice):** Update the Cache, mark the page as Dirty, and lie to the application saying "Done!".
	    - Later, in the background, the kernel writes the dirty page to the disk (this is called **Writeback**).

- **Q: Why does Linux prefer Write-Back?**
	- **Coalescing. (consolidate)**
		- If you write to the same file 10 times in 1 second:
		- **Write-Through:** Triggers 10 separate disk I/O operations.
		- **Write-Back:** Updates the RAM 10 times. The background process runs once and writes the final result to the disk _one_ time.

- **Q: What does "Dirty" mean in this context?**
	- It implies **Unsynchronized**.
		- **Clean Page:** The data in RAM matches the data on the disk exactly.
		- **Dirty Page:** The data in RAM is newer than the data on the disk. The disk is "stale" until the kernel performs writeback.


### Cache Eviction and the Two-List Strategy

- RAM is finite. Eventually, the cache fills up. We need an algorithm to decide who gets kicked out (evicted).
- The standard algorithm in computer science is **LRU (Least Recently Used)**, but Linux improves upon it.

- **Q: What is the failure of standard LRU?**
	- The "One-Time Scan" problem.
		- Imagine you have a perfect cache of useful files.
		- Suddenly, you run `find / -name "lost_file"`. This reads _every_ file on the disk once.
		- A pure LRU would see these new files as "Most Recently Used" and flush out all your actually useful hot data to make room for files you will never look at again. This is called **Thrashing**.

- **Q: How does Linux solve this (The Two-List Strategy)?**
	- Linux splits the LRU list into two linked lists:
		1. **Active List:** "Hot" data. Not eligible for eviction.
		2. **Inactive List:** "New" or "Cold" data. Candidates for eviction.
	- **The Rules:**
	    - When a page is read from disk, it lands in the **Inactive List** first.
	    - If (and only if) it is accessed _again_ while still in the Inactive List, it gets promoted to the **Active List**.
	- _Result:_ That "One-Time Scan" fills the Inactive List and then falls off the end. It never touches your hot Active List.

- **Visualization: The Two-List Flow**
  ```mermaid
	graph TD
    NewData[New Data Read from Disk] --> InactiveHead
    
    subgraph "Inactive List (Cold)"
        InactiveHead[Head] --> InactiveTail[Tail]
    end
    
    subgraph "Active List (Hot)"
        ActiveHead[Head] --> ActiveTail[Tail]
    end
    
    InactiveTail -- Needs Space? --> Evict[EVICT / DELETE]
    
    InactiveHead -- Accessed Again? --> Promote[Promote to Active List]
    
    ActiveTail -- Too Big? --> Demote[Demote to Inactive List]
    
    style Evict fill:#f9f,stroke:#333
    style Promote fill:#9f9,stroke:#333
  ```


### Modern Usage (Kernel v6.x)
This is a critical update especially for Android or resource-constrained ARM devices.

- **MGLRU (Multi-Gen LRU) - The New Standard (v6.1+):**
	- **Old Kernel:** Describes the "Two-List Strategy" (Active/Inactive).
	- **Modern Kernel:** While Two-List is still there, **Google** developed MGLRU because the Two-List strategy was struggling on systems with limited RAM (like phones).
		- MGLRU uses "Generations" (more than just two lists) and is much smarter at identifying true "working sets" vs "garbage."
		- _Impact:_ Enabling MGLRU often reduces CPU usage during low-memory situations by 50% on Android.

- **Folios (The "Page" Replacement):**
	- **Old Kernel:** Talks about caching "Pages" (4KB).
	- **Modern Kernel:** - We are moving toward **Folios**.
		- A Folio is a compound page (a group of physically contiguous pages) treated as a single unit by the cache.
		- **_Why?_** Managing 1GB of cache in 4KB chunks is millions of operations. Managing it in 2MB chunks (Folios) is much faster.

- **Writeback Throttling:**
	- Modern NVMe drives are so fast that they can sometimes accept data faster than the kernel can organize it! The writeback logic is now much more complex (cgroup aware) to prevent one heavy writer (like a logger) from stalling the whole system.


### The Linux Page Cache

- In this section, we explore the data structures and kernel facilities that maintain Linux’s page cache.

#### The `address_space` Object (The "Master Blueprint")

- We learned in Chapter 15 about `vm_area_struct` (VMA), which represents a virtual memory area for a single process.
- But what happens if three different processes, all perform `mmap()` or read the exact same file (like `libc.so`)?We don't want to load that file into physical RAM three times!
- The kernel needs a single, unified structure to represent the _physical_ pages of that file in RAM. That structure is the `address_space` object.

- **Q: What is the `address_space` object?**
	- Despite the confusing name, it has nothing to do with a process's virtual address space. The author notes a better name would be `page_cache_entity`.
	- It is the central data structure that links a physical file (the `inode`) to the pages of RAM that currently hold that file's data.
	- _Analogy:_ Think of a popular book.
		- The `inode` is the ISBN number (identifies the book itself).
		- The **`vm_area_struct` (VMA)** is a library card held by a specific person (a process's permission to read it).
		- The **`address_space`** is the physical bookshelf in the library where the physical copies of the book's pages are stored.

- **Visualization: Virtual vs. Physical Mapping**
  ```mermaid
	graph TD
    subgraph "Process A (User Space)"
        VMA1["vm_area_struct (VMA) <br/> Virtual Address: 0x1000"]
    end
    
    subgraph "Process B (User Space)"
        VMA2["vm_area_struct (VMA) <br/> Virtual Address: 0x8000"]
    end
    
    subgraph "Kernel Page Cache (Physical RAM)"
        AS{"address_space <br/> (The Physical File Entity)"}
        Page1["Physical RAM Page 1"]
        Page2["Physical RAM Page 2"]
    end

    VMA1 -- Maps to --> AS
    VMA2 -- Maps to --> AS
    AS -- Manages --> Page1
    AS -- Manages --> Page2
    
    note["Notice: Many VMAs map to ONE address_space. <br/> This saves massive amounts of RAM."]
  ```



#### `address_space` Operations (`a_ops`)

- The Page Cache is generic. It doesn't care if a file comes from an EXT4 filesystem on a hard drive, a FAT32 USB stick, or an embedded UBIFS flash chip.
- To achieve this, the `address_space` object uses an operations table (`a_ops`) of function pointers.

- **Q: What are the key functions in `address_space_operations`?**
	- These functions tell the kernel exactly _how_ to talk to the backing hardware for this specific file.
		- `readpage()`: The most important one. "Hey hardware, please go read block X from the disk and put it into this blank RAM page."
		- `writepage()`: "Hey hardware, take this dirty RAM page and write it back to the disk."
		- `set_page_dirty()`: Marks a page as modified (so the kernel knows it needs to be written out later).


#### How Page I/O Works (The Sequence)

- When a user calls `read()`, the kernel doesn't immediately talk to the disk. It goes through the Page Cache first.

- **Q: What is the step-by-step process of a Read?**
	1. The kernel calculates the `address_space` mapping and the `index` (the page offset within the file).
	2. It calls `find_get_page(mapping, index)`.
	3. **If it finds it (Cache Hit):** Great! Return the data immediately.
	4. **If it doesn't (Cache Miss):**
		- Allocate a new, blank physical page in RAM.
		- Add this blank page to the Page Cache.
		- Call `mapping->a_ops->readpage()` to command the specific filesystem/driver to wake up and fill this blank page with real data from the disk.

	- **Visualization: The Read Sequence**
	  ```mermaid
		sequenceDiagram
	    participant User as User App
	    participant Kernel as VFS (Kernel)
	    participant Cache as Page Cache
	    participant Disk as Hardware/Disk
	
	    User->>Kernel: read(file)
	    Kernel->>Cache: find_get_page()
	    
	    alt Cache Hit (Fast Path)
	        Cache-->>Kernel: Returns Data from RAM
	    else Cache Miss (Slow Path)
	        Cache-->>Kernel: Returns NULL
	        Kernel->>Cache: Allocate Blank Page
	        Kernel->>Disk: a_ops->readpage()
	        Note over Disk,Cache: Hardware I/O happens here...
	        Disk-->>Cache: Fills Blank Page with Data
	        Cache-->>Kernel: Returns Data from RAM
	    end
	    
	    Kernel-->>User: Copies Data to User Buffer
	  ```


#### Searching the Cache: The Radix Tree

- If you have 4GB of RAM acting as a cache, you might have _one million_ pages stored in it.
- When a program asks for a specific page, the kernel must find it **instantly**. If searching the cache takes too long, it defeats the purpose of having a cache!

- **Q: How does the kernel search the `address_space`?**
	- Using a **Radix Tree** (stored in the `page_tree` field).
		- A Radix Tree is a highly optimized search tree where the "key" (in this case, the page offset index) is used to traverse the branches.
		- Given an offset, the kernel can traverse the Radix Tree and find the corresponding physical page in a tiny handful of CPU cycles.

- **Q: Why don't we just use a Hash Table? (The Old Way)**
	- Prior to Kernel 2.6, Linux _did_ use a massive global Hash Table for all pages. It was terrible for a few reasons:
		1. **Global Lock Contention:** One massive lock protected the whole hash. In multi-core systems, CPUs were constantly waiting in line to search the hash.
		2. **Bloat:** It contained _every_ page on the system.
		3. **Slow Misses:** If a page wasn't there, iterating down a hash collision chain took too long.

	- By giving every single `address_space` its _own_ private Radix Tree, the lock contention problem disappeared, and performance skyrocketed.


#### Modern Usage (Kernel v6.x)
Because the kernel is always evolving, the memory management subsystem has seen massive upgrades since this book was written. Here is what you need to know for modern embedded systems:

- **Goodbye Radix Tree, Hello XArray:**
	- **Old Kernel:** Mentions the Radix Tree as the absolute standard.
	- **Modern Kernel (`v4.20+`/`v5.x+`):** The kernel developers entirely replaced the Radix Tree in the page cache with a new, even faster structure called the XArray (eXtensible Array).
	- **Why?**
		- The XArray provides an even better, lock-less (RCU-based) API. It handles huge pages seamlessly and is much more cache-line friendly for modern ARM and x86 processors. If you look at modern kernel code, you will see `xa_load()` instead of `radix_tree_lookup()`.

- **`readpage()` vs `read_folio()`:**
	- **Old Kernel:** Talks about `a_ops->readpage()`.
	- **Modern Kernel (`v5.18+`)**: The concept of a single "Page" is being phased out in favor of "Folios" (which can represent a single page or a compound "Huge" page natively). You will now see `a_ops->read_folio()` in modern filesystems.

- **Non-Volatile Memory (NVDIMM):**
	- In modern systems, sometimes the storage medium is as fast as RAM (e.g., persistent memory). In these cases, the `address_space` can be configured to bypass the page cache entirely (using DAX - Direct Access) because copying from ultra-fast storage to RAM is just a waste of CPU cycles!


### The Buffer Cache

This section is all about how the kernel cleans up after itself. Since Linux loves to keep modified ("dirty") data in RAM to make the system feel fast, it eventually needs a reliable way to safely write that data back to the physical disk.

- In older kernels (before version 2.4), Linux had two separate caches: the **Page Cache** (which cached file pages) and the **Buffer Cache** (which cached individual disk blocks).
- This caused a massive issue: **Double Caching**. A single piece of data could exist in both caches at the same time, wasting precious RAM and CPU cycles trying to keep them synchronized.
- **The Modern Solution:** The two caches were unified. Today, there is only one cache: the Page Cache. The "buffer cache" still exists in name, but it simply acts as a descriptor that maps physical disk blocks to the pages already sitting in the Page Cache.


### The Flusher Threads (The Garbage Collectors)

- When an application writes to a file, the Page Cache is updated, and the page is marked as **Dirty** (meaning RAM is newer than the disk).
- The kernel uses a group of background worker threads called **Flusher Threads** to write these dirty pages back to the disk.
- *Analogy for Flusher Threads:* Think of dirty pages like trash bins in a house, and the flusher threads are the garbage collectors. They will only take out the trash in three specific situations:

- **Q: What are the three situations that trigger a dirty page writeback?**
	1. **Low Memory (The bin is overflowing):** When free system memory drops below a certain threshold (defined by `dirty_background_ratio`), the system needs RAM back urgently. The threads wake up and flush data to disk so those pages can be marked clean and evicted.
	2. **Old Data (The trash is getting smelly):** Even if there is plenty of free RAM, we don't want dirty data sitting in volatile memory forever (a power outage would cause data loss). If a dirty page gets older than a specific time limit (defined by `dirty_expire_interval`), the threads wake up and flush it.
	3. **On Demand (The boss said do it now):** A user process explicitly calls the `sync()` or `fsync()` system calls, forcing the kernel to write the data to disk immediately.

- **Visualization: Flusher Thread Triggers**
  ```mermaid
		graph TD
		Start((Dirty Pages in RAM))
		
		Cond1{"Free RAM <br/> too low?"}
		Cond2{"Page age > <br/> expire limit?"}
		Cond3{"App called <br/> sync()?"}
		
		Action[Wake Up Flusher Threads]
		Disk[(Physical Disk)]
		
		Start --> Cond1
		Start --> Cond2
		Start --> Cond3
		
		Cond1 -- Yes --> Action
		Cond2 -- Yes --> Action
		Cond3 -- Yes --> Action
		
		Action -- Writeback --> Disk
		
		style Action fill:#ff9999,stroke:#333
		style Disk fill:#99ccff,stroke:#333
  ```

#### Laptop Mode (Crucial for Embedded/Battery Devices)

- Spinning up a hard drive (or waking up a sleeping storage controller in an embedded ARM device) consumes a huge amount of battery power.
- **Laptop Mode** is a special strategy to optimize battery life by keeping the disk asleep for as long as possible.

- **Q: How does Laptop Mode change writeback behavior?**
	- It acts as a "piggyback" system. (Piggybacking is a technique where the receiver delays sending an acknowledgement (ACK) and attaches it to its next outgoing data packet.)
		- Normally, flusher threads wake up periodically to write old data. This forces the disk to wake up constantly.
		- In Laptop Mode, the timers are delayed significantly (e.g., up to 10 minutes). However, if the disk has to spin up for any other reason (like the user opening a new app), the flusher threads instantly piggyback on that wake-up and flush all dirty buffers at once.
	- Analogy: You don't make a special trip to the garbage chute. You just leave the trash by the door and only take it out when you are already leaving the apartment for work.

#### The Evolution of Writeback Threads (Avoiding Congestion)
- Historically, the Linux kernel went through several iterations of background threads to handle this job: `bdflush` (pre-2.6) → `pdflush` (2.6) → **Flusher Threads** (2.6.32+).

- **Q: Why did the kernel move to the modern "Flusher Threads" model?**
	- To avoid **Congestion bottlenecks**.
		- In the older `pdflush` model, a pool of threads was shared across all disks globally. If one disk was very slow (congested), a thread would get stuck waiting for it, potentially stalling the whole system.
		- **The Modern Solution (Per-Spindle Flushing):** The current Flusher Thread model assigns dedicated threads to each specific block device (each disk/spindle).
		- *Why this matters:* If your system has a super-fast NVMe drive and a very slow USB thumb drive, the slow USB drive has its own dedicated flusher thread. It can take all the time it needs without blocking the threads assigned to your fast NVMe drive.

#### Modern Usage (Kernel v6.x)
Because storage hardware has changed drastically from spinning hard drives to solid-state flash memory, the kernel's writeback mechanisms have evolved significantly since this book was published.

- **From Custom Threads to Workqueues (`kworker`):**
	- **Old Kernel:** Mentions dedicated "Flusher Threads" (named things like `flush-8:0`).
	- **Modern Kernel:** The kernel developers realized that writing custom threading models was inefficient. Today, the kernel uses its highly optimized, generic **Workqueue** subsystem. If you run `ps aux | grep kworker` on your Ubuntu or embedded ARM board, you will see many threads. These generic worker threads dynamically pick up the "writeback" jobs (`bdi_writeback`) when needed, rather than having threads sit idle.

- **Cgroup-Aware Writeback (Containers/Docker):**
	- **Old Kernel:** Describes global memory thresholds (e.g., "if the *whole system* reaches 10% dirty memory, flush it").
	- **Modern Kernel:** With the rise of containers (like Docker) and Android (which isolates apps), the kernel now features **cgroup-aware writeback**. This means the kernel can say, "Container A is generating too much dirty data, throttle Container A's writes," without affecting Container B or the rest of the system.

- **Laptop Mode vs. Modern Embedded Flash (eMMC/UFS/NVMe):**
	- **Old Kernel:** "Laptop mode" was primarily designed to stop the physical platters of a hard drive from spinning to save battery.
	- **Modern Embedded Systems:** ARM devices don't use spinning disks; they use eMMC, UFS, or NVMe solid-state storage. These chips don't "spin up," but they *do* have different power states. Modern kernels rely less on the old "Laptop mode" and more on **Runtime Power Management (Runtime PM)** and hardware-level sleep states (like NVMe APST - Autonomous Power State Transitions). The kernel aggressively puts the flash controller chip to sleep between IO bursts, achieving the same battery-saving goal but at a much lower, hardware-aware level.

- **Multi-Queue Block I/O (`blk-mq`):**
	- **Old Kernel:** Assumes a single queue for disk I/O.
	- **Modern Kernel:** To handle modern NVMe drives that can do millions of operations per second, the kernel introduced `blk-mq`. Instead of all CPUs fighting over one lock to flush their dirty pages, each CPU core has its own dedicated software queue to push dirty pages down to the hardware concurrently.


## **Quick Recall**

- **Q: What is the main purpose of the Page Cache?**
	- To store data in RAM that has been read from or written to a physical disk. This prevents the kernel from having to do slow disk I/O for data it has already accessed recently, making the system feel fast.

- **Q: What is the difference between the Page Cache and the Buffer Cache?** 
	- Historically, the Page Cache cached virtual file pages, while the Buffer Cache cached physical disk blocks, leading to wasted RAM (double caching). In modern Linux, they are unified; the Buffer Cache just points directly into the Page Cache.

- **Q: What is the `address_space` object?**
	- It is the "master blueprint" that maps a physical file (the `inode`) to the physical pages in RAM that currently cache its data. Multiple processes reading the same file will all point to this single `address_space` object.

- **Q: What is the purpose of `address_space_operations` (`a_ops`)?**
	- It is a table of function pointers (like `readpage` and `writepage`) that tells the generic Page Cache exactly *how* to talk to the specific underlying file system or storage hardware.

- **Q: What happens during a Page Cache "Hit" vs "Miss"?**
	- On a Hit, the kernel finds the data in RAM and returns it instantly. On a Miss, it allocates a blank RAM page, calls `a_ops->readpage()` to fetch the data from the physical disk, fills the RAM page, and then returns it.

- **Q: How does the kernel quickly find a specific page in the cache?**
	- By using an optimized search tree. The book mentions the **Radix Tree**, but modern kernels upgraded this to a much faster, lock-less data structure called the **XArray**.

- **Q: Why did Linux stop using a Global Hash Table to track cached pages?**
	- A global hash table caused massive "lock contention" (all CPU cores fighting to use it at the same time) and became extremely slow to search as RAM sizes grew. Giving each `address_space` its own Radix Tree/XArray fixed this.

- **Q: What exactly makes a page "Dirty"?**
	- When a user space application modifies or writes new data to a file, the kernel updates the page in RAM first. This page is now "dirty" because the RAM has newer data than the physical disk.

- **Q: What are Flusher Threads (or `kworker` threads) and what do they do?**
	- They are background worker threads that act as garbage collectors. Their job is to safely write (flush) dirty pages from volatile RAM back to the non-volatile physical disk.

- **Q: When do these Flusher Threads wake up to do their job?**
	-  In three main scenarios:
		1. When free RAM drops too low.
		2. When a dirty page gets too old.
		3. When a user explicitly forces it (using sync()). *Note: Laptop Mode tweaks these rules to delay wake-ups and save battery life.*

## **Hands-on Ideas**

The Page Cache works the exact same way on your x64 Ubuntu machine and your ARM embedded boards. However, embedded environments (which often use `BusyBox` and Flash storage) require slightly different approaches. Here are five ways to test this on both:

- **Watch the Page Cache Grow and Shrink**: Look at your system's memory to see the Page Cache in action.
	- **Ubuntu:** Run `free -h` and look at the `buff/cache` column.
	- **ARM Embedded:** BusyBox often doesn't support the `-h` (human-readable) flag. Run `free -m` (megabytes) instead.
	- **Action:** Read a large file to memory (`cat /var/log/syslog > /dev/null`), check `free -m` to see the cache grow, then force the kernel to drop it: `echo 1 > /proc/sys/vm/drop_caches`. Check `free -m` again to see the memory freed.

- **Inspect Your Dirty Page Thresholds**: The kernel decides when to wake up flusher threads based on percentages of total RAM.
	- **Action:**  Run `cat /proc/sys/vm/dirty_background_ratio` and `cat /proc/sys/vm/dirty_ratio`.
	- *Embedded Note:* On ARM devices with very low RAM (e.g., 256MB), kernel developers often tweak these values to be lower. You don't want 20% of a tiny RAM chip getting clogged with dirty pages before writeback starts!

- **See the "Flusher Threads" in Action**: Modern Linux uses generic `kworker` threads for writeback rather than dedicated "flusher" threads.
	- **Action:** 
		- Run `ps aux | grep bdi_writeback` (or use `top` / `htop`).
		- You will see the generic worker threads dynamically picking up the writeback jobs. This behaves exactly the same on ARM and x64 architectures.

- **Generate "Dirty" Pages and Flush Them:** You can artificially create dirty pages and manually trigger a writeback.
	- **Ubuntu:** Run `dd if=/dev/zero of=test_file.img bs=1M count=1000` (Creates a 1GB file).
	- **ARM Embedded:** Be *careful with eMMC/NAND flash wear and tear!* Use a much smaller file: `dd if=/dev/zero of=test_file.img bs=1M count=50` (Creates a 50MB file).
	- **Action:** Immediately run `cat /proc/meminfo | grep Dirty`. You will see dirty memory spiked. Type `sync` in the terminal to manually force the writeback to the eMMC/Disk. Check `meminfo` again, and the dirty pages will have dropped to near zero.

- **Monitor Disk I/O vs Page Cache in Real-Time:** You can watch the kernel bypass the physical storage hardware using the `vmstat` (Virtual Memory Statistics) tool.
	- **Action:**
		- Run `vmstat 1` in your terminal to print stats every second.
		- Look at `bo` (Blocks Out to storage) and `bi` (Blocks In from storage). Read a file once, and `bi` spikes. Read it a second time, and `bi` stays at 0 because the ARM processor fetched it entirely from the RAM (Page Cache).
		- *Embedded Note:* `vmstat` is usually included in standard `BusyBox` builds, but if your ARM board has a highly stripped-down filesystem, you might need to rely on cat `/proc/diskstats` instead.