#include <iostream>
#include <cstddef>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>

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

size_t convertToBytes(double value, std::string unit) {
    if (unit == "KB") return static_cast<size_t>(value * 1024);
    if (unit == "MB") return static_cast<size_t>(value * 1024 * 1024);
    if (unit == "GB") return static_cast<size_t>(value * 1024 * 1024 * 1024);
    return static_cast<size_t>(value);
}

double convertFromBytes(size_t bytes, std::string unit) {
    if (unit == "KB") return static_cast<double>(bytes) / 1024.0;
    if (unit == "MB") return static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (unit == "GB") return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    return static_cast<double>(bytes);
}

struct AllocationRecord {
    void* firstFitPtr;
    void* bestFitPtr;
    void* worstFitPtr;
    double requestedValue;
};

//Memory Allocator Class
class MemoryAllocator {
private:
    void* memoryPool;
    size_t totalSize;
    BlockHeader* head;
    std::string strategyName;

public:
    //Constructor to allocate massive chunks of memory upfront
    MemoryAllocator(size_t size, std::string name){
        totalSize = size;
        strategyName = name;
        memoryPool = ::operator new(totalSize);
        head = static_cast<BlockHeader*>(memoryPool);

        head->size = totalSize - sizeof(BlockHeader);
        head->isFree = true;
        head->next = nullptr;
        head->prev = nullptr;
    }

    //Deconstructor to return the massive chunk to OS
    ~MemoryAllocator() {
        ::operator delete(memoryPool);
        std::cout << "Memory Pool destroyed.\n";
    }
    
    //Allocation Method
    void *allocate(size_t size, AllocationStrategy strategy) {
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

    void* getPointerFromBlockId(int targetId) {
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

    void printActiveBlockIds() {
        BlockHeader *current = head;
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

    //MemoryMap Method
    void printMemoryMap(std::string unit) {
        BlockHeader *current = head;
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

    
    //Summary Table Method
    void printSummaryTable(std::string unit) {
        BlockHeader *current = head;
        int blockCount = 0;
        size_t totalFreeBytes = 0;
        size_t totalUsedBytes = 0;

        while (current != nullptr) {
            if (current->isFree) {
                totalFreeBytes += current->size;
            } else {
                totalUsedBytes += current->size;
            }
            blockCount++;
            current = current->next;
        }

        std::cout << "+-------------------------------------------------+\n";
        std::cout << "| " << std::left << std::setw(47) << (strategyName + " SUMMARY") << " |\n";
        std::cout << "+-------------------------------------------------+\n";
        std::cout << std::fixed << std::setprecision(2);
        
        std::cout << "| Total Pool Size:       " 
                  << std::right << std::setw(17) << convertFromBytes(totalSize, unit) 
                  << " " << std::left << std::setw(7) << unit 
                  << std::right << "|\n";
                  
        std::cout << "| Total Used Memory:     " 
                  << std::right << std::setw(17) << convertFromBytes(totalUsedBytes, unit) 
                  << " " << std::left << std::setw(7) << unit 
                  << std::right << "|\n";
                  
        std::cout << "| Total Free Memory:     " 
                  << std::right << std::setw(17) << convertFromBytes(totalFreeBytes, unit) 
                  << " " << std::left << std::setw(7) << unit 
                  << std::right << "|\n";
                  
        std::cout << "| Total Blocks:          " 
                  << std::right << std::setw(17) << blockCount 
                  << " " << std::left << std::setw(7) << " " 
                  << std::right << "|\n";
                  
        std::cout << "+-------------------------------------------------+\n\n";
    }
};

int main() {
    std::cout << "Initializing Custom Memory Allocator...\n";
    
    size_t poolSize = 10 * 1024 * 1024;
    MemoryAllocator allocFirst(poolSize, "FIRST-FIT STRATEGY");
    MemoryAllocator allocBest(poolSize, "BEST-FIT STRATEGY");
    MemoryAllocator allocWorst(poolSize, "WORST-FIT STRATEGY");

    std::string globalUnit;
    std::cout << "\nBefore we start, select your preferred operating unit (B, KB, MB, GB): ";
    std::cin >> globalUnit;
    
    for (char &c : globalUnit) c = toupper(c);
    if (globalUnit != "B" && globalUnit != "KB" && globalUnit != "MB" && globalUnit != "GB") {
        globalUnit = "B";
    }

    int stratChoice = 0;
    std::cout << "Select your primary Allocation Strategy:\n";
    std::cout << "1. First-Fit\n2. Best-Fit\n3. Worst-Fit\n";
    std::cout << "Enter choice (1-3): ";
    std::cin >> stratChoice;

    MemoryAllocator* primaryAlloc = &allocFirst;
    if (stratChoice == 2) primaryAlloc = &allocBest;
    else if (stratChoice == 3) primaryAlloc = &allocWorst;
    
    std::cout << "\n-------------------------------------------------\n";
    std::cout << "Allocated Memory Pool of " << std::fixed << std::setprecision(2) 
              << convertFromBytes(poolSize, globalUnit) << " " << globalUnit 
              << " upfront from the OS.\n";
    std::cout << "-------------------------------------------------\n";

    std::vector<AllocationRecord> allocatedPointers; 

    int choice = 0;
    while (choice != 5) {
        std::cout << "\n--- Custom Memory Allocator Menu ---\n";
        std::cout << "1. Allocate Memory\n";
        std::cout << "2. Deallocate Memory\n";
        std::cout << "3. Display Memory Map\n";
        std::cout << "4. Compare All Strategies\n";
        std::cout << "5. Exit and Show Summary\n";
        std::cout << "Enter your choice: ";
        
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            continue;
        }

        switch (choice) {
            case 1: {
                double val;
                std::cout << "Enter amount to allocate (in " << globalUnit << "): ";
                std::cin >> val;

                size_t requestedBytes = convertToBytes(val, globalUnit);
                
                void* ptrF = allocFirst.allocate(requestedBytes, AllocationStrategy::FIRST_FIT);
                void* ptrB = allocBest.allocate(requestedBytes, AllocationStrategy::BEST_FIT);
                void* ptrW = allocWorst.allocate(requestedBytes, AllocationStrategy::WORST_FIT);
                
                if (ptrF && ptrB && ptrW) {
                    allocatedPointers.push_back({ptrF, ptrB, ptrW, val});
                    std::cout << "Successfully allocated " << val << " " << globalUnit << ".\n";
                } else {
                    std::cout << "Allocation failed due to fragmentation.\n";
                    if (ptrF) allocFirst.deallocate(ptrF);
                    if (ptrB) allocBest.deallocate(ptrB);
                    if (ptrW) allocWorst.deallocate(ptrW);
                }
                break;
            }
            case 2: {
                if (allocatedPointers.empty()) {
                    std::cout << "No blocks currently allocated to free.\n";
                    break;
                }
                
                std::cout << "\n";
                primaryAlloc->printActiveBlockIds();
                
                std::cout << "Enter the Block ID to deallocate: ";
                int id;
                std::cin >> id;

                void* primaryPtr = primaryAlloc->getPointerFromBlockId(id);
                
                if (primaryPtr) {
                    auto it = std::find_if(allocatedPointers.begin(), allocatedPointers.end(),
                        [&](const AllocationRecord& rec) {
                            if (stratChoice == 1) return rec.firstFitPtr == primaryPtr;
                            if (stratChoice == 2) return rec.bestFitPtr == primaryPtr;
                            return rec.worstFitPtr == primaryPtr;
                        });

                    if (it != allocatedPointers.end()) {
                        allocFirst.deallocate(it->firstFitPtr);
                        allocBest.deallocate(it->bestFitPtr);
                        allocWorst.deallocate(it->worstFitPtr);
                        
                        allocatedPointers.erase(it);
                        std::cout << "\nBlock " << id << " successfully deallocated.\n";
                    }
                } else {
                    std::cout << "Invalid Block ID or Block is already FREE.\n";
                }
                break;
            }
            case 3:
                primaryAlloc->printMemoryMap(globalUnit);
                break;
            case 4:
                std::cout << "\n========================================================\n";
                std::cout << "             STRATEGY COMPARISON VISUALIZER               \n";
                std::cout << "========================================================";
                allocFirst.printMemoryMap(globalUnit);
                std::cout << "\n--------------------------------------------------------";
                allocBest.printMemoryMap(globalUnit);
                std::cout << "\n--------------------------------------------------------";
                allocWorst.printMemoryMap(globalUnit);
                std::cout << "\n========================================================\n";
                break;
            case 5:
                std::cout << "\nExiting System. Generating Final Summary...\n\n";
                primaryAlloc->printSummaryTable(globalUnit);
                break;
            default:
                std::cout << "Invalid choice. Please select an option from 1 to 5.\n";
        }
    }

    return 0;
}