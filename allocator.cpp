#include "allocator.h"
#include "Utils.h"
#include <iostream>
#include <iomanip>

MemoryAllocator::MemoryAllocator(size_t size, std::string name) {
    totalSize = size;
    strategyName = name;
    memoryPool = ::operator new(totalSize);
    head = static_cast<BlockHeader*>(memoryPool);

    head->size = totalSize - sizeof(BlockHeader);
    head->requestedSize = 0;
    head->isFree = true;
    head->next = nullptr;
    head->prev = nullptr;

    firstFreeBlock = head;
}

MemoryAllocator::~MemoryAllocator() {
    ::operator delete(memoryPool);
}

void* MemoryAllocator::allocate(size_t size, AllocationStrategy strategy) {
    std::lock_guard<std::mutex> lock(arenaMutex);

    if (size == 0) return nullptr;

    BlockHeader* current = (strategy == AllocationStrategy::FIRST_FIT) ? firstFreeBlock : head;
    BlockHeader* selectedBlock = nullptr;

    if (strategy == AllocationStrategy::FIRST_FIT) {
        while (current != nullptr) {
            if (current->isFree && current->size >= size) {
                selectedBlock = current;
                break;
            }
            current = current->next;
        }
    }
    else if (strategy == AllocationStrategy::BEST_FIT) {
        while (current != nullptr) {
            if (current->isFree && current->size >= size) {
                if (selectedBlock == nullptr || current->size < selectedBlock->size) {
                    selectedBlock = current;
                }
            }
            current = current->next;
        }
    }
    else if (strategy == AllocationStrategy::WORST_FIT) {
        while (current != nullptr) {
            if (current->isFree && current->size >= size) {
                if (selectedBlock == nullptr || current->size > selectedBlock->size) {
                    selectedBlock = current;
                }
            }
            current = current->next;
        }
    }

    if (selectedBlock == nullptr) {
        return nullptr;
    }

    if (selectedBlock->size >= size + sizeof(BlockHeader) + 1) {
        char* newBlockAddress = reinterpret_cast<char*>(selectedBlock) + sizeof(BlockHeader) + size;
        BlockHeader* newBlock = reinterpret_cast<BlockHeader*>(newBlockAddress);

        newBlock->size = selectedBlock->size - size - sizeof(BlockHeader);
        newBlock->requestedSize = 0;
        newBlock->isFree = true;
        newBlock->next = selectedBlock->next;
        newBlock->prev = selectedBlock;

        if (newBlock->next != nullptr) {
            newBlock->next->prev = newBlock;
        }

        selectedBlock->size = size;
        selectedBlock->next = newBlock;
    }

    selectedBlock->isFree = false;
    selectedBlock->requestedSize = size;

    if (selectedBlock == firstFreeBlock) {
        BlockHeader* temp = firstFreeBlock->next;
        while (temp != nullptr && !temp->isFree) {
            temp = temp->next;
        }
        firstFreeBlock = temp;
    }

    return static_cast<void*>(selectedBlock + 1);
}

void MemoryAllocator::deallocate(void* ptr) {
    if (ptr == nullptr) return;

    std::lock_guard<std::mutex> lock(arenaMutex);

    BlockHeader* current = static_cast<BlockHeader*>(ptr) - 1;
    current->isFree = true;
    current->requestedSize = 0;

    if (current->next != nullptr && current->next->isFree) {
        current->size += sizeof(BlockHeader) + current->next->size;
        current->next = current->next->next;
        if (current->next != nullptr) {
            current->next->prev = current;
        }
    }

    if (current->prev != nullptr && current->prev->isFree) {
        current->prev->size += sizeof(BlockHeader) + current->size;
        current->prev->next = current->next;
        if (current->next != nullptr) {
            current->next->prev = current->prev;
        }
        current = current->prev;
    }

    if (firstFreeBlock == nullptr || current < firstFreeBlock) {
        firstFreeBlock = current;
    }
}

void* MemoryAllocator::getPointerFromBlockId(int targetId) {
    std::lock_guard<std::mutex> lock(arenaMutex);
    BlockHeader* current = head;
    int currentId = 1;
    while (current != nullptr) {
        if (currentId == targetId) {
            if (current->isFree) return nullptr;
            return static_cast<void*>(current + 1);
        }
        currentId++;
        current = current->next;
    }
    return nullptr;
}

void MemoryAllocator::printActiveBlockIds() {
    std::lock_guard<std::mutex> lock(arenaMutex);
    BlockHeader* current = head;
    int id = 1;
    bool hasActive = false;
    std::cout << "Currently active Block IDs: ";

    while (current != nullptr) {
        if (!current->isFree) {
            std::cout << id << " ";
            hasActive = true;
        }
        id++;
        current = current->next;
    }

    if (!hasActive) std::cout << "None";
    std::cout << "\n";
}

void MemoryAllocator::printMemoryMap(std::string unit) {
    std::lock_guard<std::mutex> lock(arenaMutex);
    BlockHeader* current = head;
    std::cout << "\n=== " << strategyName << " Map (" << unit << ") ===\n";

    int id = 1;
    size_t totalFreeBytes = 0;
    size_t totalUsedBytes = 0;

    while (current != nullptr) {
        std::cout << "[ID: " << id << " | ";
        if (current->isFree) {
            std::cout << "FREE";
            totalFreeBytes += current->size;
        } else {
            std::cout << "USED";
            totalUsedBytes += current->size;
        }

        double displaySize = convertFromBytes(current->size, unit);
        std::cout << ": " << std::fixed << std::setprecision(2) << displaySize << " " << unit << "] ";

        if (current->next != nullptr) {
            std::cout << "-> ";
        }

        id++;
        current = current->next;
    }

    std::cout << "\n\nBlocks: " << (id - 1)
              << " | Used: " << std::fixed << std::setprecision(2) << convertFromBytes(totalUsedBytes, unit) << " " << unit
              << " | Free: " << std::fixed << std::setprecision(2) << convertFromBytes(totalFreeBytes, unit) << " " << unit << "\n";
}

void MemoryAllocator::printSummaryTable(std::string unit) {
    std::lock_guard<std::mutex> lock(arenaMutex);
    BlockHeader* current = head;
    int blockCount = 0;
    size_t totalFreeBytes = 0;
    size_t totalUsedBytes = 0;

    size_t totalRequestedBytes = 0;
    size_t largestFreeBlock = 0;

    while (current != nullptr) {
        if (current->isFree) {
            totalFreeBytes += current->size;
            if (current->size > largestFreeBlock) {
                largestFreeBlock = current->size;
            }
        } else {
            totalUsedBytes += current->size;
            totalRequestedBytes += current->requestedSize;
        }
        blockCount++;
        current = current->next;
    }

    size_t internalWaste = totalUsedBytes - totalRequestedBytes;
    double internalFragPercent = (totalUsedBytes > 0) ? (static_cast<double>(internalWaste) / totalUsedBytes) * 100.0 : 0.0;
    double externalFragPercent = (totalFreeBytes > 0) ? (static_cast<double>(totalFreeBytes - largestFreeBlock) / totalFreeBytes) * 100.0 : 0.0;

    std::cout << "+-------------------------------------------------+\n";
    std::cout << "| " << std::left << std::setw(47) << (strategyName + " SUMMARY") << " |\n";
    std::cout << "+-------------------------------------------------+\n";
    std::cout << std::fixed << std::setprecision(2);

    std::cout << "| Total Pool Size:       "
              << std::right << std::setw(17) << convertFromBytes(totalSize, unit)
              << " " << std::left << std::setw(7) << unit << "|\n";

    std::cout << "| Total Used Memory:     "
              << std::right << std::setw(17) << convertFromBytes(totalUsedBytes, unit)
              << " " << std::left << std::setw(7) << unit << "|\n";

    std::cout << "| Total Free Memory:     "
              << std::right << std::setw(17) << convertFromBytes(totalFreeBytes, unit)
              << " " << std::left << std::setw(7) << unit << "|\n";

    std::cout << "| Total Blocks:          "
              << std::right << std::setw(17) << blockCount
              << " " << std::left << std::setw(7) << " " << "|\n";

    std::cout << "+-------------------------------------------------+\n";
    std::cout << "| Internal Fragmentation:"
              << std::right << std::setw(17) << internalFragPercent
              << " " << std::left << std::setw(7) << "%" << "|\n";

    std::cout << "| External Fragmentation:"
              << std::right << std::setw(17) << externalFragPercent
              << " " << std::left << std::setw(7) << "%" << "|\n";
    std::cout << "+-------------------------------------------------+\n\n";
}