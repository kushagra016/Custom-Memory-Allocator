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
    
    //Allocate Method
    void* allocate(size_t size){
        if (size == 0) return nullptr;

        BlockHeader* current = head;

        while (current != nullptr){
            if (current->isFree && current->size >= size){
                if (current->size >= size + sizeof(BlockHeader) + 1){
                    char* newBlockAddress = reinterpret_cast<char*>(current) + sizeof(BlockHeader) + size;
                    BlockHeader* newBlock = reinterpret_cast <BlockHeader*>(newBlockAddress);

                    newBlock->size = current->size - size - sizeof(BlockHeader);
                    newBlock->isFree = true;

                    newBlock->next = current->next;
                    newBlock->prev = current;

                    if (newBlock->next != nullptr){
                        newBlock->next->prev = newBlock;
                    }

                    current->size = size;
                    current->next = newBlock;
                }

                current->isFree = false;
                return static_cast<void*>(current + 1);
            }

            current = current->next;
        }

        std::cout << "Allocation Failed: Out of memory or fragmentation is too high.\n";
        return nullptr;
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