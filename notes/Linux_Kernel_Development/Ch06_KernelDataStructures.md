# Chapter 6: [Kernel Data Structures]

## **Summary**
- Linux kernel provides some generic data structures to encourage the code reuse. Developers should use these data structures whenever possible and not create their own.
- Following are the generic data structures provided by kernel:
	1. Linked lists
	2. Queues
	3. Maps
	4. Binary Trees
### **Linked Lists**
- Linked list is a data structure that can store and manipulate variable number of *elements*. These *elements* are called *nodes* of the list.
- **Linked list vs Array:** Unlike in static arrays, the elements in a linked list are created dynamically and inserted into the list.
- The elements in the linked list do not necessarily occupy contiguous regions in the memory because these elements are created at different times.
- Therefore, the elements needs to be *linked* together. To do so, each element contains a pointer to the *next* element.
- #### Singly Linked Lists
```c
/* an element in a linked list */
struct list_element {
	void *data;                /* the payload */
	struct list_element *next; /* pointer to the next element */
};
```
![[Pasted image 20250518104102.png]]
- #### Doubly Linked Lists
```c
/* an element in a linked list */
struct list_element {
	void *data;                /* the payload */
	struct list_element *next; /* pointer to the next element */
	struct list_element *prev; /* pointer to the previous element */
};
```
![[Pasted image 20250518104359.png]]

- #### Circular Linked Lists
	- It comes in both singly and doubly linked list versions.
	![[Pasted image 20250518104604.png]]

- Linux Kernel's linked list implementation is unique, but it is fundamentally a *circular doubly linked list*.
- Movement through a linked list occurs linearly.
- Linked list in not suited for the use cases where accessing the element randomly is an important operation. Instead, linked list is supposed to be used where iterating over the whole list is important and dynamic addition/removal of elements is required.
- The first element in a linked list is called *head* that enables the easy access to the "start" of the list.

- #### The Linux Kernel's Implementation
	- Generic approach of storing a structure in a linked list is to embed the list pointer in the structure. For example:
  ```c
	struct fox {
		unsigned long tail_length; /* length in centimeters of tail */
		unsigned long weight;      /* weight in kilograms */
		bool is_fantastic;         /* is this fox fantastic? */
	};

	struct fox {
		unsigned long tail_length; /* length in centimeters of tail */
		unsigned long weight;      /* weight in kilograms */
		bool is_fantastic;         /* is this fox fantastic? */
		struct fox *next;          /* next fox in linked list */
		struct fox *prev;          /* previous fox in linked list */
	};
	```
	- The Linux Kernel approach is different. Instead of turning the structure into a linked list, the Linux approach is to *embed a linked list node in the structure*.

- #### The Linked List Structure
	- In kernel, a linked list node is just `prev` and `next` pointers (as defined by `list_head`). i.e. it doesn't have any payload/data. 
	- This linked list node can be embedded in a parent structure which has the actual data.
	- All linked list operations only operate on the linked list nodes without consideration of the parent structure. Therefore, additional manipulation using `list_entry()` is needed to operate on the parent structure of each linked list node.
	- The linked-list code is declared in the header file `<linux/list.h>` and the data structure is simple:
	```c
	struct list_head {
		struct list_head *next
		struct list_head *prev;
	};
	```
	- The `next` pointer points to the next list node, and the `prev` pointer points to the previous list node. Yet, seemingly, this is not particularly useful. The utility is in *how* the `list_head` structure is used:
  ```c
	struct fox {
		unsigned long tail_length; /* length in centimeters of tail */
		unsigned long weight;      /* weight in kilograms */
		bool is_fantastic;         /* is this fox fantastic? */
		struct list_head list;     /* list of all fox structures */
	};
	```
	- With this, `list.next` in `fox` points to the next element, and `list.prev` in `fox` points to the previous. Now this is becoming useful, but it gets better.
	- The kernel provides a family of routines to manipulate linked lists. These methods are generic and accept only `list_head` structures.
		- `list_add()` - adds a new node to the existing linked list.
		```c
		static inline void list_add(struct list_head *new,
							struct list_head *head)
		{
	        __list_add(new, head, head->next);
		}

		static inline void __list_add(struct list_head *new,
                              struct list_head *prev,
                              struct list_head *next)
		{
	        if (!__list_add_valid(new, prev, next))
                return;

	        next->prev = new;
	        new->next = next;
	        new->prev = prev;
	        WRITE_ONCE(prev->next, new);
		}
		```
		- `container_of()` - it is a macro which returns the parent structure of any provided member variable.
		- `list_entry()` - function defined using `container_of()` macro to return the parent structure containing any `list_head`:
		```c
		#define list_entry(ptr, type, member) container_of(ptr, type, member)
		```

- #### Defining a Linked List
	- As shown previously, a `list_head` by itself is worthless; it is normally embedded inside your own structure:
	  ```c
	struct fox {
		unsigned long tail_length; /* length in centimeters of tail */
		unsigned long weight;      /* weight in kilograms */
		bool is_fantastic;         /* is this fox fantastic? */
		struct list_head list;     /* list of all fox structures */
	};
		```
	- The list needs to be initialized before it can be used. This can be done in two ways:
		1. Runtime: The most common way of linked list initialization.
		```c
		struct fox *red_fox;
		red_fox = kmalloc(sizeof(*red_fox), GFP_KERNEL);
		red_fox.tail_length = 40;
		red_fox.weight = 6;
		red_fox.is_fantastic = false;
		INIT_LIST_HEAD(&red_fox->list);
		```
		2. Compile Time: If the structure is statically created at compile time, and you have the direct reference to it:
		   ```c
		struct fox red_fox = {
			.tail_length = 40,
			.weight = 6,
			.is_fantastic = false,
			.list = INIT_LIST_HEAD(red_fox.list),
		};
```

- #### List Heads
	- So far we understood how to create a linked list and how to manage it using kernel's linked list routines. But, to use these routines, we need a canonical pointer to refer to the list as a whole - a *head* pointer.
	```c
	static LIST_HEAD(fox_list);
	```
	- `LIST_HEAD()` defines and initializes a `list_head` named `fox_list`.

- #### Manipulating Linked Lists
	- ##### Adding a Node to a Linked List
		- `list_add()` function adds the new node to the given list immediately *after* the `head` node.
		```c
		list_add(struct list_head *new, struct list_head *head)
		```
		- For example: If we want to add a new `struct fox` to the `fox_list` list,
			```c
			list_add(&f->list, &fox_list);
			```
		- To add a node at the end of the list:
		```c
			list_add_tail(struct list_head *new, struct list_head *head)
		```

	- ##### Deleting a Node from a Linked List
		- `list_del()` function removes the `element` entry from the list. It doesn't free the memory used by the node.
	```c
		list_del(struct list_head *entry)
	```

	- ##### Moving and Splicing (Joining) Linked List Nodes
		-  `list_move()` - To move a node from one list to another. This function removes the `list` entry from its linked list and adds it to the given list the *after* the `head` element.
		```c
		list_move(struct list_head *list, struct list_head *head)
```
		- `list_move_tail()` - To move a node from one list to the end of another. This function does the same as list_move(), but inserts the `list` element *before* the `head` entry.
		```c
		list_move_tail(struct list_head *list, struct list_head *head)
```
		- `list_empty()` - To check whether a list is empty. This returns non-zero if the given list is empty.
		```c
		list_empty(struct list_head *head)
```
		- `list_splice()` - To splice to unconnected lists together. This function splices together two lists by inserting the list pointed to by `list` to the given list after the element `head`.
		```c
		list_splice(struct list_head *list, struct list_head *head)
```
		- `list_splice_init()` - To splice two unconnected lists together and reinitialize the old list. This function works the same as `list_splice()`, except that the emptied list pointed to by `list` is reinitialized.
		```c
		list_splice_init(struct list_head *list, struct list_head *head)
```

- #### Traversing Linked Lists
	- list_for_each() is used to traverse over linked list
		- But it needs some modification (list_entry()) to traverse over the structures in which the list is embedded.
	- Most kernel code uses `list_for_each_entry()` macro to iterate over a linked list.
	  ```c
	/**
	 * list_for_each_entry  -       iterate over list of given type
	 * @pos:        the type * to use as a loop cursor.
	 * @head:       the head for your list.
	 * @member:     the name of the list_head within the struct.
	 */
	#define list_for_each_entry(pos, head, member)                          \
	        for (pos = list_first_entry(head, typeof(*pos), member);        \
	             !list_entry_is_head(pos, head, member);                    \
	             pos = list_next_entry(pos, member))
	```
	- Here `pos` is the pointer to the object containing the `list_head` nodes. Think of it as a return value from `list_entry()`.
	- `head` is a pointer (like `fox_list` in our example) to the `list_head` head node from which the iteration to be started.
	- `member` is the variable name of the `list_head` structure in `pos`.

- #### Iterating Through a List Backward
	- `list_for_each_entry_reverse(pos, head, member)`
	- Instead of following the `next` pointers *forward* through the list, it follows the `prev` pointers *backward*.
	- Why do we want to iterate backward?
		1. Performance: When we know that the node we are looking for is likely *behind* the node we start the search from.
		2. Order: If the linked list is used as a stack, we can iterate the list from the tail backward to achieve *last-in/first-out* (LIFO) ordering.

- #### Iterating While Removing
	- The standard iteration methods are not appropriate for this because they rely on the fact that the list entries are not changing. If the current entry is removed in the body of the `for` loop, the subsequent iteration cannot advance to the next (or previous) pointer.
	- Linux kernel provides following macro to handle such situations:
	  ```c
	/**
	 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
	 * @pos:        the type * to use as a loop cursor.
	 * @n:          another type * to use as temporary storage
	 * @head:       the head for your list.
	 * @member:     the name of the list_head within the struct.
	 */
	#define list_for_each_entry_safe(pos, n, head, member)                  \
	        for (pos = list_first_entry(head, typeof(*pos), member),        \
	                n = list_next_entry(pos, member);                       \
	             !list_entry_is_head(pos, head, member);                    \
	             pos = n, n = list_next_entry(n, member))
```
	- This macro can be used in the same manner as `list_for_each_enty()` except that you provide the `next` pointer, which is of the same type as `pos`.
	- The next pointer is used by the macro `list_for_each_entry_safe()` to store the next entry in the list, making it safe to remove the current entry.

- ##### Need of Locking
	- The “safe” variants of `list_for_each_entry()` protect you *only* from removals from the list *within* the body of the loop. If there is a chance of concurrent removals from other code-or any other form of concurrent list manipulation-you need to properly lock access to the list.
	- See Chapters 9, “*An Introduction to Kernel Synchronization*,” and Chapter 10, “*Kernel Synchronization Methods*,” for a discussion on synchronization and locking.


### **Queues**
- A common programming patter in any operating system kernel is *producer* and *consumer*
  *producer*: it creates data. For example: error message to be read or networking packets to be processed.
  *consumer*: it reads, processes, in other words *consumes* the data.
- The easiest way to implement such pattern is by using a *queue*. In a queue, the producer pushes (enqueue or *in*) the data into the queue and the consumer takes out (dequeue or *out*) the data from the queue in the order of *first-in, first-out* (FIFO).
  ![[Pasted image 20250521071849.png]]





- #### kfifo
	- Linux kernel's generic queue implementation is call *kfifo*.
	  Implemented in `kernel/kfifo.c` and declared in `<linux/kfifo.h>`.
	- kfifo object maintains two offsets into the queue:
	  1. *in offset*: it the location in the queue to which the next enqueue will occur
	  2. *out offset*: location in the queue from which the next dequeue will occur
	- **Note:** The *out offset* is always greater than or equal to the *in offset* in a queue, otherwise, you could dequeue a data that had not been enqueued.
	- After every enqueue (*in*) operation, the *in offset* is incremented by the amount of data enqueued. Similarly, after every dequeue (*out*) operation, the *out offset* is incremented by the amount of data dequeued.
	- *out offset* = *in offset*: Queue is empty. No more dequeue is possible.
	- *in offset* = length of the queue: Queue is full. No more data can be enqueued until the queue is reset.

- #### Create a Queue
	- To use a kfifo, it must be defined first and then initialized. As with most kernel objects, this can be done statically (less common method) or dynamically (most common method):
	1. Statically:
	   This creates a static kfifo named `name` with a queue of `size` bytes.
	    ```c
		DECLARE_KFIFO(name, size);
		INIT_KFIFO(name);
		```
	2. Dynamically:
		- This creates and initializes a kfifo with a queue of `size` bytes.
		- The kernel uses the *gfp mask* `gfp_mask` to allocate the queue. (More information on memory allocation in Chapter 12, "Memory Management")
	   ```c
		int kfifo_alloc(struct kfifo *fifo, unsigned int size, gfp_t gfp_mask);
		```
		- If we want to allocate the buffer our-self, we can use `kfifio_init()`.
		- This function creates and initializes a a kfifo that will use the `size` bytes of memory pointed at by `buffer` for its queue.
		```c
		void kfifo_init(struct kfifo *fifo, void *buffer, unsigned int size);
		```
- **NOTE:** The `size` must be a power of 2.

- #### Enqueue Data
	- `kfifo_in()`: It copies the `len` bytes of data starting at `from` into the queue `fifo` starting from *in offset*.
	```c
unsigned int kfifo_in(struct kfifo *fifo, const void *from, unsigned int len);
```
	- The function returns the number of bytes copied into the queue depending on the number of bytes free in the queue. If nothing was copied it will return 0.

- #### Dequeue Data
	- `kfifo_out()`: It copies at most `len` bytes starting from *out offset* from the queue `fifo` to the buffer pointed at by `to`.
	```c
unsigned int kfifo_in(struct kfifo *fifo, const void *from, unsigned int len);
```
	- The function returns the number of bytes copied depending on the number of occupied bytes present in the queue.
	- After dequeued, the data is no longer accessible from the queue.

- #### Peek Data without Dequeue
	- `kfifo_out_peek()`: It works same as `kfifo_out`, except that the *out offset* is not incremented.
	```c
	unsigned int kfifo_out_peek(struct kfifo *fifo, void *to, unsigned int len, unsigned offset);
```
	- The parameter `offset` specifies an index in the queue; specify 0 to read from the head of the queue, as `kfifo_out()` does.

- #### Obtaining the size of the Queue
	- `kfifo_size()`: It returns the total size in bytes of the buffer used to store kfifo's queue.
	```c
static inline unsigned int kfifo_size(struct kfifo *fifo);
```

- #### Other functions to access information about the Queue
	- `kfifo_len()`: Obtain number of bytes enqueued in the queue.
	- `kfifo_avail()`: To find out number of bytes available for write in the queue.
	- `kfifo_is_empty()` and `kfifo_is_full()`: It returns non-zero if the given kfifo is empty or full, respectively, and zero if not.

- #### Resetting and Destroying a Queue
	- `kfifo_reset()`: To reset a kfifo, discarding all the content of the queue.
	- `kfifo_free()`: To destroy a kfifo allocated with `kfifo_alloc()`.
	- If the kfifo was created using `kfifo_init()`, the associated buffer should be freed separately (How to do it? -> See Chapter 12 discussion on allocating and freeing dynamic memory).

- #### Example Queue Usage
	- Assume we created a kfifo pointed at `fifo` with a queue size of 8KB.
	- In this example, we will enqueue simple integers (In reality, it would more complicated, task-specific structures), peek the queue, and the dequeue them.
```c
unsigned int i;
/* enqueue [0, 32) to the kfifo named 'fifo' */
for (i = 0; i < 32; i++) {
	kfifo_in(fifo, &i, sizeof(i));
}
// The kfifo named 'fifo' now contains 0 through 31, inclusive.
// We can peek at the first item at the queue and verify it is 0.
unsigned int val;
int ret;
ret = kfifo_peek_out(fifo, &val, sizeof(val), 0);
if (ret != sizeof(val)) {
	return -EINVAL;
}
printk(KERN_INFO "First item in the queue is %d\n", val); /* should print 0 */

// Now, dequeue all the items from the queue
/* while there is data in the queue */
while(kfifo_avail(fifo)) {
	unsigned int val;
	int ret;
	/* read it, one integer at at time */
	ret = kfifo_out(fifo, &val, sizeof(val));
	if (ret != sizeof(val)) {
		return -EINVAL;
	}
	printk(KERN_INFO "%u\n", val);
}
```

- The above test code prints 0 through 31, inclusive, and in that order.
	**Note:** If the above code snippet printed the numbers backward, from 31 to 0, we would have a stack not a queue.

### **Maps**
- A *map* also known as *associative array*, is a collection of unique keys, where each key is associated with a specific value. The relationship between a key and its value is called *mapping*.
- In kernel, the maps provide a way to link UIDs to their corresponding kernel objects (e.g. PID to `task_struct()`)
- Maps enable fast lookup, allocation, and management of resources in the kernel.
- **What is a UID?**
	- UID (Unique Identifier): A UID is a general term for any unique number used to identify a specific resource or object in the Linux Kernel. It's like a label that helps the kernel keep track of things.
	- It could refer to many kinds of identifiers, such as:
		1. A process ID (PID) for a process
		2. An inode number for a file
		3. A file descriptor number for an open file
		4. Any other unique number used to tag a kernel resource
- **UID vs PID**
	- PID is a type of UID, but not all UIDs are PIDs.
	- A PID is a specific type of UID that identifies a process in the Linux Kernel.
- idmap:
	- idmap is a general term for the kernel's identity mapping system, which maps UIDs to kernel objects.
	- Typically implemented as a data structure like hash table or tree for efficient access.
- idr: (ID-to-Resource Mapping)
	- idr is a specific implementation of idmap, using a radix tree data structure.
	- designed for dynamic allocation and management of unique IDs (UIDs).
	- supports fast lookup, insertion, and deletion of ID-to-object mappings.
	- handles UID allocation to ensure uniqueness and their re-usability after resources are freed. (e.g., PID recycling)
	- provides O(log n) lookup performance due to the radix tree structure.
- Examples of Usage
	- Process Management: Maps PIDs (a type of UID) to `task_struct` for process lookup. (e.g., in system calls like `kill()`)
	- File Systems: Maps inode numbers to inode structures.
	- Device Drivers: Maps device IDs to device structures.
	- IPC: Maps IDs for message queues or semaphores to their objects.

- #### Initializing an idr
	- First step is to either statically define or dynamically allocate an `idr` structure, then next step is to call `idr_init()`.
	  ```c
	void idr_init(struct idr *idp);
```
	- Example:
	  ```c
	struct idr id_huh;     /* statically define idr structure */
	idr_init(&id_huh);     /* initialize provided idr structure */
```

- #### Allocating a new UID
	- After the idr setup, allocating a new UID is a two-step process:
	1. Tell the idr that we want to allocate a new UID, allowing it to resize the backing tree as necessary. This may require a memory allocation, without a lock.
	   ```c
	   int idr_pre_get(struct idr *idp, gfp_t gfp_mask);
		```
	2. Request the new UID and add it to the idr.
	   ```c
	   int idr_get_new(struct idr *idp, void *ptr, int *id);
		```
		- If the caller wants the new UID to be equal or greater than provided `starting_id` then `idr_get_new_above()` can be used instead of `idr_get_new().
		  ```c
	int idr_get_new_above(struct idr *idp, void *ptr, int starting_id, int *id)
		```

- #### Looking Up a UID
	- The caller provides the UID, and the idr returns the associated pointer.
	  ```c
	  void *idr_find(struct idr *idp, int id);
		```

- #### Removing a UID
	- To remove the UID from an idr:
	  ```c
	  void idr_remove(struct idr *idp, int id);
		```

- #### Destroying an idr
	- A successful call to `idr_destroy()` de-allocates only unused memory associated with the idr pointer pointed by `idp`. It does not free any memory currently in use by allocated UIDs.
	- Generally, kernel code wouldn't destroy its idr facility until it was shutting down or unloading, and it wouldn't unload until it had no more users (and thus no more UIDs).
	- To force removal of all UIDs, we can call `idr_remove_all()`.
	  ```c
	  void idr_remove_all(struct idr *idp);
		```
		Note: We should call `idr_remove_all()` first before calling `idr_destroy()` ensuring all idr memory was freed.


### **Binary Trees**
- Tree: A *tree* is a data structure that provides a hierarchical tree-like structure of data. Mathematically, it is an *acyclic*, *connected*, *directed graph* in which each vertex (called a *node*) has zero or more outgoing edges and zero or more incoming edges.
- Binary Tree: A tree in which nodes have at most two outgoing edges, i.e. a tree in which nodes have zero, one, or two children.
![[Pasted image 20250524132425.png]]

- #### Types of Binary Trees in the Kernel
	1. Binary Search Trees (BSTs):
		- Nodes are arranged so that the left sub-tree contains keys less than the parent node, and right sub-tree contains keys greater than the parent node.
		- Used for efficient searching, insertion, and deletion operations.
	2. Self-Balancing Trees:
		- The kernel often uses self-balancing variants like red-black trees to prevent performance degradation from unbalanced trees.
		- Ensures O(log n) time complexity for operations, even in worst-case scenarios.

- #### Red-Black Trees
	- A key focus in the kernel's use of binary trees, red-black trees are self-balancing search trees.
	- Properties:
		- Each node is colored red or black.
		- The root is black.
		- All leaves (NIL nodes) are black.
		- Red nodes cannot have children (no two red nodes are adjacent)
		- Every path from a node to its descendant leaves has the same number of black nodes.
	- There properties ensure the tree remains balanced, guaranteeing O(log n) performance for search, insert, and delete operations.

- #### Use Cases in the Linux Kernel
	- ##### Memory Management
		- Red-black trees are used to manage memory regions, such as virtual memory areas (VMAs) in a process's address space.
		- The `mm_struct` structure uses a red-black tree to track VMAs, enabling efficient lookup and management of memory mappings.
	- ##### Process Scheduling
		- Red-black trees are used in the Completely Fair Scheduler (CFS) to organize tasks based on their virtual runtime, ensuring fail scheduling.
	- ##### File Systems
		- Used in some file systems to manage inodes or directory entries for quick access.
	- ##### Networking
		- Red-black trees manage routing tables or other network-related data structures requiring ordered access.

- #### Implementation in Kernel (rbtrees)
	- The Linux provides a generic red-black tree implementation in `lib/rbtree.c` and `<linux/rbtree.h>`.
	- The root of an rbtree is represented by the `rb_root` structure. To create a new tree, a new `rb_root` should be allocated and initialized to a special value `RB_ROOT`.
	  ```c
	struct rb_root root = RB_ROOT;
```
	- Individual nodes in rbtree are represented by `rb_node` structure. Given an `rb_node`, we can move to its left or right child by following pointers off the node of the same name.
	- The rbtree implementation does not provide search and insert routines. Users of rbtrees are expected to define their own.

- #### Challenges and Considerations
	- Unbalanced binary search trees can degrade to O(n) performance, which is why kernel prefers self-balancing red-black trees.
	- Memory constraints in the kernel require efficient node management to avoid excessive overhead.
	- Synchronization is critical, as multiple kernel components may access trees concurrently, requiring locks or other mechanisms

- #### Comparison to Other Data Structures
	- Compared to hash tables (used in idmap/idr), binary trees are better for ordered data or range queries but may be slower for simple key-value lookups.
	- Compared to lists, binary trees offer faster search times for large datasets but are more complex to implement and maintain.

### **What Data Structure to Use, When**
- #### Linked Lists
	- If the primary use-case is to iterating over all the data.
	- When the performance is not important
	- When a relatively small number of items needs to be stored
	- If interaction is required with other kernel code that uses linked lists.
- #### Queue
	- If the code follows producer/consumer pattern, particularly if it can be done with fixed-size buffer.
	- If FIFO semantics are needed.
	- Should not be used when we need to store an unknown, potentially large number of items. Linked lists will make more sense here, because we can dynamically add any number of items in the list.
- #### Map
	- If unique identifiers (UIDs) like PIDs, file descriptors, are required to map to an object
	- Linux's map interface being specific to UID-to-pointer mappings, isn't good for much else.
- #### Red-Black Tree
	- If a large amount of data needs to be stored and looked up efficiently.
	- Red-black trees enable the searching in logarithmic time, while still providing an efficient linear time in-order traversal.
	- Although more complicated to implement than the other data structures, their in-memory footprint isn’t significantly worse.
	- Should not be used when time-critical look-up operations are not required. A linked linked should be favored here.
- #### Radix trees, bitmaps
	- If none of the above data structures fit your needs.
- #### Note:
	- Only after exhausting all kernel-provided solutions should you consider “rolling your own” data structure.

### **Algorithmic Complexity**
- Measures the resources (time and space) an algorithm or data structure requires as a function of input size (n).
- Expresses in **Big-O notation**, focusing on worst-case performance for predictability.
- **Time Complexity:**
	- Measures the number of computational steps for an operation, expressed as O(1), O(log n), O(n), etc.
	- **Importance:** Critical for kernel operations, where delays impact system responsiveness.
	- **Common Notations:**
		1. **O(1):** Constant time (e.g., array access, hash table lookup in ideal conditions)
		2. **O(log n):** Logarithmic time, scales well (e.g. red-black tree operations)
		3. **O(n):** Linear time, less desirable for large datasets (e.g., linked list traversal)
		4. **O(n^2):** Quadratic time, generally avoided in kernel due to poor scalability
-  **Kernel-Specific Considerations:**
	- **Predictability:** Worst-case time complexity is crucial for real-time operations like interrupt handling.
	- **Scalability:** Low complexity (O(1) or O(log n)) is preferred for large datasets (e.g. many processes or memory regions)
	- **Concurrency Overhead:** Locking or synchronization can add to effective complexity, requiring careful design.



## Quick Recall
- Q1: What is the primary advantage of using the kernel’s doubly linked list (`struct list_head`) for managing data in the Linux kernel?
	- The primary advantage is its simplicity and O(1) time complexity for insertion and deletion at a known position, making it ideal for dynamic, sequential data such as task lists or wait queues.
- Q2: How does the `idr` data structure in the Linux kernel ensure efficient management of unique identifiers?
	- The `idr`, implemented as a radix tree, provides O(log n) lookup, insertion, and deletion for mapping unique IDs (e.g., PIDs) to kernel objects, with dynamic allocation to ensure uniqueness.
- Q3: Why are red-black trees preferred over basic binary search trees in the Linux kernel for tasks like memory management?
	- Red-black trees are self-balancing, ensuring O(log n) time complexity for search, insert, and delete operations by maintaining color-based properties, unlike basic binary search trees, which can degrade to O(n) if unbalanced.
- Q4: What is the significance of the `kfifo` queue in the Linux kernel, and what is its typical time complexity for enqueue and dequeue operations?
	- The `kfifo` queue provides a thread-safe, fixed-size FIFO buffer for data like event logs or interrupts, with O(1) time complexity for enqueue and dequeue operations due to its circular buffer implementation.
- Q5: How does the concept of time complexity guide the selection of data structures in the Linux kernel?
	- Time complexity (e.g., O(1), O(log n), O(n)) helps developers choose data structures that ensure fast, predictable performance for kernel operations, prioritizing O(1) or O(log n) for time-critical tasks like scheduling or memory lookups.
- Q6: What factors should a kernel developer consider when choosing a data structure, according to the "What Data Structure to Use, When" section?
	- Developers should consider the operation type (e.g., search, insert), frequency, memory constraints, concurrency requirements, and simplicity, selecting structures like linked lists for sequential data or red-black trees for ordered data.

## Hands-On Ideas
1. **Linked List Kernel Module for GPIO Device Tracking**
    - **Description:** Write a kernel module that uses the kernel’s doubly linked list (struct list_head) to manage a list of GPIO pins on the BBB. The module will allow users to add, remove, and list GPIO pins (e.g., used for LEDs or buttons) via a `/proc` file interface.
2. **Kernel Queue for Button Press Events**
	- **Description:** Implement a kernel module that uses the kernel’s kfifo queue to buffer button press events from a GPIO pin on the BBB. The queue will store timestamps of button presses, readable via a /proc file.
3. **idr-Based Device ID Allocator**
	- **Description:** Create a kernel module that uses the idr data structure to allocate and manage unique IDs for virtual devices on the BBB. Each device will represent a GPIO pin, and the module will expose an interface via `/sys` to allocate and free IDs.
4. **Red-Black Tree for Process Priority Management**
	- **Description:** Develop a kernel module that uses a red-black tree to manage a custom priority list of processes on the BBB. Each process is identified by its PID and assigned a priority, stored in a red-black tree, with a `/proc` interface to add, remove, and list priorities.
5. **Bench-marking Data Structure Performance**
	- **Description:** Create a kernel module that benchmarks the performance of linked lists, red-black trees, and idr for storing and retrieving a large number of virtual device entries (e.g., 1000 entries). Measure insertion and lookup times to compare O(1), O(n), and O(log n) complexities, outputting results via `/proc`.
- ***Note*:** *More information on how to do it:*
  https://grok.com/share/c2hhcmQtMg%3D%3D_3380570c-3b13-4a0a-89a7-91658392ee9f
