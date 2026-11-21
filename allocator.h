#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <cstddef>
#include <string>
#include <mutex>
#include "json.hpp"

using json = nlohmann::json;

struct BlockHeader {
    int uniqueId;
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

    void* allocate(size_t size, AllocationStrategy strategy, int newId = 0);
    void deallocate(void* ptr);
    void* getPointerFromBlockId(int targetId);
    void printActiveBlockIds();
    void printMemoryMap(std::string unit);
    void printSummaryTable(std::string unit);
    void* getPointerByUniqueId(int targetId);
    json getMemoryMapJson(const std::string& unit);
    json getAnalyticsJson();
};

#endif