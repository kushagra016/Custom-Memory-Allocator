# Custom Memory Allocator

---

## Project Overview

This project is a C++ based Custom Memory Allocator designed to replace standard operating system memory management. Instead of relying on general-purpose runtime calls like `new` or `malloc()` for every individual memory request, this allocator pre-allocates a massive contiguous chunk of raw memory upfront directly from the OS. It uses a Doubly Linked List paired with hidden `BlockHeader` metadata to manually track, partition, and distribute smaller pieces of this memory arena to the application. This approach effectively bypasses slow OS runtime calls, minimizes overhead, and optimizes memory utilization for custom data structures. It supports three ways of finding free space (First-Fit, Best-Fit, and Worst-Fit), splits large blocks when needed, merges neighboring free blocks when memory is freed, overloads C++ `new` and `delete` operators, supports thread safety, calculates fragmentation percentages, and compares its speed against standard operating system calls.

## Problem Statement

General-purpose Operating System memory allocators are inherently designed to handle requests of any size safely, which introduces significant runtime performance overhead. Over time, this generalized approach leads to severe memory fragmentation, creating unusable memory holes (external fragmentation) and wasting space within allocated blocks (internal fragmentation).

## Objectives

- Demonstrate custom allocation from a pre-allocated memory pool.
- Implement First-Fit, Best-Fit, and Worst-Fit block-selection policies.
- Represent the pool as a doubly linked list of block headers.
- Split a free block when it is large enough to hold the requested allocation and a new header.
- Merge a freed block with adjacent free blocks where possible.
- Intercept standard C++ memory requests via class-specific `operator new` and `operator delete` overloading (`DataBlock`).
- Ensure thread safety across all memory operations using `std::mutex` and `std::lock_guard`.
- Let the user select the displayed size unit: bytes (B), kilobytes (KB), megabytes (MB), or gigabytes (GB).
- Display active block IDs, block-by-block memory maps, and side-by-side strategy comparisons.
- Measure and compare execution speed against standard OS allocation using an STL `std::chrono` benchmarking suite.
- Summarize used memory, free memory, total pool size, and block count.

## Key Concepts

### Memory pool

The program reserves a contiguous region of memory when each `MemoryAllocator` object is constructed. 

### Block header

Each memory block begins with a `BlockHeader` structure:

```cpp
struct BlockHeader
{
    size_t size;
    size_t requestedSize;
    bool isFree;
    BlockHeader *next;
    BlockHeader *prev;
};
```

- `size`: size of the block's payload in bytes, excluding its header.
- `isFree`: indicates whether the block is available for allocation.
- `requestedSize`: exact number of bytes requested by the caller, used to calculate internal fragmentation.
- `next`: points to the next block in the linked list.
- `prev`: points to the previous block in the linked list.

The pointer returned to the caller points immediately after the block header, so the header is kept separate from the payload.

### Splitting

If the selected free block is large enough to hold the request plus another `BlockHeader` and at least one byte of remaining payload, the allocator splits it into an allocated block and a new free block. If there is not enough room for a new header and payload, the allocator uses the selected block as a whole.

### Coalescing

When a block is deallocated, the allocator marks it free and attempts to merge it with a free next block and then a free previous block. This coalescing can reduce external fragmentation by combining neighbouring free regions.

### Fragmentation Analytics

Fragmentation occurs when available memory is divided into pieces that cannot be efficiently utilized:

- **Internal Fragmentation:** Calculated from the unused space inside allocated blocks when a block cannot be split further:
  $$\text{Internal Fragmentation (\%)} = \frac{\text{Total Used Bytes} - \text{Total Requested Bytes}}{\text{Total Used Bytes}} \times 100$$
- **External Fragmentation:** Calculated by comparing the largest single contiguous free block against the total free memory available:
  $$\text{External Fragmentation (\%)} = \frac{\text{Total Free Bytes} - \text{Largest Free Block}}{\text{Total Free Bytes}} \times 100$$

A request fails when one or more of the three allocator instances cannot find a single contiguous free block large enough to satisfy it.

### Operator Overloading & Thread Safety

- **Operator Overloading:** The `DataBlock` struct overloads `operator new` and `operator delete` to route memory requests directly to `globalAllocator->allocate()` and `globalAllocator->deallocate()` using the active `globalStrategy`.
- **Thread Safety:** All core operations (`allocate`, `deallocate`, `getPointerFromBlockId`, `printActiveBlockIds`, `printMemoryMap`, and `printSummaryTable`) acquire a `std::lock_guard<std::mutex>` on `arenaMutex` to prevent race conditions and metadata corruption in concurrent environments.

## Allocation Strategies

### 1. First-Fit

First-Fit traverses the block list from the beginning and chooses the **first free block whose payload size is at least the requested size**.

**Advantages**
- Simple to implement.
- Stops searching as soon as a suitable block is found.

**Trade-offs**
- The chosen block depends on the order of blocks in the list.
- Repeated allocations can leave small free blocks in portions of the pool.

### 2. Best-Fit

Best-Fit scans the list and chooses the **smallest free block that can satisfy the request**.

**Advantages**
- Attempts to leave the smallest remainder among the blocks that can satisfy a request.

**Trade-offs**
- It scans the full block list to identify the smallest suitable block.
- It can create small leftover blocks that may be difficult to reuse.

### 3. Worst-Fit

Worst-Fit scans the list and chooses the **largest free block that can satisfy the request**.

**Advantages**
- Uses the largest available region for the current request.
- May leave a sizeable remainder in that selected region.

**Trade-offs**
- It scans the full block list.
- Its results depend on the sequence and sizes of allocation requests; it is not guaranteed to reduce fragmentation.

No strategy is universally best. Their outcomes depend on the allocation workload and the resulting memory layout.

## Major Features and Modules

### 1. Memory-pool initialization

Each allocator reserves its own memory region using `::operator new(totalSize)`. The initial region is represented by one free block whose payload size is the pool size minus the first header.

### 2. Allocation engine

`MemoryAllocator::allocate()` searches for a suitable free block according to the selected `AllocationStrategy`. It returns `nullptr` when no suitable block is found and otherwise returns a pointer to the block's payload.

### 3. Deallocation engine

`MemoryAllocator::deallocate()` marks the corresponding block as free and attempts to coalesce adjacent free blocks. The menu uses the selected primary strategy to identify which allocation the user wants to release, then releases the matching allocation in all three allocator instances.

### 4. Operator Overloading Layer 

`DataBlock` overrides `operator new` and `operator delete` to transparently route allocations and deallocations to the active `MemoryAllocator` instance without exposing internal pool calls.

### 5. Allocation record tracking

The `AllocationRecord` structure stores the three pointers associated with one successful, synchronized request and the original requested value:

```cpp
struct AllocationRecord {
    void* firstFitPtr;
    void* bestFitPtr;
    void* worstFitPtr;
    void* osPtr;
    double requestedValue;
};
```

This allows the program to keep the three simulations in step: a request is retained only if all three allocators succeed. If any allocator fails, any successful allocations from that request are released and the request is reported as failed.

### 6. Unit conversion

The helper functions `convertToBytes()` and `convertFromBytes()` convert values between bytes and KB, MB, and GB. The program accepts `B`, `KB`, `MB`, and `GB` as the display/input unit. Unit conversion uses powers of 1024.

### 7. Active Block IDs & Memory-Map Display

- `printActiveBlockIds()` lists block IDs that are currently allocated in the primary allocator (numbered starting at 1 based on current linked-list order).
- `printMemoryMap()` renders an ASCII map displaying each block's ID, state (`FREE` or `USED`), and payload size in the selected unit, along with block count and used/free totals.

### 8. Strategy Comparison

The comparison option prints the memory maps for First-Fit, Best-Fit, and Worst-Fit sequentially so their layouts can be inspected and compared side by side.

### 9. STL Performance Benchmarking Suite

Using `<chrono>` (`std::chrono::high_resolution_clock`), the benchmark compares the time (in microseconds) required to allocate and free 10,000 blocks via:
- **Standard OS Allocation:** `new StandardNode()` and `delete`
- **Custom Allocator Engine:** `DataBlock::operator new(1)` and `DataBlock::operator delete(...)` on the primary allocator

### 10. Summary Table with Fragmentation Analytics

`printSummaryTable()` outputs a formatted ASCII table showing Total Pool Size, Total Used Memory, Total Free Memory, Total Blocks, Internal Fragmentation (%), and External Fragmentation (%) for the primary allocator.

## Technologies / Tools Used
* **Programming Language:** C++ (Standard C++11 or higher)
* **Core Concepts:** Doubly Linked Lists, Pointer Arithmetic (byte-level casting), Class Encapsulation, Operator Overloading, Thread Safety , High-Resolution Timing.
* **Tools / Frameworks:** Standard C++ Library / STL (for internal arrays and benchmarking)
* **IDE:** VS Code / Code::Blocks
* **Version Control:** Git & GitHub

## Requirements

- A C++ compiler supporting the language features used in the source.
- A terminal or command prompt.
- The source file `main.cpp`.

On Linux, GCC is a straightforward option. On Windows, a GCC-based toolchain such as MinGW-w64 or a compatible C++ development environment can be used. On macOS, Clang is commonly available through the developer tools.

## Project Setup / Installation Instructions

### 1. Clone the Repository

```bash
git clone https://github.com/kushagra016/Custom-Memory-Allocator
cd Custom-Memory-Allocator
```

### 2. Compile the Code

Make sure you have a C++11 compiler (such as `g++`) installed:

```bash
# On Windows (MinGW)
g++ main.cpp allocator.cpp utils.cpp -o allocator -std=c++11

# On Linux / macOS
g++ main.cpp allocator.cpp utils.cpp -o allocator -std=c++11 -pthread
```

### 3. Run the Program

```bash
./allocator
```

### 4. How to Use the Program

When the program starts, it sets up three 10 MB memory pools (one each for First-Fit, Best-Fit, and Worst-Fit) and asks you to choose:

* Your preferred **size unit** (`B`, `KB`, `MB`, or `GB`)
* Your **main strategy** (`1. First-Fit`, `2. Best-Fit`, or `3. Worst-Fit`)

You can then use the menu options:

1. **Allocate Memory:** Allocates the size you enter across all three strategies using `DataBlock::operator new`, and also makes a matching OS allocation (`::operator new`) to measure the time taken in nanoseconds.
2. **Deallocate Memory:** Shows the active Block IDs, frees the block you pick across all three strategies using `DataBlock::operator delete`, frees the matching OS block, and merges neighboring free blocks.
3. **Display Memory Map:** Shows the current layout of your chosen strategy with Block IDs, status (`FREE` or `USED`), and block sizes.
4. **Compare All Strategies:** Shows the memory maps of First-Fit, Best-Fit, and Worst-Fit together so you can compare them directly.
5. **Run Session Performance Benchmark:** Shows the total time (in nanoseconds) taken by your allocations and deallocations so far, comparing your custom allocator with standard OS calls.
6. **Exit and Show Summary:** Frees any remaining OS blocks and prints a summary table with total memory, used memory, free memory, block count, and fragmentation percentages.

## Current Project Status

- [x] Three independent memory pools.
- [x] First-Fit allocation.
- [x] Best-Fit allocation.
- [x] Worst-Fit allocation.
- [x] Block splitting.
- [x] Deallocation with adjacent-block coalescing.
- [x] Input and display units: B, KB, MB, and GB.
- [x] Active block ID display.
- [x] Memory map for the selected strategy.
- [x] Side-by-side textual comparison of the three strategies.
- [x] Summary table for the selected strategy.
- [x] Operator Overloading. 
- [x] Performance Benchmarking.
- [x] Fragmentation Analytics.
- [x] Multi-threading Support.


## Team Members

| No. | Name | Role / Contribution |
|---:|---|---|
| 1 | `Kushagra Katiyar` | Core Architect |
| 2 | `Manya Gupta` | Allocation Developer |
| 3 | `Soumya Chauhan` | Deallocation Developer |
| 4 | `Arsh` | Testing & UI Developer |
