
# Chapter 17: [Devices and Modules]


## **Summary**

- This chapter bridges the gap between the software kernel and physical hardware.
- It introduces the unified framework Linux uses to manage hardware complexities.
- It covers four major kernel components related to device drivers:
	- **Device types:** Classifications used to unify how we interact with hardware.
	- **Modules:** The mechanism allowing Linux kernel to load  or unload driver code at runtime.
	- **Kernel objects:** The framework creating parent and child relationships between kernel structures.
	- **Sysfs:** A virtual filesystem that represents the system's hardware device tree in user space.

- **Q: Why does the kernel need these four specific components?**
	- They work together to abstract hardware details from user space.
		- They provide a standardized way to classify hardware.
		- They allow the kernel to load driver code dynamically.
		- They create a structured hierarchy of how devices connect to the system.

### Device Types

- In Linux, following the standard Unix philosophy, almost all physical hardware is represented to user space as a virtual "file" called a device node.
- These devices are broadly categorized into three primary types to standardize communication.
	- **Block Devices (blkdevs):**
		- Handle data in addressable chunks called "blocks".
		- Support random access (seeking).
		- **Analogy:** Like a book. You can jump instantly to page 50 without reading pages 1 through 49.
		- **Examples:** Hard drives, SD cards, and flash memory.
		- Usually formatted with a filesystem (like ext4) and mounted by the user.

	- **Character Devices (cdevs):**
		- Handle data as a continuous stream of bytes.
		- Usually do not support random access.
		- **Analogy:** Like a water pipe. You must process the water exactly in the order it flows out.
		- **Examples:** Keyboards, mice, UART serial ports, and I2C sensors.
		- Accessed directly via character device nodes (like `/dev/ttyS0`).

	- **Network Devices**
		- Provide access to a network (like Ethernet or WiFi).
		- Break the "everything is a file" rule.
		- Accessed using the Socket API instead of standard device nodes in `/dev/`.

#### Other Specialized Devices:
- **Miscellaneous Devices (miscdevs):**
	- A lightweight version of character devices.
	- Used by developers to quickly write simple drivers.
	- Shares common infrastructure to avoid needing a full major device number.

- **Pseudo Devices:**
	- Virtual device drivers providing kernel functionality.
	- Do not communicate with actual physical hardware.
	- **Examples:** `/dev/random` (generates random numbers) or `/dev/null` (a black hole that discards written data).


#### Visualization:
  ```mermaid
	flowchart TD
    A["Linux Hardware Interfaces"] --> B["Block Devices"]
    A --> C["Character Devices"]
    A --> D["Network Devices"]
    
    B --> B1["Random Access (Seekable)"]
    B1 --> B2["Hard Drives, SD Cards, Flash"]
    
    C --> C1["Stream of Bytes (Sequential)"]
    C1 --> C2["UART, Keyboards, Sensors"]
    
    D --> D1["Breaks Everything is a File rule"]
    D1 --> D2["Accessed via Socket API"]
    
    C --> E["Misc Devices"]
    E --> E1["Simplified Char Devices"]
  ```


#### Modern Usage

- **Block Layer Evolution:**
	- **Old Kernel:** Describes traditional blocks layers optimized for slow magnetic hard drives with single processing queues.
	- **Modern Kernel (v6.0+):** The kernel now uses the Block Multi Queue framework. It is designed for incredibly fast parallel storage like NVMe, handling millions of operations across multiple CPU cores.

- **Embedded ARM Storage:**
	- Embedded block devices are rarely spinning hard drives.
	- They are usually eMMC chips, raw NAND flash, or modern NVMe.
	- Because raw flash requires wear leveling, you often use specialized filesystems (like UBIFS) or rely on the eMMC controller.
	- Modern kernels have specialized subsystems (like the MTD subsystem) to handle complex wear leveling that old kernels lacked.

- **Character Devices in IoT:**
	- These are an embedded engineer's bread and butter.
	- Writing drivers for an I2C temperature sensor, SPI display, or GPIO button means you are interacting with Character Devices.

- **Automated Device Nodes (devtmpfs):**
	- **Old Kernel:** Engineers had to manually create device nodes under `/dev/` using the `mknod` command with strict major and minor numbers.
	- **Modern Kernel:** Systems now use `devtmpfs` inside the kernel, paired with user-space tools like `udev` or `mdev` in BusyBox.
	- These automatically create the device nodes the instant your driver registers the hardware.


### Modules

Despite being a "monolithic" kernel (meaning the whole kernel runs in one massive shared memory space), Linux is highly modular.
-  You can compile a minimal base kernel and dynamically load drivers (`.ko` files) only when needed as "Modules".
- Modules are built and managed using the kernel kbuild system.

#### The "Hello, World!" Module
Writing a module is slightly similar to writing a standard user space application because it has distinct entry and exit points.

- **The Entry Point:**
	- Defined by the `module_init()`macro.
	- Called by the kernel when you load the module (using `insmod`).
	- Must return 0 if initialization succeeds.
	- Returns a non zero error code if it fails.

- **The Exit Point:**
	- Defined by the `module_exit()` macro.
	- Called when you remove a module (using `rmmod`).
	- Its strict job is cleanup: free memory and shut down hardware.

- **Licensing and the "Tainted" Kernel:**
	- You must declare a license using `MODULE_LICENSE("GPL")`.
	- Loading a proprietary (non GPL) module marks the kernel as "tainted".
	- Kernel developers usually ignore bug reports from tainted kernels because they cannot debug closed source code to find the root cause of a crash.

- **Q: How is a kernel module structured differently than a user space application?**
	- A module lacks a standard `main()` function and uses entry and exit hooks instead.

#### Building Modules (Living in the Source Tree)
Modern Linux uses the **kbuild** system, making module compilation much easier.

##### Living in the Source Tree
- If adding a driver to the kernel source, it lives inside the `drivers/` directory, categorized by subsystem.
- To build your new module, you edit the `Makefile` in that directory:
	- **Direct Build:** `obj-m += fishing.o` 
		- Tells kbuild to build `fishing.c` as a loadable module (`fishing.ko`).
	- **Configured Build:** `obj-$(CONFIG_FISHING_POLE) += fishing.o`
		- The most common approach.
		- Builds the module only if the user selected CONFIG_FISHING_POLE during kernel configuration (`menuconfig`).
	- **Handling Multiple Files:**
		- Used when a complex driver spans multiple C files.
		- Instructs kbuild to link them using: `fishing-objs := main.o line.o`.

##### Living Externally
- We need not always have to put our driver code directly inside the main Linux kernel source tree.
- We can build our module "out of tree" (externally) in your own project directory.
- We still need a `Makefile` with the `obj-m := fishing.o` instruction.

- **Q: How do you compile a module that lives outside the kernel tree?**
	- We must tell our `make` command where the pre-compiled kernel source is located.
	- We run a command like: `make -C /path/to/kernel/source SUBDIRS=$PWD modules`
	- The `-C` flag tells the compiler to change into the kernel source directory to find the main build rules before building the code in our current working directory.

#### Installing Modules
- Once a module is compiled (`.ko` file), it needs to be placed in a standard location so the system can find it automatically.
- Compiled modules are stored in `/lib/modules/version/kernel/` on our filesystem.

- **Q: How do you install your compiled modules into the correct directory?**
	- We use the kernel build system installation command.
		- Run `make modules_install` as the root user.
		- This automatically copies our `.ko` files into the proper `/lib/modules` subdirectories based on our kernel version.


#### Generating Module Dependencies

- Linux modules can depend on other modules to function correctly.
- For example, a specific USB camera driver might depend on the core USB subsystem module being loaded first.
- The kernel needs a map of these dependencies to load things in the correct order.

- **Q: How does Linux know which modules depend on others?**
	- It uses a tool to map them out and store the information.
		- The `depmod` command scans your modules and generates a dependency map.
		- This map is saved in a text file located at `/lib/module/version/modules.dep`.
		- Most Linux distributions automatically run `depmod`during the boot process to keep this file updated.

#### Loading Modules

- Linux provided different utilities to load (`.ko`) modules into the running kernel.
- Some utilities are very basic and manual, while others are smart and automated.

- **Q: What is the difference between `insmod` and `modprobe`?**
	- It comes down to dependency resolution.
		- **`insmod`:** A very basic tool. You run `insmod module.ko`.
			- It blindly tries to load exactly what we tell it.
			- If the module requires another module that is not loaded yet, `insmod` will simply fail. Think of it like a manual override.
		- **`modprobe`:** The smart, preferred tool. You run `modprobe module`.
			- It checks the `modules.dep` file first, automatically loads any required background dependencies, and then loads our module. Think of it like an intelligent assistant.

- **Q: How do you remove a module from the kernel?**
	- We can use either `rmmod` or `modprobe`.
		- Run `rmmod module_name` (We do not need the `.ko` extension here)
		- Alternatively, run `modprobe -r module_name` which smartly removes the module and any unused dependencies it originally pulled in.


#### Managing Configuration Options
- The Linux kernel uses the Kconfig system to manage thousands of compilation options.
- When your run `make menuconfig`, the menus you see are generated by reading `Kconfig` files scattered throughout the kernel source tree.
- Adding a new driver means you must add an entry to the local `Kconfig` file so users can select it.

- **Q: What are the main configuration directives in a Kconfig file?**
	- These directives define how your driver appears in the configuration menu.
		- **tristate:** Means the driver can be built into the kernel (Y), built as a loadable module (M), or completely excluded (N). This is standard for most device drivers.
		- **bool:** Means the option can only be built in (Y) or excluded (N). It cannot be a module.
		- **depends on:** Hides your option unless another specific option is enabled first (e.g., your I2C sensor driver depends on the core I2C subsystem being enabled.)
		- **select:** Forcibly enables another option if your driver is chosen. Kernel developers advise using this sparingly because it forces configurations behind the scenes.
		- **help:** Provides the text descriptions the user sees when they ask for more information about your driver in the configuration menu.


#### Module Parameters
- The kernel allows you to pass arguments (variables) to your driver at the exact moment it loads.
- These parameters let users tweak driver behavior without having to recompile the C code.
- They are exposed to user space via the sysfs virtual system.

- **Q: How do you define a module parameter in your C code?**
	- We use the `module_param()` macro.
		- The syntax is `module_param(name, type, perm)`.
		- **name:** The variable name is your C code.
		- **type:** The data type (like int, bool, or charp for character pointers).
		- **perm:** The sysfs file permissions (like `0644`). This decides if user-space applications can read or write to this parameter while the system is running. A permission of 0 completely hides it from sysfs.

- We can pass complex data types like strings and arrays to modules when they are loaded.
- The kernel provides specific macros to handle memory copying for these complex types automatically.
- All of these parameter macros require us to include the standard Linux module header file in our C code.

- **Q: How do you actually pass these parameters to the kernel?**
	- You append them as key value pairs using standard command line tools or bootloader arguments.
		- **Via user space tools:** When loading a module manually, you type the arguments right after the command (e.g., `insmod fishing.ko pole_length=5` or `modprobe fishing pole_length=5`).
		- **Via bootloader (for built in drivers):** If the driver is compiled directly into the kernel (not a module), the bootloader (like U-Boot or GRUB) passes them via the main kernel command line (e.g., appending `fishing.pole_length=5` to the `bootargs`).

- **Q: How do you pass a strings or arrays to a module?**
	- We have specialized macros available depending on how we want to manage memory.
		- `module_param(name, charp, perm)`: Use this to let the kernel manage the memory and simply point our character pointer to it.
		- `module_param_string(name, string, len, perm)`: Use this to force the kernel to copy the text directly into a character array buffer that we have already created.
		- `module_param_array(name, type, nump, perm)`: Use this to accept a comma separated list of values to fill a static C array.

- **Q: How do you pass a list of values to a module?**
	- We use the array macros to accept a comma separated list of parameters.
		- `module_param_array(name, type, nump, perm)`: Use this to fill a static C array.
			- `nump`: This argument is a pointer to an integer where the kernel will store exactly how many items the user actually provided.
			- The array in our code must be statically allocated at compile time so the kernel can prevent memory overruns.
		- `module_param_array_named()`: If our internal array variable has a different name than what we want the use to type, we can use this function.

- **Q: How do you document these parameters so other users know how to use them?**
	- We use the description macro.
		- Use `MODULE_PARAM_DESC(name, "Description text"`.
		- The text shows up when a user types the `modinfo` command in their terminal to inspect our compiled `.ko` file.

#### Exported Symbols

- When modules are loaded into RAM, they are dynamically linked into the running kernel.
- Unlike core kernel code, modules cannot just call any random internal function they want.
- Modules can only use external functions that the kernel explicitly makes available to them.
- These available functions are known as exported kernel interfaces.

- **Q: How does the kernel export a symbol for modules to use?**
	- By using special macros placed immediately after the function definition in the code code.
		- The `EXPORT_SYMBOL(function_name)` macro makes the function available to any loadable module.
		- **Analogy:** It is like putting a public API gateway in front of your private company server so external applications can safely request data.

- **Q: What if a kernel developer only wants open source modules to use their function?**
	- They use the strict GPL version of the export macro.
		- The `EXPORT_SYMBOL_GPL(function_name)` macro restricts access completely.
		- The kernel linker enforces this by checking the license declaration of the module trying to use it.
		- If a proprietary closed source driver tries to call a  GPL exported symbol, the kernel strictly blocks it and throws a linking error.

- **Visualization:**
  ```mermaid
	flowchart TD
	A["Core Kernel Function"] --> B{"Is it Exported?"}
	B -- "No" --> C["Hidden from Modules"]
	B -- "EXPORT_SYMBOL" --> D["Available to ALL Modules"]
	B -- "EXPORT_SYMBOL_GPL" --> E["Available ONLY to GPL Modules"]
	
	D --> F["Proprietary Driver (Closed Source)"]
	D --> G["Open Source Driver (GPL)"]
	
	E -- "Blocked Linker Error" --> F
	E -- "Allowed" --> G
  ```


#### Modern Usage

- **Module Language and Boilerplate (Hello World):**
	- **Old Kernel:** Kernel modules were strictly written in C. Logging was done using the raw `printk()` function.
	- **Modern Kernel (v6.1+):** A massive architectural shift occurred. The kernel now officially supports writing loadable modules in Rust. This brings memory safety and prevents buffer overflows natively. For C modules, developers now strictly use modern logging macros like `pr_info()` or `dev_info()` instead of raw `printk`.

- **Tooling for Dependencies and Loading:**
	- **Old Kernel:** The tools like `insmod`, `modprobe`, and `depmod` were provided by an older software package called `module init tools`. They executed as heavy standalone binaries.
	- **Modern Kernel:** The entire module management stack was rewritten into a project called **kmod**. It provides a shared library (`libkmod`). This makes module loading significantly faster because system managers (like systemd or udev) can load modules directly via library calls without spawning new command line processes.

- **Security: Module Signing and Secure Boot:**
	- **Old Kernel:** would happily load almost all `.ko` file you provided as root.
	- **Modern Kernel:** Security is massive now. Modern ARM kernels use strict cryptographic module signing. If a module is not signed with a trusted key matching the kernel keyring, the system will outright refuse to load it to prevent rootkit attacks.
	- **How to sign:** You use the `scripts/sign-file` utility provided in the kernel source tree. You execute it by passing the hashing algorithm (like sha256), your private key, your public certificate, and the target module file.

- **Static vs Dynamic in Embedded:**
	- **Old Kernel:** The philosophy was to compile almost everything as a loadable module to save RAM on standard PCs.
	- **Modern Kernel:** In your daily work, core drivers (like eMMC controller, I2C bus, or vital regulators) are usually compiled directly into the kernel binary instead of being modules.
		- This is because the bootloader needs them instantly available to mount the root filesystem without waiting for user space tools to load them.

- **Exported Symbols Namespaces:**
	- **Old Kernel:** Using `EXPORT_SYMBOL` dumped the function into one massive global pool that any module could see.
	- **Modern Kernel:** The kernel now heavily uses Symbol Namespaces via `EXPORT_SYMBOL_NS()`.
		- Developers group functions into logical namespaces (like `USB_CORE` or `I2C_CORE`). Your modern module now has to explicitly import that exact namespace using `MODULE_IMPORT_NS()` to use those specific functions, keeping the kernel code incredibly organized and safe.


### The Device Model

- The Linux 2.6 kernel introduced a unified device model to represent devices and describe their topology (how they are connected).
- The primary motivation for building this complex tree was to facilitate system wide power management.
- It provides a single mechanism for common facilities like reference counting and power sequencing.

- **Q: Why does power management require a unified device model?**
	- The kernel must know the exact physical hierarchy to turn things off in the correct order.
		- To suspend the system without crashing, the kernel must power down the "leaf" nodes before the "root" nodes.
		- **Analogy:** Like shutting down a corporate office. You must turn off the individual desk computers (USB mice) before you cut power to the main floor circuit breaker (USB controller), and you cut the floor breaker before shutting off the main building generator (PCI bus).

- **Q: What are the other major benefits of this model?**
	- It organizes the chaotic physical hardware world into clean software abstractions.
		- It minimizes code duplication by centralizing common driver tasks.
		- It allows user space to enumerate all devices and view their status.
		- It allows the kernel to categorize devices by class (like "input devices") without needing to know exactly how they are wired to the motherboard.

#### Kobjects
- At the absolute heart of the device model is the kernel object, represented by `struct kobject`.
- It is very similar to the base "Object" class in object oriented programming languages like Java or C#.
- Kobjects provide basic facilities: reference counting, a name, and a parent pointer to create a hierarchy.

- **Q: Do driver developers use kobjects by themselves?**
	- Usually no, because a plain kobject does not do anything useful on its own.
		- Instead, they are embedded inside larger, more specific data structures (like a character device structure `struct cdev`).
		- By embedding a kobject, the larger structure automatically inherits standard hierarchical and reference counting features.

#### Ktypes
- Every kobject is associated with a specific kernel object type, represented by `struct kobj_type`.
- Ktypes describe the default behavior for a family of related kobjects so that every single object does not have to redefine the same rules.

- **Q: What exactly does the ktype structure define?**
	- It defines three primary shared properties.
		- **Release function:** A destructor pointer that is called automatically when the reference count drops to zero to free memory.
		- **Sysfs ops:** Defines how the virtual files for this object behave when read or written by a user.
		- **Default attributes:** Defines the default properties (sysfs files) that should be created when the object is exported to user space.

#### Ksets
- Ksets (kernel object sets) are aggregate collections that work as the base container class.
- They group related kernel objects together in a single place, such as "all block devices".
- Ksets provide a base class for the group and aggregate them into the same directory level in the sysfs filesystem.

- **Q: What is the difference between a kset and a ktype?**
	- One groups them logically, the other provides shared operations.
		- **Ktypes:** Group objects that need to share the exact same operations and release functions, regardless of where they sit in the device tree.
		- **Ksets:** Group related objects together into a single directory structure, even if they have different underlying operations.

- **Q: What is the uevent pointer inside the kset structure?**
	- It handles hot plug events.
		- Uevent stands for user events.
		- It is the mechanism for communicating with user space when a device is suddenly plugged in or removed from the system.

#### Interrelation of Kobjects, Ktypes, and Ksets
- These three simple structures are highly interrelated and work as an ecosystem.
- Kobjects are the foundational building blocks embedded in other structures.
- Ktypes give those blocks their default behaviors and destruction rules.
- Ksets gather those blocks into manageable groups and directory folders.
- In sysfs, kobjects are the individual directories in the filesystem. Related directories—say, perhaps all subdirectories of a given directory—might be in the same kset.

#### Managing and Manipulating Kobjects
- Driver writers usually let the subsystem handle kobjects entirely behind the scenes.
- However, if you must interact with them, you use exported kernel interfaces rather than manipulating the struct directly.

- **Q: How do you initialize a kobject manually?**
	- It is a two step manual process requiring careful memory clearing.
		- You must completely zero out the structure first using `memset` (setting all bits to zero).
		- Then you call `kobject_init()` passing the object and its associated ktype.
	- It is safe to set the parent and kset pointers only after zeroing.
	  ```c
		struct kobject *kobj;
		kobj = kmalloc(sizeof (*kobj), GFP_KERNEL);
		if (!kobj)
			return -ENOMEM;
		memset(kobj, 0, sizeof (*kobj));
		kobj->kset = my_kset;
		kobject_init(kobj, my_ktype);
	  ```
	- This multistep effort is handled automatically by kobject_create(), which returns a newly allocated kobject:
	  ```c
		struct kobject *kobj;
		kobj = kobject_create();
		if (!kobj)
			return –ENOMEM;
	  ```

- **Q: What is the preferred, safer way to create a kobject?**
	- Using the automated helper function.
		- Developers should strongly favor calling `kobject_create()`.
		- This automatically allocates the memory, zeroes it, and returns the initialized object in one step, preventing simple human errors.


#### Reference Counts
- The kobject system provides a unified reference counting system for the kernel.
- When a kobject is initialized, its reference count is set exactly to one.
- As long as the count is nonzero, the object is said to be "pinned" and remains safely in memory.


##### Incrementing and Decrementing Reference Counts

- **Q: How do you tell the kernel that you are using an object?**
	- You call a function to bump the count.
		- You use `kobject_get()` which increments the reference count.
		- Any code that holds a pointer to the object must elevate this count to ensure the object is not deleted while being used.

- **Q: What happens when you are finished with the object?**
	- You put it back.
		- You use `kobject_put()` which decrements the count.
		- If this function drops the count to zero, the object is no longer valid, the release function is invoked, and memory is freed immediately.

##### Krefs
- Internally, kobjects do not handle their own counting mathematics.
- They delegate this job to a specialized structure called a kref (`struct kref`).
- The kref contains a single atomic variable used to hold the reference count safely across multiple CPU cores.

- **Q: Why use a kref wrapper instead of a standard atomic variable directly?**
	- It forces developers to use a standardized, bug free pattern.
		- Writing custom reference counting using raw atomic variables often leads to subtle race conditions and memory leaks.
		- The kref API (`kref_init`, `kref_get`, `kref_put`) provides a common, widely tested, and known correct mechanism for the entire kernel.

#### Modern Usage

- **Hardware Topology: Hardcoded vs Device Tree**
	- **Old Kernel:** Especially for embedded ARM boards, the physical device topology (which I2C sensor connects to which bus) was hardcoded in massive, messy C files known as "board files". The kernel had to be recompiled for every minor hardware change.
	- **Modern Kernel in ARM:** Modern ARM systems completely abandoned board files in favor of the **Device Tree (DTS/DTB)**. The bootloader passes a compiled hardware map to the kernel at boot. The kernel parses this tree and dynamically builds this exact kobject/kset hierarchy on the fly. You never hardcode topology in modern ARM drivers.

- **Power Management Evolution (Runtime PM)**
	- **Old Kernel:** The device tree was primarily used for system wide suspend (putting the whole laptop to sleep).
	- **Modern Kernel in ARM:** Modern embedded systems (like cell phones) cannot afford to wait for a system wide sleep. They use Runtime PM (Runtime Power Management) and GenPD (Generic Power Domains). The kernel aggressively uses the kobject hierarchy to power down specific leaves (like shutting down a camera sensor and its specific voltage regulator) the exact millisecond the user closes an app, while the rest of the system stays fully awake.

- **Memory Safety and Rust**
	- **Old Kernel:** Using `kobject_get()` and `kobject_put()` required strict discipline. If a C developer forgot a `put()` call on an error path, the reference count would never hit zero, causing a permanent memory leak.
	- **Modern Kernel (v6.1+):** With the introduction of Rust into the kernel, the kref system is abstracted using Rust's smart pointers (like `Arc` for atomic reference counting). The Rust compiler automatically guarantees that the reference count is decremented when the object goes out of scope, physically preventing memory leaks at compile time.


### sysfs

- The sysfs filesystem is an in-memory virtual filesystem.
- It provides a user space view of the internal kobject hierarchy (the device model).
- It allows users to view device topology and read/write kernel variables via simple text files.
- It is normally mounted at the `/sys` directory on modern Linux systems.

- **Q: How does sysfs magically mirror the internal kernel device tree?**
	- It maps kobjects directly to filesystem directory entries (dentries).
		- Because kobjects already form a parent/child tree in memory, turning them into a directory tree is trivial.
		- Every time a kobject is added to the device model, a corresponding directory folder appears in `/sys`.

- **Q: What are the most important root directories inside /sys?**
	- There are several, but they organize the same data in different ways.
		- `/sys/devices`: This is the most important directory. It exports the actual, physical device topology of the hardware tree.
		- `/sys/class`: Organizes devices by high level function (like "net" for network cards or "input" for keyboards) regardless of how they are physically wired.
		- `/sys/bus`: Organizes devices based on the physical bus they attach to (like PCI, I2C, or USB).
		- `/sys/dev` directory is a view of registered device nodes.
		- `/sys/firmware` directory contains a system-specific tree of low-level subsystems such as ACPI, EDD, EFI, and so on.
		- The `/sys/fs` directory contains a view of registered filesystems. The `/sys/kernel`directory contains kernel configuration options and status information while the `/sys/modules` directory contains a view of the system’s loaded modules.
		- `/sys/power` directory contains system-wide power management data.
		- Many directories outside of `/sys/devices` simply contain symbolic links pointing back to the real physical hardware inside the `devices` folder.

#### Adding and Removing kobjects from sysfs
- Initializing a kobject inside the kernel does not automatically make it visible in `/sys`. You must explicitly export it to user space.

- **Q: How do you make a kobject visible as a directory in sysfs?**
	- You use `kobject_add()`.
	- The location of the new directory depends on the kobject's parent pointer. If it has a parent, it becomes a sub-directory of that parent.
	- If you want to create and add the object in one step, you can use the helper function `kobject_create_and_add()`.

- **Q: How do you remove a kobject's directory from sysfs?**
	- You use `kobject_del()`.
	- This removes the directory and all its files from user space, but does not free the internal kernel memory (that relies on reference counting).

#### Adding Files to sysfs
- Kobjects map to directories, but directories are empty without files to provide actual data.
- In sysfs, files are called "attributes".
- Attributes map internal kernel data variables to user readable text files.

##### Default Attributes
- Most kobjects get their files automatically via their assigned ktype.
- The `struct kobj_type` contains a default attributes array.
- This ensures all objects of the same type (like all I2C sensors) have the exact same standard files (like a `power_state` file).

- **Q: How does the kernel know what to do when a user reads or writes a sysfs file?**
	- It uses the `sysfs_ops` structure defined in the ktype.
		- This structure contains two function pointers: `show()` and `store()`.
		- `show()`: Invoked when user space reads the file. It must copy the internal kernel variable into a text buffer to print to the screen.
		- `store()`: Invoked when user space writes to the file. It must parse the incoming text string and update the internal kernel variable.

##### Creating New Attributes
- Sometimes a specific device needs a unique file that is not part of the standard default attributes.

- **Q: How do you create a custom file in a sysfs directory?**
	- You use the sysfs creation API.
		- You call `sysfs_create_file()`, passing the kobject and a custom attribute structure.
		- You can also create symbolic links using `sysfs_create_link()`.

##### Destroying Attributes

- **Q: How do you remove a custom file when the device is unplugged?**
	- You use the removal API.
		- You call `sysfs_remove_file()` to delete a standard attribute.
		- You call `sysfs_remove_link()` to delete a symbolic link.

#### sysfs Conventions
- Sysfs is designed to replace messy, undocumented `ioctl()` calls and the chaotic `/proc` filesystem for device management.
- To keep sysfs clean, kernel developers strictly enforce certain rules.

- **Q: What are the strict rules for creating sysfs files?**
	- There are three major conventions you must follow.
		- **One value per file:** A sysfs file should export a single, text based value. You should never format complex tables or multi value strings in a single file. It should be easy for a basic shell script to read.
		- **Clean hierarchy:** Keep the data organized intuitively. Only put attributes in folders where they logically belong.
		- **Stable ABI:** Sysfs is considered a user space Application Binary Interface. Once you create a file and user space scripts rely on it, you cannot rename it or change how it behaves, or you will break user applications.


### The Kernel Events Layer

- The kernel needs a way to push asynchronous events up to user space (e.g., "Hard drive is full!" or "USB device plugged in!").
- This is modeled using kobjects and sysfs paths.
- Because every kobject maps to a sysfs path, the sysfs path acts as the "source address" for the event.

- **Q: How do these events actually travel from kernel space to user space?**
	- They use a high speed networking socket.
		- The kernel uses a **Netlink** multicast socket.
		- User space runs a daemon (like `udev` or `systemd-udevd`) that constantly listens on this socket for incoming hardware events.

- **Q: How does a driver send an event?**
	- The driver calls `kobject_uevent()`.
	- It passes the kobject (the source) and an action verb (like `KOBJ_ADD` for a plug in event or `KOBJ_REMOVE` for an unplug event).
	  
	  ```mermaid
		sequenceDiagram
		participant Hardware as USB Sensor
		participant Kernel as Kernel (kobject)
		participant Netlink as Netlink Socket
		participant UserSpace as User Space (udev)
		
		Hardware->>Kernel: Device physically plugged in
		Kernel->>Kernel: kobject_create_and_add()
		Kernel->>Kernel: Directory appears in /sys/devices/...
		Kernel->>Netlink: kobject_uevent(KOBJ_ADD)
		Netlink->>UserSpace: Broadcasts "ADD" event + sysfs path
		UserSpace->>UserSpace: udev script runs (creates /dev/ node)
	  ```


#### Modern Usage

- **Managing Device Nodes (udev vs devtmpfs)**
	- **Old Kernel:** In the early 2.6 days, user space relied entirely on `udev` listening to Netlink events to manually create character and block device nodes in `/dev/`. If `udev` crashed, your hardware nodes never appeared.
	- **Modern Kernel:** To solve a chicken-and-egg boot problem, modern kernels use **devtmpfs**. The kernel itself automatically creates the `/dev/` node the exact instant the driver calls `device_add()`, entirely bypassing user space. `udev` still exists, but mostly just to apply custom symlinks or file permissions after `devtmpfs` has already done the heavy lifting.

- **Modern Kernel in ARM (sysfs for GPIO and PWM)**
	- **Old Kernel:** Sysfs was mostly used for complex things like PCI buses and power management parameters.
	- **Modern Kernel:** Sysfs became the absolute standard for quick-and-dirty embedded prototyping. You can export raw GPIO pins to sysfs (via `/sys/class/gpio`) to toggle LEDs directly from bash scripts, or control motor speeds via `/sys/class/pwm`. (Note: The sysfs GPIO interface is currently deprecated in favor of the newer character device GPIO ABI, but sysfs remains heavily used in legacy embedded systems).

- **ABI Strictness and Documentation**
	- **Old Kernel:** Developers would often throw new sysfs files into the kernel without much oversight, leading to a messy, fragmented user experience.
	- **Modern Kernel:** The rules for sysfs ABI stability are now heavily enforced. If you want to add a new sysfs attribute to the kernel today, you are strictly required to write corresponding documentation in the `Documentation/ABI/` directory explaining exactly what the file does, its expected values, and who maintains it, before your patch will be accepted.


## **Quick Recall**

- **Q: What are the 4 main device types in Linux?**
	- Character (byte stream),
	- Block (fixed chunks),
	- Network (packets), and
	- Miscellaneous ("misc" simplified char devices).

- **Q: What is the file extension of a compiled kernel module?**
	- `.ko` (Kernel Object).

- **Q: What are the two mandatory functions in any module?**
	- The entry point (`module_init`) and the exit point (`module_exit`).

- **Q: What does the "tainted" kernel flag mean?**
	- A warning that non-GPL or unsupported code was loaded, telling developers the crash might not be their fault.

- **Q: `insmod` vs `modprobe`?**
	- `insmod` blindly loads one file; `modprobe` is smart and automatically resolves/loads dependencies first.

- **Q: What file stores module dependencies, and what tool generates it?**
	- `modules.dep`, generated by the `depmod` tool.

- **Q: In Kconfig, what does `tristate` allow?**
	- Three choices: Built-in (Y), Module (M), or Excluded (N).

- **Q: How do you pass a parameter to a _built-in_ driver (not a module)?**
	- Via the bootloader command line (e.g., `drivername.param=value`).

- **Q: `EXPORT_SYMBOL` vs `EXPORT_SYMBOL_GPL`?**
	-  The former shares a function with _all_ modules; the latter restricts it _only_ to GPL-licensed modules.

- **Q: What is the base "object" at the heart of the Device Model?**
	- The `kobject` (kernel object).

- **Q: What happens when a kobject's reference count (kref) hits zero?**
	- The `release` function is called, and the memory is freed.

- **Q: What is `sysfs` and where is it mounted?**
	- A virtual filesystem exposing the kobject hierarchy to userspace, mounted at `/sys`.

- **Q: How does the kernel notify userspace (udev) that a device was plugged in?**
	- By sending a `uevent` over a Netlink socket.

## **Hands-on Ideas**

1. **The "Hello World" Lifecycle (BBB or Qualcomm)**
	- Write a basic module with `module_init`/`module_exit` using `pr_info()`.
	- Cross-compile it for ARM. Load with `insmod`, watch the message with `dmesg`, then `rmmod`.
	- _Goal: Master the basic build-load-unload cycle on the target._

2. **Interactive Module Parameters (BBB or Qualcomm)**
	- Add an `int` parameter using `module_param(delay, int, 0644)`.
	- Load it with a value: `insmod mymod.ko delay=5`.
	- _Goal: Verify the value, then navigate to `/sys/module/mymod/parameters/` and try changing it live via `echo`._

3. **Explore the Real Device Tree via sysfs (Best on Qualcomm SA525)**
	- The SA525 is a rich SoC. Explore `/sys/bus/` and `/sys/class/`.
	- Run `ls -l /sys/class/net/` and follow the symlinks back to the physical device in `/sys/devices/`.
	- _Goal: Physically see how `sysfs` links "function" (class) to "physical location" (devices)._

4. **Toggle an LED via sysfs (BeagleBone Black)**
	- The BBB has 4 user LEDs perfect for this. Navigate to `/sys/class/leds/`.
	- Manually turn one on/off: `echo 1 > /sys/class/leds/beaglebone:green:usr0/brightness`.
	- _Goal: Interact with a real driver's attributes without writing any C code._

5. **Monitor Live uevents (BBB or Qualcomm)**
	- Run `udevadm monitor --kernel` in a terminal.
	- Plug/unplug a USB drive (BBB) or trigger a modem event (Qualcomm)
	- _Goal: Watch the raw Netlink `KOBJ_ADD`/`KOBJ_REMOVE` events fire in real-time._

6. **Create a Custom sysfs Attribute (Advanced - BBB or Qualcomm)**
	- Extend your Hello World module to create a custom file using `kobject_create_and_add()` and `sysfs_create_file()`.
	- Implement the `show()` and `store()` functions to read/write an integer.
	- _Goal: Build your own userspace-to-kernel communication channel._