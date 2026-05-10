# AOS

x86 / ARM  kernel written in C++17. AOS boots under QEMU with GRUB, runs in 64-bit long mode, supports preemptive multitasking, demand paging with swap, a MINIX-based filesystem, and a POSIX-style userspace with pthreads, pipes, shared memory and `mmap`.

The project is intentionally compact — so the entire boot path, scheduler, MM and syscall layer fit in your head. It is designed to be read, and extended.

## Features

**Kernel**
- Multiboot boot path → long mode → C++ kernel (`x86/64`, `x86/32/pae`, `x86/32`, `arm`, `armv8`)
- Preemptive round-robin scheduler with kernel + user threads
- Dedicated `IdleThread`, `CleanupThread`, `SwappingThread`, `LogFlushThread`
- Synchronization: `SpinLock`, `Mutex`, `Condition`, `ScopeLock`
- Inverted page table, page-fault driven demand paging
- Heap allocator (`KernelHeap` / `kmalloc` / placement-new)
- Swap engine backed by a dedicated disk partition (LRU-style page-out under pressure)
- Shared memory registry (`shm_*`)
- VFS layer with pluggable filesystems: `ramfs`, `devicefs`, `minixfs`
- Block-device abstraction (`BlockDev`, IDE driver)
- Per-process file slot table; `dup`, `dup2`, pipes, redirection
- Loader for ELF user binaries with on-demand page loading
- App registry, `fork` / `exec` / `exit` / `wait`

**Userspace**
- Minimal libc with `pthread_*`, `mutex`, `cond`, `semaphore`, `spinlock`
- `fork`, `exec*`, `exit`, `sleep`, `usleep`, `clock`, `yield`
- `mmap`, `munmap`, `mprotect`, `brk`/`sbrk`, shared memory
- TLS, ASLR, guard pages, non-executable stack, write-protected code
- 100+ self-contained test programs under [`user/tests/`](user/tests/) covering every syscall and edge case

## Repository layout

```
arch/             per-architecture boot, paging, interrupt and context-switch code
core/             portable kernel sources and headers
  incl/
    kernel/      scheduler, threads, locks, MM, syscall layer
    memory/      page manager, kernel heap, swap, shared memory
    filesystem/  VFS, minixfs, ramfs, devicefs, block devices
    display/     text + framebuffer console
    utility/     mini-STL containers, kprintf, debug helpers
    aostl/       freestanding container library
  src/           matching .cpp files
user/
  libc/          freestanding libc, pthread, syscall stubs
  tests/         standalone test programs
utils/
  aos-mkimg/     host tool that packs binaries into the disk image
  aos-dwarf/     embeds DWARF info into the kernel image
  images/        GRUB config + base disk template
cmake/           custom CMake modules
```

## Requirements

- Linux host 
- `gcc` / `g++` 11 or newer
- `cmake` 3.10+
- `qemu-system-x86` (and `qemu-system-arm` for ARM targets)
- `make`, `python3`, `build-essential`

```bash
sudo apt install cmake qemu-system-x86 gcc g++ python3 build-essential
```

## Build

The build is fully out-of-tree. In-source builds are disabled. Using a `tmpfs` build directory (e.g. `/tmp`) is strongly recommended - the build is faster and avoids hammering the disk.

> **Disclaimer:** This software is provided "as is", without warranty of any kind, express or implied. AOS is a kernel that boots real CPUs and writes raw disk images - building, running or modifying it is done entirely at your own risk. The author is **not liable** for any damage, data loss, hardware issues or other consequences that may result from using this project.

```bash
git clone <this-repo> aos
cd aos

mkdir -p /tmp/aos
cd /tmp/aos
cmake <path-to-aos-source>
make -j$(nproc)
```

Or use the helper script from the source root:

```bash
./setup_cmake.sh           # generates Makefiles in /tmp/aos
make -C /tmp/aos -j$(nproc)
```

The first build takes 1–3 minutes. Incremental rebuilds are seconds. Output artifacts live in the build directory:

| File | Description |
| --- | --- |
| `kernel.x` | Multiboot ELF stub loaded by GRUB |
| `kernel64.x` | The 64-bit kernel image |
| `AOS.qcow2` | Bootable disk image (GRUB + kernel + userspace) |
| `aos-mkimg` | Host tool that packs binaries into the image |
| `user/tests/*.elf` | All compiled test programs |

### Architecture targets

The default target is `x86/64`. To pick a different one:

```bash
cmake -DARCH=x86/32       <src>   # i386
cmake -DARCH=x86/32/pae   <src>   # i386 with PAE
cmake -DARCH=arm/rpi2     <src>   # Raspberry Pi 2
cmake -DARCH=armv8/rpi3   <src>   # Raspberry Pi 3 (64-bit)
```

Or use the convenience targets:

```bash
make x86_64
make x86_32
make armv8_rpi3
```

### Debug build

```bash
make debug      # reconfigures with -DDEBUG=1
```

This keeps full DWARF info in the kernel and enables verbose debug logging.

## Run

From the build directory:

```bash
make qemu       # boot in QEMU (graphical window + serial debugcon → stdout)
make kvm        # same, with KVM acceleration
make qemutacos  # headless, all I/O over stdio (good for SSH)
```

You should see the kernel boot, all kernel threads start, the VFS mount, and finally `/usr/shell.elf` come up. From the shell you can launch any test:

```
$ helloworld.elf
$ pthread_create_simple_1.elf
$ swap.elf
$ shell.elf
```

### Debugging with GDB

In one terminal:

```bash
make qemugdb    # QEMU starts and waits on localhost:1234
```

In another:

```bash
make rungdb     # plain gdb
make runcgdb    # cgdb
make rungdbgui  # browser-based gdbgui
make runddd     # ddd
```

The supplied [`utils/gdbinit`](utils/gdbinit) sets up the symbol file and connects to port 1234 automatically.

## Other targets

```bash
make clean      # remove object files
make mrproper   # nuke the build directory and re-run cmake
make showhelp   # full target list
```

## Writing a userspace test

1. Drop a `.c` file into [`user/tests/`](user/tests/) (or a subdirectory for multi-file tests).
2. Rebuild — the binary is automatically packaged into the disk image.
3. `make qemu`, then run it from the AOS shell.

Existing tests under [`user/tests/`](user/tests/) are the best reference: each one is small and focused on a single syscall or feature.

## Adding a syscall

1. Reserve a number in [`core/incl/kernel/syscall-definitions.h`](core/incl/kernel/syscall-definitions.h).
2. Implement the handler in [`core/src/kernel/Syscall.cpp`](core/src/kernel/Syscall.cpp).
3. Add the userspace stub in [`user/libc/`](user/libc/).
4. Write a test under [`user/tests/`](user/tests/).
