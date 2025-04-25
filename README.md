# Memory Violation Exploration

## Overview

This project demonstrates controlled and safe access testing of memory regions such as the stack and heap. It is designed to probe platform-specific memory protection boundaries and explore how operating systems handle segmentation faults and access violations. The program is cross-platform and supports Linux, macOS, and Windows.

## Problem Description

Accessing memory beyond the bounds of allocated stack or heap regions typically results in segmentation faults or undefined behavior. This project safely attempts to read from various memory addresses at the edges and beyond the allocated memory segments and uses platform-specific mechanisms to:
- Detect stack and heap boundaries.
- Attempt memory reads at strategic offsets (start, middle, end, ±1).
- Catch segmentation faults using `sigsetjmp/siglongjmp` (POSIX) or SEH (`__try/__except`) on Windows.
- Report whether access was successful or blocked by the OS.

This exploration helps visualize how the system maps and protects memory, and reveals subtle platform-specific behaviors related to page alignment, lazy mapping, and guard pages.

## Explanation of Some Topics

- **Stack vs Heap**: The stack holds automatic variables and grows downwards; the heap stores dynamically allocated memory and grows upwards. Each is managed by the OS and bounded by safety margins.
- **Memory Pages and Access Violations**: Modern OSes use paging (typically 4 KB per page). Unmapped pages or guard pages result in SIGSEGV or similar signals.
- **Signal Handling**: 
  - On **Linux** and **macOS**, illegal memory accesses are caught using `signal(SIGSEGV, handler)` and execution is restored via `siglongjmp`.
  - On **Windows**, instead of `signal()`, the project uses `__try { } __except { }` blocks (Structured Exception Handling, SEH).  
    This is necessary because Windows does not deliver `SIGSEGV` via standard C signals; access violations must be handled using system-level structured exceptions.  
    This ensures that illegal memory accesses are caught and handled cleanly without crashing the program on Windows.

- **Cross-platform Memory Bound Detection**:
  - On Linux/macOS: memory bounds are parsed from `/proc/self/maps` or queried using virtual memory APIs.
  - On Windows: `VirtualQuery` is used to inspect and locate the memory layout of the stack and heap.

## Example Output

```
=== Memory Violation Exploration ===

Stack  : 0x7ffe00000 - 0x7ffffc000
Heap   : 0x6f0000 - 0x741000

[*] Attempting to access memory in various regions...

[*] Testing Stack range: 0x7ffe00000 - 0x7ffffc000
[*] Stack start - 1: 0x7ffdfffff
    [*] Setting up sigsetjmp for address: 0x7ffdfffff
    [*] Address 0x7ffdfffff is not readable
    [!] Skipping access to non-readable address 0x7ffdfffff

[*] Stack start: 0x7ffe00000
    [*] Setting up sigsetjmp for address: 0x7ffe00000
    [*] Address 0x7ffe00000 is not readable
    [!] Skipping access to non-readable address 0x7ffe00000

[*] Stack middle: 0x7ffefe000
    [*] Setting up sigsetjmp for address: 0x7ffefe000
    [*] Address 0x7ffefe000 is not readable
    [!] Skipping access to non-readable address 0x7ffefe000

[*] Stack end - 1: 0x7ffffbfff
    [*] Setting up sigsetjmp for address: 0x7ffffbfff
    [*] Address 0x7ffffbfff is readable
    [*] Attempting to access memory at address: 0x7ffffbfff
    [*] Value: 0

[*] Stack end: 0x7ffffc000
    [*] Setting up sigsetjmp for address: 0x7ffffc000
    [*] Address 0x7ffffc000 is readable
    [*] Attempting to access memory at address: 0x7ffffc000
    [*] Value: 0

[*] Stack end + 1: 0x7ffffc001
    [*] Setting up sigsetjmp for address: 0x7ffffc001
    [*] Address 0x7ffffc001 is readable
    [*] Attempting to access memory at address: 0x7ffffc001
    [*] Value: 0

[*] Testing Heap range: 0x6f0000 - 0x741000
[*] Heap start - 1: 0x6effff
    [*] Setting up sigsetjmp for address: 0x6effff
    [*] Address 0x6effff is not readable
    [!] Skipping access to non-readable address 0x6effff

[*] Heap start: 0x6f0000
    [*] Setting up sigsetjmp for address: 0x6f0000
    [*] Address 0x6f0000 is readable
    [*] Attempting to access memory at address: 0x6f0000
    [*] Value: 0

[*] Heap middle: 0x718800
    [*] Setting up sigsetjmp for address: 0x718800
    [*] Address 0x718800 is readable
    [*] Attempting to access memory at address: 0x718800
    [*] Value: 0

[*] Heap end - 1: 0x740fff
    [*] Setting up sigsetjmp for address: 0x740fff
    [!] Address 0x740fff is too close to region end 0x741000
    [!] Skipping access to non-readable address 0x740fff

[*] Heap end: 0x741000
    [*] Setting up sigsetjmp for address: 0x741000
    [*] Address 0x741000 is not readable
    [!] Skipping access to non-readable address 0x741000

[*] Heap end + 1: 0x741001
    [*] Setting up sigsetjmp for address: 0x741001
    [*] Address 0x741001 is not readable
    [!] Skipping access to non-readable address 0x741001
```

## Explanation of Output

### **Stack Memory Access Results**

- **`stack_start - 1`, `stack_start`, `stack_middle`**  
  These addresses are **unreadable**. The OS typically reserves stack space using virtual memory, but only **commits pages near the top** (where the actual stack is active).  
  The lower pages (near `stack_start`) are often **guarded** or uncommitted, triggering a segmentation fault on access.

- **`stack_end - 1`, `stack_end`, `stack_end + 1`**  
  These addresses are **readable** because they lie within **the currently mapped page(s)** of the active stack:
  - `stack_end` reflects the upper limit of the reserved stack region.
  - `stack_end - 1` and `stack_end` are part of that top-most mapped page.
  - `stack_end + 1` appears **surprisingly readable**, but this is because it **still falls inside the same memory page** (4 KB on most systems).  
    Even though it's beyond the logical stack end, it's **not outside the actual mapped physical page**, and thus doesn't trigger a fault.
  
  > **Why it works:** Page alignment. A page may range from `0x7ffffc000` to `0x7ffffcfff`. So even `0x7ffffc001` is inside that valid page.

---

### **Heap Memory Access Results**

- **`heap_start - 1`**  
  Falls outside the allocated heap segment. Accessing this address causes a segmentation fault, as expected.

- **`heap_start`, `heap_middle`**  
  Fall safely inside the committed heap range — these are readable and return values (likely zero-initialized).

- **`heap_end - 1`, `heap_end`, `heap_end + 1`**  
  These addresses lie **on or beyond the boundary** of the heap segment:
  - May fall into guard pages or unmapped memory.
  - OS often protects these boundary pages to catch overflows or illegal access.

---

## How to Compile and Run

### 1. Clone the Repository
```bash
git clone https://github.com/LyudmilaKostanyan/Kernel-Space-Access.git
cd Kernel-Space-Access
```

### 2. Build the Project
Use CMake to build the project:

```bash
cmake -S . -B build
cmake --build build
```

Ensure you have CMake and a C++ compiler (e.g., `g++`, `clang++`, or MSVC) installed.

### 3. Run the Program

#### For Windows Users
```bash
./build/main.exe
```

#### For Linux/macOS Users
```bash
./build/main
```
