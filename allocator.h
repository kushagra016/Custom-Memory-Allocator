#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <cstddef>
#include <string>
#include <mutex>

struct BlockHeader {
    size_t size;
    size_t requestedSize;
    bool isFree;
    BlockHeader* next;
    BlockHeader* prev;
};

enum class AllocationStrategy {
    FIRST_FIT,
    BEST_FIT,
    WORST_FIT
};

class MemoryAllocator {
private:
    void* memoryPool;
    size_t totalSize;
    BlockHeader* head;
    BlockHeader* firstFreeBlock;
    std::string strategyName;
    std::mutex arenaMutex;

public:
    MemoryAllocator(size_t size, std::string name);
    ~MemoryAllocator();

    void* allocate(size_t size, AllocationStrategy strategy);
    void deallocate(void* ptr);
    void* getPointerFromBlockId(int targetId);
    void printActiveBlockIds();
    void printMemoryMap(std::string unit);
    void printSummaryTable(std::string unit);
};

#endif