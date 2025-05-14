# Chapter 2: Building the Kernel

## Summary
- Kernel is built from source using a configuration system and `make`.
- Config options are saved in `.config`. Customize with:
  ```bash
  make menuconfig
  ```
- Kernel is compiled via:
  ```bash
  make -j$(nproc)
  ```
- Modules are built and installed separately.
- Useful tools: `lsmod`, `modinfo`, `insmod`, `rmmod`.

## Quick Recall
- What file stores kernel configuration?
- How do you compile only modules?
- Difference between built-in and loadable modules?
- Where is the entry point for kernel build?

## Hands-On Ideas
- On your PC or BBB:
  - Try building a `hello_world` kernel module.
  - Install kernel headers if not already installed.
- Sample Makefile and source:
  ```c
  // hello.c
  #include <linux/init.h>
  #include <linux/module.h>
  MODULE_LICENSE("GPL");
  static int __init hello_init(void) { printk("Hello, BBB\n"); return 0; }
  static void __exit hello_exit(void) { printk("Bye, BBB\n"); }
  module_init(hello_init); module_exit(hello_exit);
  ```
  ```Makefile
  obj-m += hello.o
  all:
    make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
  ```

- Load and verify on BBB:
  ```bash
  insmod hello.ko
  dmesg | tail
  rmmod hello
  ```
