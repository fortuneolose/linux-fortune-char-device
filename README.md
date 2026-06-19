# Linux Fortune Character Device Driver

`linux-fortune-char-device` is a minimal educational Linux kernel module that implements a character device driver. When the module is loaded, it creates:

```text
/dev/fortune_device
```

Userspace programs can read a fortune string from the device and write a new fortune string back to it. The project is intentionally small, but it follows the normal Linux character-device path: dynamic device-number allocation, `struct cdev`, file operations, kernel/user copying helpers, and kernel log debugging with `dmesg`.

## Features

- Loadable Linux kernel module written in C
- Creates `/dev/fortune_device`
- Implements `open()`, `release()`, `read()`, and `write()`
- Stores one fortune message in kernel memory
- Protects shared state with a kernel mutex
- Uses `copy_to_user()` and `copy_from_user()` for safe kernel/user transfers
- Allocates a dynamic major number with `alloc_chrdev_region()`
- Creates the device node automatically with `class_create()` and `device_create()`
- Includes Makefile targets for build, load, unload, test, logs, and clean

## Repository Layout

```text
.
|-- fortune_device.c
|-- Makefile
|-- README.md
|-- .gitignore
`-- .gitattributes
```

| File | Purpose |
| --- | --- |
| `fortune_device.c` | Linux kernel module source code |
| `Makefile` | Builds the module using the running kernel build system |
| `README.md` | Project explanation, build instructions, testing notes, and debugging guide |
| `.gitignore` | Ignores kernel build artifacts and editor files |
| `.gitattributes` | Keeps source files and Makefile line endings as LF |

## Requirements

Use a Linux machine, Linux VM, or configured kernel-development environment with:

- Linux kernel headers for the running kernel
- `make`
- `gcc`
- `sudo`

On Debian or Ubuntu:

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)
```

Confirm the kernel headers exist:

```bash
ls /lib/modules/$(uname -r)/build
```

If that path does not exist, the module cannot be built for the currently running kernel until the matching headers are installed.

## Build

From the repository root:

```bash
make
```

The Makefile delegates the build to the Linux kernel build system:

```bash
make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
```

Successful builds produce:

```text
fortune_device.ko
```

This `.ko` file is the loadable kernel object.

## Load The Module

Load the module with:

```bash
sudo insmod fortune_device.ko
```

Or use the Makefile target:

```bash
make load
```

Confirm the module is loaded:

```bash
lsmod | grep fortune_device
sudo dmesg | tail -n 20
```

Expected kernel log output includes a message similar to:

```text
fortune_device: loaded with major=<number> minor=0
```

If udev is running, the driver should create:

```bash
ls -l /dev/fortune_device
```

Example:

```text
crw------- 1 root root <major>, 0 ... /dev/fortune_device
```

## Read From The Device

Read the default fortune:

```bash
sudo cat /dev/fortune_device
```

Expected output:

```text
Fortune favors the prepared kernel hacker.
```

The driver's `read()` implementation supports normal file offset behavior. After userspace reads all available bytes, a later read on the same open file descriptor returns `0`, which signals end-of-file.

## Write To The Device

Write a new fortune:

```bash
echo "Kernel space is not user space." | sudo tee /dev/fortune_device
```

Read it back:

```bash
sudo cat /dev/fortune_device
```

Expected output:

```text
Kernel space is not user space.
```

The device buffer is 256 bytes including the trailing NUL byte. A write longer than 255 bytes is rejected with `-EMSGSIZE`, so the existing fortune is not partially replaced.

## Test With dmesg

Run the included test target:

```bash
make test
```

The target:

1. Reads the default fortune from `/dev/fortune_device`.
2. Writes a new fortune.
3. Reads the updated fortune.
4. Prints recent kernel log messages with `dmesg`.

Manual version:

```bash
sudo cat /dev/fortune_device
echo "The kernel keeps its own address space." | sudo tee /dev/fortune_device
sudo cat /dev/fortune_device
sudo dmesg | tail -n 30
```

Expected log messages include:

```text
fortune_device: loaded with major=<number> minor=0
fortune_device: opened
fortune_device: stored <number> byte fortune
fortune_device: closed
```

## Demo Output

The exact kernel version, build path, major number, timestamps, and file permissions can vary by machine. A successful run on a Linux VM looks like this:

```bash
$ make
make -C /lib/modules/6.8.0-31-generic/build M=/home/fortune/linux-fortune-char-device modules
make[1]: Entering directory '/usr/src/linux-headers-6.8.0-31-generic'
  CC [M]  /home/fortune/linux-fortune-char-device/fortune_device.o
  MODPOST /home/fortune/linux-fortune-char-device/Module.symvers
  CC [M]  /home/fortune/linux-fortune-char-device/fortune_device.mod.o
  LD [M]  /home/fortune/linux-fortune-char-device/fortune_device.ko
  BTF [M] /home/fortune/linux-fortune-char-device/fortune_device.ko
make[1]: Leaving directory '/usr/src/linux-headers-6.8.0-31-generic'

$ sudo insmod fortune_device.ko

$ lsmod | grep fortune_device
fortune_device         12288  0

$ ls -l /dev/fortune_device
crw------- 1 root root 240, 0 Jun 19 18:42 /dev/fortune_device

$ sudo cat /dev/fortune_device
Fortune favors the prepared kernel hacker.

$ echo "Kernel space is not user space." | sudo tee /dev/fortune_device
Kernel space is not user space.

$ sudo dmesg | tail -n 30
[ 3482.091347] fortune_device: loaded with major=240 minor=0
[ 3491.255083] fortune_device: opened
[ 3491.255102] fortune_device: closed
[ 3504.812774] fortune_device: opened
[ 3504.812793] fortune_device: stored 32 byte fortune
[ 3504.812810] fortune_device: closed

$ sudo rmmod fortune_device
```

## Unload And Clean

Unload the module:

```bash
sudo rmmod fortune_device
```

Or:

```bash
make unload
```

Clean generated build files:

```bash
make clean
```

Generated kernel build artifacts include files such as `.o`, `.ko`, `.mod`, `.mod.c`, `.symvers`, `.order`, and `.cmd` files.

## How The Driver Works

The driver registers a character device with the kernel and supplies a `struct file_operations` table:

```c
static const struct file_operations fortune_fops = {
	.owner = THIS_MODULE,
	.open = fortune_open,
	.release = fortune_release,
	.read = fortune_read,
	.write = fortune_write,
	.llseek = no_llseek,
};
```

When a userspace process runs `cat /dev/fortune_device`, the kernel routes that operation to the driver's `read()` callback. When a process writes with `tee` or another userspace program, the kernel routes that operation to the driver's `write()` callback.

## Memory Model

The fortune is stored in a static kernel buffer:

```c
static char fortune_buffer[256];
```

That buffer belongs to kernel space. It is not part of the address space of the process running `cat`, `echo`, or `tee`.

The driver also tracks how many bytes are valid:

```c
static size_t fortune_size;
```

Because multiple processes can open the device at the same time, the shared buffer is protected by a kernel mutex:

```c
static DEFINE_MUTEX(fortune_lock);
```

The mutex ensures that a read does not observe the buffer halfway through a write.

## Kernel/User Boundary

Kernel code must not directly dereference userspace pointers. The pointer passed to `read()` points to userspace memory:

```c
char __user *user_buffer
```

The pointer passed to `write()` also points to userspace memory:

```c
const char __user *user_buffer
```

The `__user` annotation documents that the pointer is not a normal kernel pointer.

The driver uses Linux helper functions for safe copying:

```c
copy_to_user()
copy_from_user()
```

Read path:

```text
kernel buffer -> copy_to_user() -> userspace buffer
```

Write path:

```text
userspace buffer -> copy_from_user() -> temporary kernel buffer -> device buffer
```

If either copy fails, the driver returns `-EFAULT`.

## Module Loading Lifecycle

On load, `fortune_init()`:

1. Allocates a dynamic device number with `alloc_chrdev_region()`.
2. Initializes a `struct cdev` with `cdev_init()`.
3. Registers the character device with `cdev_add()`.
4. Creates a device class with `class_create()`.
5. Creates `/dev/fortune_device` with `device_create()`.

On unload, `fortune_exit()` reverses those steps:

1. Removes `/dev/fortune_device` with `device_destroy()`.
2. Destroys the class with `class_destroy()`.
3. Deletes the character device with `cdev_del()`.
4. Releases the allocated device number with `unregister_chrdev_region()`.

The cleanup order matters because kernel resources should be released in the opposite order they were acquired.

## Debugging Guide

Watch kernel logs live:

```bash
sudo dmesg -w
```

Show recent logs:

```bash
sudo dmesg | tail -n 50
```

Check whether the module is loaded:

```bash
lsmod | grep fortune_device
```

Inspect module metadata:

```bash
modinfo fortune_device.ko
```

Check whether the device node exists:

```bash
ls -l /dev/fortune_device
```

Unload the module:

```bash
sudo rmmod fortune_device
```

## Common Issues

### Missing kernel headers

Error:

```text
linux/module.h: No such file or directory
```

Fix:

```bash
sudo apt install linux-headers-$(uname -r)
```

### Module loading is blocked

Error:

```text
insmod: ERROR: could not insert module fortune_device.ko: Operation not permitted
```

Possible causes:

- `insmod` was run without `sudo`
- Secure Boot is blocking unsigned kernel modules
- The environment does not allow loading custom kernel modules

### Device node does not appear

Check:

```bash
sudo dmesg | tail -n 50
```

Possible causes:

- The module failed to load
- udev is not running
- `device_create()` failed

### Module cannot unload

Error:

```text
rmmod: ERROR: Module fortune_device is in use
```

Cause:

A process still has `/dev/fortune_device` open. Close the process or shell command using the device, then retry:

```bash
sudo rmmod fortune_device
```

## Important Safety Note

Kernel modules run in kernel mode. A bug in a kernel module can crash the whole system. Build and test experimental drivers in a VM when possible.

## License

This module declares:

```c
MODULE_LICENSE("GPL");
```

Use it as an educational starting point for learning Linux character device drivers.
