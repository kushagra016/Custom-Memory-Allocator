#include <iostream>
#include <cstddef>
#include <string>

//BlockHeader
struct BlockHeader {
    size_t size;
    bool isFree;
    BlockHeader* next;
    BlockHeader* prev;
};

enum class AllocationStrategy {
    FIRST_FIT,
    BEST_FIT,
    WORST_FIT
};

//Memory Allocator Class
class MemoryAllocator {
private:
    void* memoryPool;
    size_t totalSize;
    BlockHeader* head;

public:
    //Constructor to allocate massive chunks of memory upfront
    MemoryAllocator(size_t size){
        totalSize = size;

        memoryPool = ::operator new(totalSize);

        head = static_cast<BlockHeader*>(memoryPool);

        head->size = totalSize - sizeof(BlockHeader);
        head->isFree = true;
        head->next = nullptr;
        head->prev = nullptr;

        std::cout << "Allocated Memory Pool of " << totalSize << " bytes.\n";
        std::cout << "Overhead per block (Header Size): " << sizeof(BlockHeader) << " bytes.\n";
    }

    //Deconstructor to return the massive chunk to OS
    ~MemoryAllocator() {
        ::operator delete(memoryPool);
        std::cout << "Memory Pool destroyed.\n";
    }
    
    //Allocation Method
    void *allocate(size_t size, AllocationStrategy strategy = AllocationStrategy::FIRST_FIT) {
        if (size == 0) {
            return nullptr;
        }

        BlockHeader *current = head;
        BlockHeader *selectedBlock = nullptr;

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
            std::cout << "Allocation Failed: Out of memory or fragmentation is too high.\n";
            return nullptr;
        }

        //Coalescing Logic
        if (selectedBlock->size >= size + sizeof(BlockHeader) + 1) {

            char *newBlockAddress = reinterpret_cast<char *>(selectedBlock) + sizeof(BlockHeader) + size;
            BlockHeader *newBlock = reinterpret_cast<BlockHeader *>(newBlockAddress);

            newBlock->size = selectedBlock->size - size - sizeof(BlockHeader);
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

        return static_cast<void *>(selectedBlock + 1);
    }
    
    //Deallocate Method
    void deallocate(void* ptr){
        if (ptr == nullptr) return;

        BlockHeader* current = static_cast<BlockHeader*>(ptr) - 1;

        current->isFree = true;

        if (current->next != nullptr && current->next->isFree){
            current->size += sizeof(BlockHeader) + current->next->size;

            current->next = current->next->next;

            if (current->next != nullptr){
                current->next->prev = current;
            }
        }

        if (current->prev != nullptr && current->prev->isFree){
            current->prev->size += sizeof(BlockHeader) + current->size;

            current->prev->next = current->next;

            if (current->next != nullptr){
                current->next->prev = current->prev;
            }
        }
    }

    //MemoryMap Method
    void printMemoryMap(){
        BlockHeader* current = head;
        std:: cout << "\n=== Current Memory Map ===\n";

        int blockCount = 0;
        size_t totalFree = 0;
        size_t totalUsed = 0;

        while (current != nullptr){
            std::cout << "[";
            if (current->isFree){
                std::cout << "FREE";
                totalFree += current->size;
            }
            else {
                std::cout << "USED";
                totalUsed += current->size;
            }
            std::cout << ": " << current->size << "B] ";

            if (current->next != nullptr){
                std::cout << "->";
            }

            blockCount++;
            current = current->next;
        }

        std::cout << "\nBlocks: " << blockCount
                  << " | Used Data: " << totalUsed << "B"
                  << " | Free Space: " << totalFree << "B\n";
        std::cout << "===========================\n";
    }

};

int main() {
    MemoryAllocator allocator(1024);
    allocator.printMemoryMap();

    std::cout << "\n>>> ACTION: Allocating 3 chunks of memory...\n";
    void* ptr1 = allocator.allocate(100);
    void* ptr2 = allocator.allocate(200);
    void* ptr3 = allocator.allocate(50);
    allocator.printMemoryMap();

    std::cout << "\n>>> ACTION: Freeing middle chunk (ptr2)...\n";
    allocator.deallocate(ptr2);
    allocator.printMemoryMap();

    std::cout << "\n>>> ACTION: Freeing first chunk (ptr1)...\n";
    allocator.deallocate(ptr1);
    allocator.printMemoryMap();

    std::cout << "\n>>> ACTION: Freeing last chunk (ptr3)...\n";
    allocator.deallocate(ptr3);
    allocator.printMemoryMap();
    
    return 0;
}