#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <cctype>

#include "utils.h"
#include "allocator.h"

bool suppressOutput = false;
MemoryAllocator* globalAllocator = nullptr;
AllocationStrategy globalStrategy = AllocationStrategy::FIRST_FIT;

struct DataBlock {
    void* operator new(size_t size) {
        return globalAllocator->allocate(size, globalStrategy);
    }
    
    void operator delete(void* ptr) {
        globalAllocator->deallocate(ptr);
    }
};

struct StandardNode {  
    char byte;
    StandardNode() {}
    ~StandardNode() {}
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
    
    for (char &c : globalUnit) c = std::toupper(c);
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
    while (choice != 6) {
        std::cout << "\n--- Custom Memory Allocator Menu ---\n";
        std::cout << "1. Allocate Memory\n";
        std::cout << "2. Deallocate Memory\n";
        std::cout << "3. Display Memory Map\n";
        std::cout << "4. Compare All Strategies\n";
        std::cout << "5. Run STL Performance Benchmark\n";
        std::cout << "6. Exit and Show Summary\n";
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
                
                globalAllocator = &allocFirst;
                globalStrategy = AllocationStrategy::FIRST_FIT;
                void* ptrF = DataBlock::operator new(requestedBytes);
                
                globalAllocator = &allocBest;
                globalStrategy = AllocationStrategy::BEST_FIT;
                void* ptrB = DataBlock::operator new(requestedBytes);
                
                globalAllocator = &allocWorst;
                globalStrategy = AllocationStrategy::WORST_FIT;
                void* ptrW = DataBlock::operator new(requestedBytes);
                
                if (ptrF && ptrB && ptrW) {
                    allocatedPointers.push_back({ptrF, ptrB, ptrW, val});
                    std::cout << "Successfully allocated " << val << " " << globalUnit << ".\n";
                } else {
                    std::cout << "Allocation failed due to fragmentation.\n";
                    if (ptrF) { globalAllocator = &allocFirst; DataBlock::operator delete(ptrF); }
                    if (ptrB) { globalAllocator = &allocBest; DataBlock::operator delete(ptrB); }
                    if (ptrW) { globalAllocator = &allocWorst; DataBlock::operator delete(ptrW); }
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
                        globalAllocator = &allocFirst;
                        DataBlock::operator delete(it->firstFitPtr);
                        
                        globalAllocator = &allocBest;
                        DataBlock::operator delete(it->bestFitPtr);
                        
                        globalAllocator = &allocWorst;
                        DataBlock::operator delete(it->worstFitPtr);
                        
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
            case 5: {              // 
                std::cout << "\n--- STL Benchmarking Suite ---\n";
                const int NUM_NODES = 10000;
                std::cout << "Allocating and freeing " << NUM_NODES << " blocks...\n";

                auto startOS = std::chrono::high_resolution_clock::now();
                std::vector<StandardNode*> stdNodes(NUM_NODES);
                
                for (int i = 0; i < NUM_NODES; i++) {
                    stdNodes[i] = new StandardNode();
                }
                for (int i = 0; i < NUM_NODES; i++) {
                    delete stdNodes[i];
                }
                
                auto endOS = std::chrono::high_resolution_clock::now();
                auto durationOS = std::chrono::duration_cast<std::chrono::microseconds>(endOS - startOS).count();

                globalAllocator = primaryAlloc;
                globalStrategy = (stratChoice == 1) ? AllocationStrategy::FIRST_FIT : 
                                 (stratChoice == 2) ? AllocationStrategy::BEST_FIT : AllocationStrategy::WORST_FIT;
                
                suppressOutput = true; 

                auto startCustom = std::chrono::high_resolution_clock::now();
                std::vector<DataBlock*> customNodes(NUM_NODES);
                
                for (int i = 0; i < NUM_NODES; i++) {
                    customNodes[i] = static_cast<DataBlock*>(DataBlock::operator new(1));
                }
                for (int i = 0; i < NUM_NODES; i++) {
                    DataBlock::operator delete(customNodes[i]);
                }
                
                auto endCustom = std::chrono::high_resolution_clock::now();
                auto durationCustom = std::chrono::duration_cast<std::chrono::microseconds>(endCustom - startCustom).count();

                suppressOutput = false; 

                std::cout << "\n[RESULTS]\n";
                std::cout << "Standard OS (new/delete): " << durationOS << " microseconds\n";
                std::cout << "Custom Allocator Engine:  " << durationCustom << " microseconds\n\n";
                
                if (durationCustom < durationOS) {
                    std::cout << ">>> SUCCESS: Custom Allocator is faster by " 
                              << (durationOS - durationCustom) << " microseconds!\n";
                } else {
                    std::cout << ">>> NOTE: OS was faster on this run.\n";
                }
                break;
            }
            case 6:
                std::cout << "\nExiting System. Generating Final Summary...\n\n";
                primaryAlloc->printSummaryTable(globalUnit);
                break;
            default:
                std::cout << "Invalid choice. Please select an option from 1 to 5.\n";
        }
    }

    return 0;
}