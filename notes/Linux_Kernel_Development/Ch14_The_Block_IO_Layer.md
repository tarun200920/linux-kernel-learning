
# Chapter 14: [The Block I/O Layer]

## **Summary**

- As an embedded developer, you’ve likely worked with Character Drivers (UART, I2C, SPI) where data flows like water in a pipe. The Block Layer is a different beast. It is the subsystem responsible for managing hard drives, SD cards, and NAND flash.
- This is where the kernel focuses on **Performance**. If your UART is slow, your console logs appear slowly. If your Block Layer is slow, the entire system feels sluggish, boot time increases, and apps freeze.

- The Block I/O layer is the subsystem responsible for managing block devices (Hard disks, SD cards, Flash).
- Unlike character devices (UART, Keyboard) which are sequential streams, block devices allow **random access**.

### Character vs Block Devices

- **The Core Concept:** The Kernel distinguishes devices based on _how_ we access data from them.

1. **Character Devices (The Stream):**
       - Accessed sequentially (one byte after another).
    - **No "Seeking"**: You cannot ask a keyboard, "Give me the keystroke you sent 5 minutes ago." You consume the data as it arrives.
    - _Examples:_ UART, Keyboard, Mouse, Sound Card.

2. **Block Devices (The Random Access):**
       - Accessed randomly. You can jump to the beginning, middle, or end instantly.
    - **Seeking is Key**: You mount filesystems on them.
    - _Examples:_ Hard Disk, SD Card, eMMC, USB Drive.

- **Analogy: The Radio vs. The Book**
	- **Character Device (Radio):** You listen to the song currently playing. You cannot "rewind" the live broadcast. You just accept the stream.
	- **Block Device (Book):** You can read page 1, then immediately flip to page 300, then back to page 50. You access "blocks" of text in any order you want.


### Anatomy of a Block Device

- **The Sector (Hardware Reality)**
	- This is the smallest unit the **Hardware** can handle.
	- The disk controller literally cannot read/write anything smaller than a sector.
	- **Standard Size:** Traditionally **512 Bytes**. (Modern SSDs often use 4096 bytes, but emulate 512).

- **The Block (Software Abstraction)**
	- This is the smallest unit the **Filesystem** (Software) wants to manage.
	- The Filesystem (like ext4) groups several sectors together to make management easier.
	- **Standard Size:** Usually **1KB (1024)** or **4KB (4096)**.

- **The Golden Rules:**
	1. **Block Size $\ge$ Sector Size:** You can't have a software block smaller than a hardware sector.
	2. **Block Size $\le$ Page Size:** A block must fit inside a RAM Page (usually 4KB). You cannot have an 8KB block if your RAM is managed in 4KB chunks.

- **Visualization: The Hierarchy**
  ```mermaid
	graph TD
    subgraph "Hardware (Disk)"
    S1[Sector 512B] --- S2[Sector 512B]
    end

    subgraph "Software (Filesystem)"
    Block[Block 1KB]
    end

    subgraph "Memory (RAM)"
    Page[Page Frame 4KB]
    end

    S1 --> Block
    S2 --> Block
    
    Block -- fits inside --> Page
    
    note[Note: 1 Block = 2 Sectors in this example]
  ```


#### Modern Usage:

- **4K Native Drives:** In the past, all sectors were 512 bytes. Modern SSDs and high-capacity HDDs now use **4096-byte (4K)** physical sectors. The kernel now supports logical block sizes fitting these larger physical sectors seamlessly.
- **Zoned Storage:** Modern kernel block layers have added support for "Zoned Block Devices" (SMR drives, NVMe ZNS) where you must write sequentially within a zone, blurring the line between random access and sequential streams.



### Buffers and Buffer Heads (`struct buffer_head`)

- When a block is read from the disk, it is stored in a specific region of RAM.
- This region needs a descriptor to tell the kernel which disk sector corresponds to this data in RAM.

- **Q: What is a Buffer?**
	- A Buffer is simply the raw memory area (RAM) that holds the data of exactly one disk block.
	- If the block size is 4KB, the buffer is a 4KB array in memory.

- **Q: What is a `buffer_head`?**
	- This is the descriptor (the metadata) associated with the buffer.
	- The raw data doesn't know where it came from. The `buffer_head` structure tells the kernel: "This data belongs to Block #100 on the SD Card."
	- It serves as the mapping interface between the physical disk and the RAM buffer.

- **Q: What are the key fields in `struct buffer_head`?**
	- `b_page`: Points to the physical RAM page where the data sits.
	- `b_data`: Points to the specific offset within that page (the actual data start).
	- `b_blocknr`: The logical block number on the disk.
	- `b_state`: Flags indicating the status (e.g., `BH_Dirty` means data needs to be written back to disk, `BH_Uptodate` means data is valid).

- **Q: What is the major limitation of using Buffer Heads?**
	- Scalability and Memory Overhead.
		- A `buffer_head` describes exactly one block.
		- If you are doing a large I/O operation (e.g., writing a 10 MB file) with 1 KB blocks, the kernel has to create and manage 10,000 `buffer_head` structures.
		- Breaking a large I/O request into thousands of tiny metadata structures hurts performance and clogs the memory with headers.


#### Modern Usage:

- **Buffer Heads are "Deprecated" for Data:**
	- **Old Kernel:** Says `buffer_head` is inefficient but still discusses it heavily.
	- **Modern Kernel:** We _hate_ `buffer_head`. We try to avoid it at all costs for actual file data. We mostly use it only for reading "Metadata" (like reading an Inode or a Superblock). All heavy data lifting is done by the `bio` struct (Block I/O).

- **Sector Sizes (4K Native):**
	- **Old Kernel:** Assumes 512-byte sectors.
	- **Modern HW:** Most modern NVMe drives and huge HDDs use 4K Native sectors. The kernel now has to handle cases where the hardware sector is the same size as the memory page.

- **Direct I/O:**
	- Modern databases (like PostgreSQL or Oracle) often bypass the Kernel's buffer cache entirely and talk straight to the Block Layer using `O_DIRECT`. This skips the whole `buffer_head` overhead completely.


### The `bio` Structure

- While `buffer_head` tracks a single block to a single page, the `bio` structure is designed for **High Performance I/O**.
- It represents an **active, in-flight** block I/O operation (read or write).
- Its superpower is **Scatter-Gather I/O**: It can gather data from many different fragmented locations in RAM and write them to a single contiguous chunk on the disk.

- **Q: What exactly is the `struct bio`?**
	- It is the fundamental container for block I/O in the Linux Kernel.
	- When the VFS wants to read/write data, it creates a `bio`.
	- It contains the direction (Read/Write), the target sector on the disk, and a list of memory pages involved in the transfer.

- **Q: What is Scatter-Gather (Vectored) I/O?**
	- This is a method where a single transfer reads/writes data from multiple non-adjacent memory addresses.
	- *Without Scatter-Gather*: You must copy all small data chunks into one big contiguous buffer before sending it to the disk. (Slow, CPU intensive).
	- _With Scatter-Gather (`bio`):_ You give the disk controller a list of memory addresses: "Take 1 KB from address A, 2 KB from address B, and write them sequentially to Disk Sector 100."
	- _Analogy:_ Instead of repacking all your items into one giant suitcase before the flight, you just tell the airline, "Take these 5 small bags and treat them as one shipment."

- **Q: What is `struct bio_vec`?**
	- The `bio` structure holds an array of `bio_vec` structures. Each `bio_vec` describes one memory segment. It has three fields:
		1. `bv_page`: The physical page.
		2. `bv_len`: The length of the data in bytes.
		3. `bv_offset`: The offset within that page.
	- **Visual:** `bio` is the "Invoice", `bio_vec` are the "Line Items" on that invoice.

- **Q: What is `bi_idx`?**
	- This is the "Current Cursor".
	- It points to the `bio_vec` that the hardware is currently processing.
	- As the driver finishes each segment, it increments `bi_idx`.
	- _Use Case:_ This allows a driver to stop halfway through a request, or for RAID drivers to split a single `bio` across multiple hard drives effortlessly.

- **Visualization: The Bio Structure**
  
  ```mermaid
	graph TD
    Bio[struct bio]
    
    subgraph "The Vector Array (bi_io_vec)"
        Vec1[bio_vec 0]
        Vec2[bio_vec 1]
        Vec3[bio_vec 2]
    end
    
    subgraph "Physical RAM"
        PageA[Page Frame A]
        PageB[Page Frame B]
    end

    Bio -- points to list --> Vec1
    Vec1 --- Vec2 --- Vec3
    
    Vec1 -- points to --> PageA
    Vec2 -- points to --> PageB
    Vec3 -- points to --> PageA
    
    note[The Disk Controller reads Page A + Page B + Page A and writes them continuously to the Disk.]
  ```


#### The Old Versus the New

- The transition from purely `buffer_head` (2.4 kernel) to `bio` (2.6 kernel) was a major revolution. This section compares why the change was necessary.

- **Q: Why is `bio` better than `buffer_head` for data transfer?**
	1. **Efficiency:** `bio` can represent 1 MB of I/O with a single structure pointing to a list of pages. `buffer_head` would require thousands of structures linked together.
	2. **HighMem Support:** `bio` deals with physical pages directly (`struct page *`), so it can easily handle memory larger than 1 GB (HighMem) without needing permanent mappings in the kernel.
	3. **Direct I/O:** `bio` makes it easy for applications (like Databases) to bypass the OS cache and write directly to disk.

- **Q: Is `buffer_head` dead?**
	- No, but it has been demoted.
		- It is still used for mapping a single disk block to a page.
		- It is essentially just a "descriptor" now, mostly used for filesystem metadata (inodes, superblocks). It is no longer the primary vehicle for moving file data.


#### Modern Usage:

This area has evolved massively to support high-speed NVMe SSDs.

- **Immutable Bio Vecs (`bvec_iter`):**
	- **Old Kernel:** Describes modifying `bi_idx` inside the `bio` structure to track progress.
	- **Modern Kernel:**
		- Modifying the `bio` structure during I/O is dangerous in multi-core systems.
		- Modern kernels use a separate iterator structure called `struct bvec_iter`.
		- The `bio` itself remains mostly constant, while the iterator tracks the position. This makes the code safer and simpler for stacking drivers (like encryption/DM-crypt).

- **`bio` Splitting:**
	- Modern kernels have very sophisticated logic to split `bio` structures if they are too large for the hardware to handle in one go, or if they cross boundary limits (like 4GB boundaries in some DMA controllers).


### Request Queues

- Block devices are significantly slower than the CPU.
- If we sent requests to the disk one by one immediately, performance would be terrible.
- The kernel uses a **Queue** to store pending requests, giving it a chance to optimize the order (e.g., grouping reads that are close to each other).

- **Q: What is a `request_queue`?**
	- It is a doubly linked list of pending I/O requests.
		- Defined as `struct request_queue`.
		- Every block device (like `/dev/sda`) has one queue.

- **Q: What is a `struct request`?**
	- This represents a specific job in the queue.
		- **Crucial distinction:** A `request` is NOT the same as a `bio`.
		- A single `request` can contain multiple `bio` structures!
		- _Why?_ If you submit a `bio` to read Sector 100, and I submit a `bio` to read Sector 101, the kernel sees they are adjacent. It merges them into a **single** `request` to read Sectors 100-101. This reduces overhead.

- **Visualization: The Hierarchy**
  ```mermaid
	  graph LR
    subgraph "The Queue"
        RQ[Request Queue]
    end

    subgraph "The Requests"
        Req1[Request 1]
        Req2[Request 2]
    end

    subgraph "The Bios"
        Bio1[Bio A]
        Bio2[Bio B]
        Bio3[Bio C]
    end

    RQ --> Req1
    RQ --> Req2
    
    Req1 -- contains --> Bio1
    Req1 -- contains --> Bio2
    
    Req2 -- contains --> Bio3
    
    note[Bio A and Bio B were merged into one Request because they access adjacent disk sectors.]
  ```


#### Modern Usage:

- **Multi-Queue Block Layer (`blk-mq`):**
	- **Old Kernel:**
		- Describes a single `request_queue` protected by a single lock. This was fine for Hard Drives (100 IOPS).
	- **Modern Kernel:**
		- NVMe drives can do 1,000,000 IOPS. A single lock is a bottleneck. Linux now uses **Multi-Queue (`blk-mq`)**.
		- There is one software queue **per CPU core**.
		- These map to multiple hardware queues on the NVMe device.
		- This eliminates locking contention and allows massive parallelism.


### I/O Schedulers

- Hard Disk Drives (HDDs) are mechanical. Moving the magnetic head physically (seeking) takes milliseconds, which is an eternity in CPU time.
- If requests are sent to the disk in the order they arrive (FIFO), the disk head might jump wildly from the inner track to the outer track and back. This destroys performance.
- The I/O Scheduler optimizes the order of requests to minimize head movement.

- **Q: What is the main job of an I/O Scheduler?**
	- To Virtualize the block device. Just as the Process Scheduler shares the CPU among processes, the I/O Scheduler shares the Disk among multiple block requests. Its goal is **Global Throughput** (getting the most work done) while avoiding **Starvation** (making sure every request eventually gets served).

- **Q: What is Merging?**
	- Combining two separate requests into one.
		- If you ask to read "Sector 10", and the next request is for "Sector 11", the scheduler merges them into a single request for "Sectors 10-11".
		- _Benefit:_ This reduces the number of commands sent to the hardware, lowering overhead.

- **Q: What is Sorting (The Elevator Algorithm)?**
	- Reordering requests based on physical disk location.
		- If requests arrive for sectors: 10, 500, 12, 502.
		- The Scheduler reorders them to: 10, 12, 500, 502.
		- _Analogy:_ An elevator doesn't go Floor 1 -> Floor 10 -> Floor 2 -> Floor 9. It goes 1 -> 2 -> 9 -> 10.

#### The Linus Elevator

- This was the **default scheduler** in Kernel 2.4.
- It was simple but effective for basic workloads.

- **Q: How did the Linus Elevator work?**
	- It maintained a sorted queue. When a new request arrived:
		1. **Check for Merging:** Can we attach it to an existing request? If yes, merge.
		2. **Insertion:** If not, insert it into the queue at the correct sorted position (Sector wise).
		3. **Age Check:** If an old request has been waiting too long, stop sorting and just put the new request at the end to prevent starvation.

- **Q: What was the flaw?**
	- **Starvation**. Despite the age check, if you constantly hammered one area of the disk (e.g., Sector 1000), a request for a distant area (Sector 10) might wait a very long time. This created "latency spikes."


#### The Deadline I/O Scheduler

- Designed to solve the starvation problem of the Linus Elevator.
- It introduces the concept of **Latency Guarantees**: "I promise to serve this request within X milliseconds."

- **Q: How does the Deadline Scheduler structure its queues?**
	- It uses Three Queues:
		1. **Sorted Queue:** (Standard Elevator sorting).
		2. **Read FIFO:** List of read requests sorted by arrival time.
		3. **Write FIFO:** List of write requests sorted by arrival time.

- **Q: Why separate Reads and Writes?**
	- Because **Reads are more important**.
		- When an app Reads, it usually stops and waits (Blocks) until data arrives. The user sees a frozen screen.
		- When an app Writes, the kernel buffers it in RAM and returns immediately. The app doesn't wait for the disk.
		- _Policy:_ Read deadline is **500 ms**. Write deadline is **5 seconds**.

- **Q: How does it service requests?**
	- It mostly pulls from the **Sorted Queue** to keep the disk efficient.
		- _However_, if the head of the Read FIFO or Write FIFO is about to expire (cross the deadline), it stops sorting and immediately services that expiring request.


#### The Anticipatory I/O Scheduler

- Designed to fix a specific inefficiency in the Deadline scheduler called "Deceptive Idleness."
- It is optimized for total throughput on spinning disks.

- **Q: What is the "Deceptive Idleness" problem?**
	- Imagine an app reads a block. The scheduler serves it. The app processes the data for 1ms, then asks for the _next_ block.
		- In that 1ms gap, the scheduler thinks the disk is idle and might move the head away to service a different task.
		- When the app asks for the next block, the head has to seek _back_. This "back-and-forth" kills performance.

- **Q: How does Anticipatory Scheduling solve this?**
	-  It **Pauses and Waits**.
		- After servicing a read request, the scheduler does _nothing_ for a few milliseconds (up to 6ms).
		- It bets (anticipates) that the same app will ask for the next adjacent sector soon.
		- If the request comes, it serves it instantly without seeking. If not, it wasted a few ms.


#### The Complete Fair Queuing (CFQ) I/O Scheduler

- This was the default scheduler for desktop Linux for many years.
- It focuses on fairness between **Processes**, not just requests.

- **Q: How does CFQ work?**
	- It gives every Process its own Queue.
		- If you have 10 apps running, you have 10 queues.
		- The Scheduler services them in a **Round Robin** fashion (Queue 1, then Queue 2, then Queue 3...).
		- It grabs a "slice" of time (or number of sectors) from one queue before moving to the next.


#### The Noop I/O Scheduler

- The simplest scheduler possible.
- "Noop" stands for No Operation (regarding sorting).

- **Q: How does Noop work?**
	- It is a simple FIFO (First-In, First-Out) queue.
		- It **Does** perform merging (combining adjacent requests).
		- It **Does Not** perform sorting (elevator algorithm).

- **Q: Who is this for?**
	- **Flash Storage (SD Cards, USB, SSDs)**.
		- Flash memory has no physical moving head. "Seeking" is instant.
		- Spending CPU cycles to sort requests by sector number is a waste of time on an SD card. Just send them as fast as possible!


#### Modern Usage:

- The I/O Scheduler landscape has been completely rewritten to support modern NVMe SSDs (which are too fast for old schedulers).

- **Legacy vs. Multi-Queue (`blk-mq`):**
    - **Old Kernel:** Describes "Single Queue" schedulers.
    - **Modern Kernel:** We now use **Multi-Queue** schedulers.
	    - **Dead Schedulers:** The "Linus Elevator", "Anticipatory", and original "CFQ" are **deleted** from the modern kernel.

- **The New Schedulers:**
	- **`mq-deadline`:** The multi-queue version of the Deadline scheduler. Good for SATA SSDs and HDDs.
	- **`bfq` (Budget Fair Queuing):** The spiritual successor to CFQ. Extremely complex, designed for desktop responsiveness (so your mouse doesn't lag while copying a large file).
	- **`kyber`:** A fast scheduler designed by Facebook/Meta for high-end NVMe servers.
	- **`none` (The new Noop):** For fast NVMe drives, we often run with no scheduler. The hardware is so fast that any software reordering just slows it down.

- **Embedded Context:**
	- On your embedded ARM board with an eMMC or SD Card: You will likely use `mq-deadline` or `bfq`.
	- On a Ramdisk: You use `none`.


## **Quick Recall**

- **Q: If I want to read the byte at offset 500 inside a file, but the disk sector size is 512 bytes, how much data does the disk actually read?**
	- **512 Bytes (One Sector)**. The hardware cannot read just one byte. It reads the entire 512-byte sector into a buffer in RAM, and the OS gives you the specific byte you asked for.

- **Q: Why is the `bio` structure preferred over `buffer_head` for moving large data?**
	- **Scatter-Gather (Vector) I/O**. A single `bio` can point to multiple non-contiguous physical pages in RAM. A `buffer_head` only points to one. To write 1MB of data, you need _one_ `bio`, but you would need thousands of `buffer_heads`.

- **Q: Explain the "Elevator Algorithm" simply.**
	- It sorts pending requests based on their physical sector number to ensure the disk head moves in one smooth direction (like an elevator going up) rather than zig-zagging wildly.

- **Q: Which I/O Scheduler should I use for an SD Card or eMMC?**
	- **Noop (or None / `mq-deadline`)**. Since Flash memory has no moving parts, "seeking" is instant. Spending CPU cycles to sort requests (like the Elevator does) is wasted effort. Just send the requests as fast as possible (FIFO).

- **Q: What is the difference between a `Request` and a `Bio`?**
	- **Bio:** A request from the Filesystem (High level). "I want these pages."
	- **Request:** A job in the Block Queue (Low level).
	- **Relationship:** The Kernel merges multiple adjacent `Bio`s into a single `Request` to send to the driver.


## **Hands-on Ideas**

As an embedded developer, the Block Layer is often something you "tune" rather than "write." These exercises focus on performance and configuration.

- **The "Tuner": Changing I/O Schedulers on the Fly**
	- You don't need to recompile the kernel to change the scheduler. You can do it at runtime.
	- **Task:**
		1. Find your disk name (e.g., `sda` or `mmcblk0`): `lsblk`.
		2. Check the current scheduler:
		   ```c
			cat /sys/block/sda/queue/scheduler
			# Output might be: [mq-deadline] kyber bfq none
		   ```
			- _(The one in brackets `[]` is active)._

		3. Change it to `bfq` (Budget Fair Queuing):
		   ```c
			echo bfq | sudo tee /sys/block/sda/queue/scheduler
		   ```
		4. **Experiment:** Run a heavy file copy. Does the system feel more responsive (mouse doesn't lag)? That is BFQ's goal.

-  **The "Simulator": Creating a Virtual Block Device**
	- If you don't want to mess with your real partitions, create a "Loop Device". This makes a file look like a hard drive.
	- **Task:**
		1. Create a 100 MB file:
		   ```c
			dd if=/dev/zero of=disk.img bs=1M count=100
		   ```
		2. Turn it into a block device:
		   ```c
			sudo losetup -fP disk.img
		   ```
		3. Find it: `lsblk` (look for `loop0`)
		4. Format it: `sudo mkfs.ext4 /dev/loop0`.
		5. Mount it and use it.
			- **Concept:** You just created a Block Device backed by a File, handled by the VFS and Block Layer!

- **The "Investigator": Watching Block I/O Live**
	- `top` shows CPU usage. `iotop` shows Disk usage.
	- **Task:**
		1. Install `iotop`: `sudo apt install iotop`.
		2. Run `sudo iotop`.
		3. Open another terminal and run `dd if=/dev/zero of=testfile bs=1M count=1000`.
		4. Watch `iotop`.
			- **Observe:** You will see the `dd` process at the top, the Read/Write speeds, and the IO% (how much time the process spent waiting for the disk). This is crucial for debugging "sluggish" embedded systems.

- **The "Deep Dive": `blktrace` (Advanced)**
	- If you really want to see the "Merge" and "Dispatch" events we studied.
	- **Task:**
		1. Run `sudo blktrace -d /dev/sda -o - | blkparse -i -`.
		2. **Warning:** This produces a LOT of output.
		3. You will see flags like:
		    - `Q`: Queued (Bio created).
		    - `G`: Get Request.
		    - `M`: Merge (The kernel combined this bio with another!).
		    - `D`: Dispatch (Sent to driver).
		    - **Goal:** Spot an `M` event. You just witnessed the Block Layer optimization in real-time!