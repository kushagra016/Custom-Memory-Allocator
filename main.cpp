#include <iostream>
#include <cstddef>

//BlockHeader
struct BlockHeader {
    size_t size;
    bool isFree;
    BlockHeader* next;
    BlockHeader* prev;
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
    
    void* allocate(size_t size){
        //Allocator logic
    }

    void deallocate(void* ptr){
        //Deallocator logic
    }

    void printMemoryMap(){
        //Visualizer logic
    }
};

int main() {
    MemoryAllocator myAllocator(1048576); //Allocating 1 MegaByte
    
    return 0;
}